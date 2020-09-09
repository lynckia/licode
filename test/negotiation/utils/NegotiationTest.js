const path = require('path');
const puppeteer = require('puppeteer-core');
const expect = require('chai').expect;

const BrowserInstaller = require('./BrowserInstaller');
const ClientStream = require('./ClientStream');
const ClientConnection = require('./ClientConnection');
const ErizoConnection = require('./ErizoConnection');
const SdpChecker = require('./SdpUtils');

let browser;

before(async function() {
  this.timeout(30000);
  if (!BrowserInstaller.installed) {
    this.timeout(300000);
    await BrowserInstaller.install('canary');
  }
  browser = await puppeteer.launch({
    headless: true,
    executablePath: BrowserInstaller.revisionInfo.executablePath,
    args: [
      '--use-fake-ui-for-media-stream',
      '--use-fake-device-for-media-stream',
    ]
  });
  const internalPage = await browser.newPage();
  await internalPage.goto(`chrome://webrtc-internals`);
});

after(async function() {
   await browser.close();
});


const describeNegotiationTest = function(title, test) {
  describe(title, function() {
    this.timeout(50000);

    const ctx = {
      clientStreams: {},
      erizoStreams: {},
      expectedClientSdpVersion: 2,
      expectedErizoSdpVersion: 0,
    };

    let page;
    const erizoConns = {};

    ctx.getFirstClientStream = function() {
      const streamIds = Object.keys(ctx.clientStreams);
      if (streamIds.length > 0) {
        return ctx.clientStreams[streamIds[0]];
      }
      return;
    };

    ctx.getFirstErizoStream = function() {
      const streamIds = Object.keys(ctx.erizoStreams);
      if (streamIds.length > 0) {
        return ctx.erizoStreams[streamIds[0]];
      }
      return;
    };

    ctx.createClientStream = async function() {
      const stream = new ClientStream(page);
      ctx.clientStreams[stream.id] = stream;
      await stream.init();
      await stream.waitForAccepted();
      await stream.getLabel();
      return stream;
    };

    ctx.deleteClientStream = async function(stream) {
      await ctx.clientStreams[stream.id].remove();
      delete ctx.clientStreams[stream.id];
    };

    ctx.createErizoStream = async function() {
      const id = parseInt(Math.random() * 1000, 0);
      const stream = { id, label: id.toString(), audio: true, video: true, addedToConnection: false };
      ctx.erizoStreams[id] = stream;
      return stream;
    };

    ctx.deleteErizoStream = async function(erizoStream) {
      delete ctx.erizoStreams[erizoStream.id];
    };

    ctx.createClientConnection = async function() {
      const connectionId = parseInt(Math.random() * 1000, 0);
      const conn = new ClientConnection(page, connectionId);
      ctx.client = conn;
      await conn.init();
      return conn;
    };

    ctx.createErizoConnection = async function() {
      const connectionId = parseInt(Math.random() * 1000, 0);
      const conn = new ErizoConnection(connectionId);
      await conn.init();
      ctx.erizo = conn;
      return conn;
    };

    after(async function() {
      await page.close();
      ctx.erizo.removeAllStreams();
      ctx.erizo.close();
    });

    before(async function() {
      const currentProcessPath = process.cwd();
      const htmlPath = path.join(currentProcessPath, '../../extras/basic_example/public/index.html');
      page = await browser.newPage();
      await page.goto(`file://${htmlPath}?forceStart=1`);
    });

    before(async function() {
      await ctx.createClientConnection();
      await ctx.createErizoConnection();
    });

    ctx.checkClientSentCorrectOffer = (getOffer) => {
      it('Client should send the correct offer', function() {
        const sdp = new SdpChecker(getOffer());
        sdp.expectType('offer');
        sdp.expectVersion(ctx.expectedClientSdpVersion++);
        if (ctx.expectedClientSdpVersion > 3) {
          sdp.expectToIncludeCandidates();
        } else {
          sdp.doNotExpectToIncludeCandidates();
        }
        sdp.expectToHaveStreams(ctx.clientStreams);
      });
    };

    ctx.checkErizoSentCorrectOffer = (getOffer) => {
      it('Erizo should send the correct offer', function() {
        const sdp = new SdpChecker(getOffer());
        sdp.expectType('offer');
        sdp.expectVersion(ctx.expectedErizoSdpVersion++);
        sdp.expectToIncludeCandidates();
        sdp.expectToHaveStreams(ctx.erizoStreams);
      });
    };

    ctx.checkClientSentCorrectAnswer = (getAnswer) => {
      it('Client should send the correct answer', function() {
        const sdp = new SdpChecker(getAnswer());
        sdp.expectType('answer');
        sdp.expectVersion(ctx.expectedClientSdpVersion++);
        sdp.expectToHaveStreams(ctx.clientStreams);
      });
    };

    ctx.checkErizoSentCorrectAnswer = (getAnswer) => {
      it('Erizo should send an answer', function() {
        const sdp = new SdpChecker(getAnswer());
        sdp.expectType('answer');
        sdp.expectVersion(ctx.expectedErizoSdpVersion++);
        sdp.expectToIncludeCandidates();
        sdp.expectToHaveStreams(ctx.erizoStreams);
      });
    };

    ctx.checkClientSentCandidates = () => {
      it('Client should send candidates', function() {
        for (const candidate of ctx.candidates) {
          const sdp = new SdpChecker(candidate);
          sdp.expectType('candidate');
        }
      });
    };

    ctx.checkErizoFinishedSuccessfully = () => {
      it('Erizo should finish successfully', function() {
        expect(ctx.erizo.isReady).to.be.true;
      });
    };

    ctx.checkClientFSMStateIsStable = () => {
      it('Client FSM state should be stable', async function() {
        const state = await ctx.client.getFSMState();
        expect(state).to.be.equals('stable');
      });
    };

    ctx.checkClientHasNotFailed = () => {
      it('Client should not be failed', async function() {
        const connectionFailed = await ctx.client.isConnectionFailed();
        expect(connectionFailed).to.be.false;
      });
    };

    ctx.checkClientDroppedOffer = (getOffer) => {
      it('Client should drop erizo offer', function() {
        expect(getOffer()).to.have.property('type', 'offer-dropped');
      });
    };

    ctx.publishToErizoStreamStep = function() {
      describe('Publish One Client Stream', function() {
        let offer, answer, candidates, erizoMessages, clientStream, label;
        before(async function() {
          clientStream = await ctx.createClientStream();

          await ctx.client.addStream(clientStream);
          await ctx.erizo.publishStream(clientStream);

          offer = await ctx.client.getSignalingMessage();

          await ctx.erizo.processSignalingMessage(offer);
          answer = ctx.erizo.getSignalingMessage();

          await ctx.client.processSignalingMessage(answer);

          if (!ctx.candidates) {
            await ctx.client.waitForCandidates();
            ctx.candidates = await ctx.client.getAllCandidates();
            for (const candidate of ctx.candidates) {
              await ctx.erizo.processSignalingMessage(candidate);
            }
          }

          await ctx.client.waitForConnected();
          await ctx.erizo.waitForReadyMessage();
          erizoMessages = ctx.erizo.getSignalingMessages();
        });

        ctx.checkClientSentCorrectOffer(() => offer);
        ctx.checkErizoSentCorrectAnswer(() => answer);
        ctx.checkClientSentCandidates();
        ctx.checkErizoFinishedSuccessfully();
        ctx.checkClientFSMStateIsStable();
        ctx.checkClientHasNotFailed();
      });
    };

    ctx.unpublishStreamStep = function() {
      describe('Unpublish One Client Stream', function() {
        let offer, answer, candidates, erizoMessages, clientStream, label;
        before(async function() {
          clientStream = ctx.getFirstClientStream();

          await ctx.client.removeStream(clientStream);
          await ctx.erizo.unpublishStream(clientStream);

          offer = await ctx.client.getSignalingMessage();

          await ctx.erizo.processSignalingMessage(offer);
          answer = ctx.erizo.getSignalingMessage();

          await ctx.client.processSignalingMessage(answer);

          if (!ctx.candidates) {
            await ctx.client.waitForCandidates();
            ctx.candidates = await ctx.client.getAllCandidates();
            for (const candidate of ctx.candidates) {
              await ctx.erizo.processSignalingMessage(candidate);
            }
          }

          await ctx.client.waitForConnected();
          await ctx.erizo.waitForReadyMessage();
          erizoMessages = ctx.erizo.getSignalingMessages();

         await ctx.deleteClientStream(clientStream);
        });

        ctx.checkClientSentCorrectOffer(() => offer);
        ctx.checkErizoSentCorrectAnswer(() => answer);
        ctx.checkClientSentCandidates();
        ctx.checkErizoFinishedSuccessfully();
        ctx.checkClientFSMStateIsStable();
        ctx.checkClientHasNotFailed();
      });
    };

    ctx.subscribeToErizoStreamStep = function() {
      describe('Subscribe to One Erizo Stream', function() {
        let offer, answer, erizoStream;

        before(async function() {
          erizoStream = await ctx.createErizoStream();

          await ctx.erizo.subscribeStream(erizoStream);
          offer = await ctx.erizo.createOffer();

          await ctx.client.processSignalingMessage(offer);
          answer = await ctx.client.getSignalingMessage();

          await ctx.erizo.processSignalingMessage(answer);
          if (!ctx.candidates) {
            await ctx.client.waitForCandidates();
            ctx.candidates = await ctx.client.getAllCandidates();
            for (const candidate of ctx.candidates) {
              await ctx.erizo.processSignalingMessage(candidate);
            }
          }

          await ctx.client.waitForConnected();
          await ctx.erizo.waitForReadyMessage();
          erizoMessages = ctx.erizo.getSignalingMessages();
        });

        ctx.checkErizoSentCorrectOffer(() => offer);
        ctx.checkClientSentCorrectAnswer(() => answer);
        ctx.checkClientSentCandidates();
        ctx.checkErizoFinishedSuccessfully();
        ctx.checkClientFSMStateIsStable();
        ctx.checkClientHasNotFailed();
      });
    };

    ctx.unsubscribeStreamStep = function() {
      describe('Unsubscribe One Erizo Stream', function() {
        let offer, answer, erizoStream;

        before(async function() {
          erizoStream = ctx.getFirstErizoStream();

          await ctx.erizo.unsubscribeStream(erizoStream);
          offer = await ctx.erizo.createOffer();

          await ctx.client.processSignalingMessage(offer);
          answer = await ctx.client.getSignalingMessage();

          await ctx.erizo.processSignalingMessage(answer);
          if (!ctx.candidates) {
            await ctx.client.waitForCandidates();
            ctx.candidates = await ctx.client.getAllCandidates();
            for (const candidate of ctx.candidates) {
              await ctx.erizo.processSignalingMessage(candidate);
            }
          }

          await ctx.client.waitForConnected();
          await ctx.erizo.waitForReadyMessage();
          erizoMessages = ctx.erizo.getSignalingMessages();
          await ctx.deleteErizoStream(erizoStream);
        });

        ctx.checkErizoSentCorrectOffer(() => offer);
        ctx.checkClientSentCorrectAnswer(() => answer);
        ctx.checkClientSentCandidates();
        ctx.checkErizoFinishedSuccessfully();
        ctx.checkClientFSMStateIsStable();
        ctx.checkClientHasNotFailed();
      });
    };

    ctx.publishAndSubscribeStreamsStep = function(steps) {
      describe('Publish and Subscribe Streams', function() {
        let erizoOffer, clientOffer, erizoAnswer, erizoMessages, clientStream, erizoStream, clientOfferDropped, retransmittedErizoOffer, retransmittedClientAnswer;

        before(async function() {
          clientStream = await ctx.createClientStream();
          erizoStream = await ctx.createErizoStream();

          for (const step of steps) {
            switch (step) {
              case 'client-add-stream':
                await ctx.client.addStream(clientStream);
                break;
              case 'erizo-publish-stream':
                await ctx.erizo.publishStream(clientStream);
                break;
              case 'erizo-subscribe-stream':
                await ctx.erizo.subscribeStream(erizoStream);
                break;
              case 'erizo-get-offer':
                erizoOffer = await ctx.erizo.createOffer();
                break;
              case 'client-get-offer':
                await ctx.client.waitForSignalingMessage();
                clientOffer = await ctx.client.getSignalingMessage();
                break;
              case 'erizo-process-offer':
                await ctx.erizo.processSignalingMessage(clientOffer);
                await ctx.erizo.subscribeStream(erizoStream);
                break;
              case 'client-process-offer':
                await ctx.client.processSignalingMessage(erizoOffer);
                break;
              case 'erizo-get-answer':
                await ctx.erizo.waitForSignalingMessage();
                erizoAnswer = await ctx.erizo.getSignalingMessage();
                break;
              case 'client-process-answer':
                await ctx.client.processSignalingMessage(erizoAnswer);
                break;
              case 'client-get-offer-dropped':
                clientOfferDropped = await ctx.client.getSignalingMessage();
                break;
              case 'get-and-process-candidates':
                if (!ctx.candidates) {
                  await ctx.client.waitForCandidates();
                  ctx.candidates = await ctx.client.getAllCandidates();
                  for (const candidate of ctx.candidates) {
                    ctx.erizo.processSignalingMessage(candidate);
                  }
                }
                break;
              case 'wait-for-being-connected':
                await ctx.erizo.waitForReadyMessage();
                await ctx.client.waitForConnected();
                erizoMessages = await ctx.erizo.getSignalingMessages();
                break;
              case 'erizo-process-offer-dropped':
                await ctx.erizo.processSignalingMessage(clientOfferDropped);
                break;
              case 'erizo-get-rtx-offer':
                retransmittedErizoOffer = await ctx.erizo.getSignalingMessage();
                await ctx.erizo.subscribeStream(erizoStream);
                break;
              case 'client-process-rtx-offer':
                await ctx.client.processSignalingMessage(retransmittedErizoOffer);
                break;
              case 'client-get-rtx-answer':
                await ctx.client.waitForSignalingMessage();
                retransmittedClientAnswer = await ctx.client.getSignalingMessage();
                break;
              case 'erizo-process-rtx-answer':
                await ctx.erizo.processSignalingMessage(retransmittedClientAnswer);
                break;
              case '':
                await ctx.erizo.subscribeStream(erizoStream);
                break;
              default:
                break;

            }
          }
       });

        ctx.checkErizoSentCorrectOffer(() => erizoOffer);
        ctx.checkClientSentCorrectOffer(() => clientOffer);
        ctx.checkErizoSentCorrectAnswer(() => erizoAnswer);
        ctx.checkClientDroppedOffer(() => clientOfferDropped);
        ctx.checkErizoSentCorrectOffer(() => retransmittedErizoOffer);
        ctx.checkClientSentCorrectAnswer(() => retransmittedClientAnswer);

        ctx.checkClientSentCandidates();
        ctx.checkErizoFinishedSuccessfully();
        ctx.checkClientFSMStateIsStable();
        ctx.checkClientHasNotFailed();
      });
    };

    test.call(this, ctx);
  });
};



module.exports = describeNegotiationTest;
