const path = require('path');
const puppeteer = require('puppeteer-core');
const expect = require('chai').expect;

const BrowserInstaller = require('./BrowserInstaller');
const ClientStream = require('./ClientStream');
const ClientConnection = require('./ClientConnection');
const ErizoConnection = require('./ErizoConnection');
const SdpChecker = require('./SdpUtils');

let browser;
let currentErizoStreamId = 10;

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


const describeNegotiationTest = function(title, test, only = false) {
  const describeImpl = only ? describe.only : describe;
  describeImpl(title, function() {
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
      const id = parseInt(currentErizoStreamId++, 0);
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
      ctx.erizo = conn;
      return conn;
    };

    after(async function() {
      await page.close();
      await ctx.erizo.removeAllStreams();
      ctx.erizo.close();
    });

    before(async function() {
      const currentProcessPath = process.cwd();
      const htmlPath = path.join(currentProcessPath, '../../extras/basic_example/public/index.html');
      page = await browser.newPage();

      // page.on('console', msg => console.log('PAGE LOG:', msg.text()));
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
        sdp.expectToHaveStreams(ctx.clientStreams);
      });
    };

    ctx.checkErizoSentCorrectOffer = (getOffer) => {
      it('Erizo should send the correct offer', function() {
        const sdp = new SdpChecker(getOffer());
        sdp.expectType('offer');
        sdp.expectToIncludeCandidates();
        sdp.expectToHaveStreams(ctx.erizoStreams);
      });
    };

    ctx.checkClientSentCorrectAnswer = (getAnswer) => {
      it('Client should send the correct answer', function() {
        const sdp = new SdpChecker(getAnswer());
        sdp.expectType('answer');
        sdp.expectToHaveStreams(ctx.clientStreams);
      });
    };

    ctx.checkErizoSentCorrectAnswer = (getAnswer) => {
      it('Erizo should send an answer', function() {
        const sdp = new SdpChecker(getAnswer());
        sdp.expectType('answer');
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
      describe('Publish One Client Stream', async function() {
        let offer, answer, candidates, erizoMessages, clientStream, label;
        before(async function() {
          clientStream = await ctx.createClientStream();

          await ctx.client.addStream(clientStream);
          await ctx.erizo.publishStream(clientStream);

          await ctx.client.setLocalDescription();
          offer = { type: 'offer', sdp: await ctx.client.getLocalDescription() };
          await ctx.erizo.setRemoteDescription(offer);
          await ctx.erizo.setLocalDescription();
          answer = { type: 'answer', sdp:  await ctx.erizo.getLocalDescription()};
          await ctx.client.setRemoteDescription(answer);
          if (!ctx.candidates) {
            await ctx.client.waitForCandidates();
            ctx.candidates = await ctx.client.getAllCandidates();
            for (const candidate of ctx.candidates) {
              await ctx.erizo.addIceCandidate(candidate);
            }
          }
          await ctx.client.waitForConnected();
          await ctx.erizo.waitForReadyMessage();
        });

        ctx.checkClientSentCorrectOffer(() => offer);
        ctx.checkErizoSentCorrectAnswer(() => answer);
        ctx.checkClientSentCandidates();
        ctx.checkErizoFinishedSuccessfully();
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

          await ctx.client.setLocalDescription();
          offer = { type: 'offer', sdp: await ctx.client.getLocalDescription() };

          await ctx.erizo.setRemoteDescription(offer);
          answer = { type: 'answer', sdp: await ctx.erizo.getLocalDescription() };

          await ctx.client.setRemoteDescription(answer);

          if (!ctx.candidates) {
            await ctx.client.waitForCandidates();
            ctx.candidates = await ctx.client.getAllCandidates();
            for (const candidate of ctx.candidates) {
              await ctx.erizo.addIceCandidate(candidate);
            }
          }

          await ctx.client.waitForConnected();
          await ctx.erizo.waitForReadyMessage();

         await ctx.deleteClientStream(clientStream);
        });

        ctx.checkClientSentCorrectOffer(() => offer);
        ctx.checkErizoSentCorrectAnswer(() => answer);
        ctx.checkClientSentCandidates();
        ctx.checkErizoFinishedSuccessfully();
        ctx.checkClientHasNotFailed();
      });
    };

    ctx.subscribeToErizoStreamStep = function() {
      describe('Subscribe to One Erizo Stream', function() {
        let offer, answer, erizoStream;

        before(async function() {
          erizoStream = await ctx.createErizoStream();

          const negotiationNeeded = ctx.erizo.onceNegotiationIsNeeded();
          await ctx.erizo.subscribeStream(erizoStream);
          await negotiationNeeded;

          await ctx.erizo.setLocalDescription();
          offer = { type: 'offer', sdp: await ctx.erizo.getLocalDescription() };
          await ctx.client.setRemoteDescription(offer);
          await ctx.client.setLocalDescription();
          answer = { type: 'answer', sdp: await ctx.client.getLocalDescription() };
          await ctx.erizo.setRemoteDescription(answer);
          if (!ctx.candidates) {
            await ctx.client.waitForCandidates();
            ctx.candidates = await ctx.client.getAllCandidates();
            for (const candidate of ctx.candidates) {
              await ctx.erizo.addIceCandidate(candidate);
            }
          }

          await ctx.client.waitForConnected();
          await ctx.erizo.waitForReadyMessage();
          // await ctx.erizo.sleep(1000 * 60 * 5);
        });

        ctx.checkErizoSentCorrectOffer(() => offer);
        ctx.checkClientSentCorrectAnswer(() => answer);
        ctx.checkClientSentCandidates();
        ctx.checkErizoFinishedSuccessfully();
        ctx.checkClientHasNotFailed();
      });
    };

    ctx.unsubscribeStreamStep = function() {
      describe('Unsubscribe One Erizo Stream', function() {
        let offer, answer, erizoStream;

        before(async function() {
          erizoStream = ctx.getFirstErizoStream();

          const negotiationNeeded = ctx.erizo.onceNegotiationIsNeeded();
          await ctx.erizo.unsubscribeStream(erizoStream);
          await negotiationNeeded;
          await ctx.erizo.setLocalDescription();
          offer = { type: 'offer', sdp: await ctx.erizo.getLocalDescription() };

          await ctx.client.setRemoteDescription(offer);
          answer = { type: 'answer', sdp: await ctx.client.getLocalDescription() };

          await ctx.erizo.setRemoteDescription(answer);
          if (!ctx.candidates) {
            await ctx.client.waitForCandidates();
            ctx.candidates = await ctx.client.getAllCandidates();
            for (const candidate of ctx.candidates) {
              await ctx.erizo.addIceCandidate(candidate);
            }
          }

          await ctx.client.waitForConnected();
          await ctx.erizo.waitForReadyMessage();
          await ctx.deleteErizoStream(erizoStream);
        });

        ctx.checkErizoSentCorrectOffer(() => offer);
        ctx.checkClientSentCorrectAnswer(() => answer);
        ctx.checkClientSentCandidates();
        ctx.checkErizoFinishedSuccessfully();
        ctx.checkClientHasNotFailed();
      });
    };

    ctx.publishAndSubscribeStreamsStep = function(steps) {
      describe('Publish and Subscribe Streams', function() {
        let erizoOffer, clientOffer, erizoAnswer, clientAnswer, clientStream, erizoStream, clientOfferDropped, retransmittedErizoOffer, retransmittedClientAnswer;

        before(async function() {
          clientStream = await ctx.createClientStream();
          erizoStream = await ctx.createErizoStream();
          let processOfferPromise, addStreamPromise;

          for (const step of steps) {
            switch (step) {
              case 'client-add-stream':
                await ctx.client.addStream(clientStream);
                break;
              case 'client-add-stream-and-process-erizo-offer':
                addStreamPromise = ctx.client.addStream(clientStream);
                processOfferPromise = ctx.client.setRemoteDescription(erizoOffer);
                await addStreamPromise;
                await processOfferPromise;
                break;
              case 'client-get-offer-and-process-erizo-offer':
                processOfferPromise = ctx.client.setRemoteDescription(erizoOffer);
                clientOfferPromise = ctx.client.setLocalDescription();
                await clientOfferPromise;
                clientOffer = { type: 'offer', sdp: await ctx.client.getLocalDescription() };
                break;
              case 'erizo-publish-stream':
                await ctx.erizo.publishStream(clientStream);
                break;
              case 'erizo-subscribe-stream':
                await ctx.erizo.subscribeStream(erizoStream);
                break;
              case 'erizo-get-offer':
                await ctx.erizo.setLocalDescription();
                erizoOffer = { type: 'offer', sdp: await ctx.erizo.getLocalDescription() };
                break;
              case 'erizo-get-answer':
                await ctx.erizo.setLocalDescription();
                erizoAnswer = { type: 'answer', sdp: await ctx.erizo.getLocalDescription() };
                break;
              case 'client-get-offer':
                await ctx.client.setLocalDescription();
                clientOffer = { type: 'offer', sdp: await ctx.client.getLocalDescription() };
                break;
              case 'client-get-answer':
                await ctx.client.setLocalDescription();
                clientAnswer = { type: 'answer', sdp: await ctx.client.getLocalDescription() };
                break;
              case 'erizo-process-offer':
                await ctx.erizo.setRemoteDescription(clientOffer);
                break;
              case 'erizo-process-answer':
                await ctx.erizo.setRemoteDescription(clientAnswer);
                break;
              case 'client-process-offer':
                await ctx.client.setRemoteDescription(erizoOffer);
                break;

              case 'client-process-answer':
                await ctx.client.setRemoteDescription(erizoAnswer);
                break;
              case 'get-and-process-candidates':
                if (!ctx.candidates) {
                  await ctx.client.waitForCandidates();
                  ctx.candidates = await ctx.client.getAllCandidates();
                  for (const candidate of ctx.candidates) {
                    await ctx.erizo.addIceCandidate(candidate);
                  }
                }
                break;
              case 'wait-for-being-connected':
                await ctx.erizo.waitForStartedMessage();
                await ctx.client.waitForConnected();
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

        ctx.checkClientSentCandidates();
        ctx.checkErizoFinishedSuccessfully();
        ctx.checkClientHasNotFailed();
      });
    };

    test.call(this, ctx);
  });
};



module.exports = describeNegotiationTest;
