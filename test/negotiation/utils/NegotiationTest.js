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
      erizo: {},
      client: {},
      candidates: {},
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

    ctx.createClientConnection = async function(idx) {
      const connectionId = parseInt(Math.random() * 1000, 0);
      const conn = new ClientConnection(page, connectionId);
      ctx.client[idx] = conn;
      await conn.init();
      return conn;
    };

    ctx.createErizoConnection = async function(idx) {
      const connectionId = parseInt(Math.random() * 1000, 0);
      const conn = new ErizoConnection(connectionId);
      ctx.erizo[idx] = conn;
      return conn;
    };

    after(async function() {
      await page.close();
      await ctx.erizo['pub'].removeAllStreams();
      ctx.erizo['pub'].close();
    });

    before(async function() {
      const currentProcessPath = process.cwd();
      const htmlPath = path.join(currentProcessPath, '../../extras/basic_example/public/index.html');
      page = await browser.newPage();

      // page.on('console', msg => console.log('PAGE LOG:', msg.text()));
      await page.goto(`file://${htmlPath}?forceStart=1`);
    });

    before(async function() {
      await ctx.createClientConnection('pub');
      await ctx.createErizoConnection('pub');
      await ctx.createClientConnection('sub');
      await ctx.createErizoConnection('sub');
    });

    ctx.checkClientSentCorrectOffer = (getOffer, isPubConnection) => {
      it('Client should send the correct offer', function() {
        const sdp = new SdpChecker(getOffer());
        sdp.expectType('offer');
        if (isPubConnection) {
          sdp.expectToHaveStreams(ctx.clientStreams);
        }
      });
    };

    ctx.checkErizoSentCorrectOffer = (getOffer, isPubConnection) => {
      it('Erizo should send the correct offer', function() {
        const sdp = new SdpChecker(getOffer());
        sdp.expectType('offer');
        sdp.expectToIncludeCandidates();
        if (!isPubConnection) {
          sdp.expectToHaveStreams(ctx.erizoStreams);
        }
      });
    };

    ctx.checkClientSentCorrectAnswer = (getAnswer, isPubConnection) => {
      it('Client should send the correct answer', function() {
        const sdp = new SdpChecker(getAnswer());
        sdp.expectType('answer');
        if (isPubConnection) {
          sdp.expectToHaveStreams(ctx.clientStreams);
        }
      });
    };

    ctx.checkErizoSentCorrectAnswer = (getAnswer, isPubConnection) => {
      it('Erizo should send an answer', function() {
        const sdp = new SdpChecker(getAnswer());
        sdp.expectType('answer');
        sdp.expectToIncludeCandidates();
        if (!isPubConnection) {
          sdp.expectToHaveStreams(ctx.erizoStreams);
        }
      });
    };

    ctx.checkClientSentCandidates = (idx) => {
      it('Client should send candidates', function() {
        for (const candidate of ctx.candidates[idx]) {
          const sdp = new SdpChecker(candidate);
          sdp.expectType('candidate');
        }
      });
    };

    ctx.checkErizoFinishedSuccessfully = (idx) => {
      it('Erizo should finish successfully', function() {
        expect(ctx.erizo[idx].isReady).to.be.true;
        // expect(ctx.erizo['sub'].isReady).to.be.true;
      });
    };

    ctx.checkClientHasNotFailed = (idx) => {
      it('Client should not be failed', async function() {
        const connectionFailed = await ctx.client[idx].isConnectionFailed();
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

          await ctx.client['pub'].addStream(clientStream);
          await ctx.erizo['pub'].publishStream(clientStream);

          await ctx.client['pub'].setLocalDescription();
          offer = { type: 'offer', sdp: await ctx.client['pub'].getLocalDescription() };
          await ctx.erizo['pub'].setRemoteDescription(offer);
          await ctx.erizo['pub'].createAnswer();
          answer = { type: 'answer', sdp:  await ctx.erizo['pub'].getLocalDescription()};
          await ctx.client['pub'].setRemoteDescription(answer);
          if (!ctx.candidates['pub']) {
            await ctx.client['pub'].waitForCandidates();
            ctx.candidates['pub'] = await ctx.client['pub'].getAllCandidates();
            for (const candidate of ctx.candidates['pub']) {
              await ctx.erizo['pub'].addIceCandidate(candidate);
            }
          }
          await ctx.client['pub'].waitForConnected();
          await ctx.erizo['pub'].waitForReadyMessage();
        });

        ctx.checkClientSentCorrectOffer(() => offer, true);
        ctx.checkErizoSentCorrectAnswer(() => answer, true);
        ctx.checkClientSentCandidates('pub');
        ctx.checkErizoFinishedSuccessfully('pub');
        ctx.checkClientHasNotFailed('pub');
      });
    };

    ctx.unpublishStreamStep = function() {
      describe('Unpublish One Client Stream', function() {
        let offer, answer, candidates, erizoMessages, clientStream, label;
        before(async function() {
          clientStream = ctx.getFirstClientStream();

          await ctx.client['pub'].removeStream(clientStream);
          await ctx.erizo['pub'].unpublishStream(clientStream);

          await ctx.client['pub'].setLocalDescription();
          offer = { type: 'offer', sdp: await ctx.client['pub'].getLocalDescription() };
          await ctx.erizo['pub'].setRemoteDescription(offer);
          await ctx.erizo['pub'].createAnswer();
          answer = { type: 'answer', sdp: await ctx.erizo['pub'].getLocalDescription() };
          await ctx.client['pub'].setRemoteDescription(answer);

          if (!ctx.candidates['pub']) {
            await ctx.client['pub'].waitForCandidates();
            ctx.candidates['pub'] = await ctx.client['pub'].getAllCandidates();
            for (const candidate of ctx.candidates['pub']) {
              await ctx.erizo['pub'].addIceCandidate(candidate);
            }
          }

          await ctx.client['pub'].waitForConnected();
          await ctx.erizo['pub'].waitForReadyMessage();

         await ctx.deleteClientStream(clientStream);
        });

        ctx.checkClientSentCorrectOffer(() => offer, true);
        ctx.checkErizoSentCorrectAnswer(() => answer, true);
        ctx.checkClientSentCandidates('pub');
        ctx.checkErizoFinishedSuccessfully('pub');
        ctx.checkClientHasNotFailed('pub');
      });
    };

    ctx.subscribeToErizoStreamStep = function() {
      describe('Subscribe to One Erizo Stream', function() {
        let offer, answer, erizoStream;

        before(async function() {
          erizoStream = await ctx.createErizoStream();

          const negotiationNeeded = ctx.erizo['sub'].onceNegotiationIsNeeded();
          await ctx.erizo['sub'].subscribeStream(erizoStream);
          await negotiationNeeded;

          await ctx.erizo['sub'].setLocalDescription();
          offer = { type: 'offer', sdp: await ctx.erizo['sub'].getLocalDescription() };
          await ctx.client['sub'].setRemoteDescription(offer);
          await ctx.client['sub'].setLocalDescription();
          answer = { type: 'answer', sdp: await ctx.client['sub'].getLocalDescription() };
          await ctx.erizo['sub'].setRemoteDescription(answer);
          if (!ctx.candidates['sub']) {
            await ctx.client['sub'].waitForCandidates();
            ctx.candidates['sub'] = await ctx.client['sub'].getAllCandidates();
            for (const candidate of ctx.candidates['sub']) {
              await ctx.erizo['sub'].addIceCandidate(candidate);
            }
          }

          await ctx.client['sub'].waitForConnected();
          await ctx.erizo['sub'].waitForReadyMessage();
          // await ctx.erizo['sub'].sleep(1000 * 60 * 5);
        });

        ctx.checkErizoSentCorrectOffer(() => offer, false);
        ctx.checkClientSentCorrectAnswer(() => answer, false);
        ctx.checkClientSentCandidates('sub');
        ctx.checkErizoFinishedSuccessfully('sub');
        ctx.checkClientHasNotFailed('sub');
      });
    };

    ctx.unsubscribeStreamStep = function() {
      describe('Unsubscribe One Erizo Stream', function() {
        let offer, answer, erizoStream;

        before(async function() {
          erizoStream = ctx.getFirstErizoStream();

          const negotiationNeeded = ctx.erizo['sub'].onceNegotiationIsNeeded();
          await ctx.erizo['sub'].unsubscribeStream(erizoStream);
          await negotiationNeeded;
          await ctx.erizo['sub'].setLocalDescription();
          offer = { type: 'offer', sdp: await ctx.erizo['sub'].getLocalDescription() };

          await ctx.client['sub'].setRemoteDescription(offer);
          answer = { type: 'answer', sdp: await ctx.client['sub'].getLocalDescription() };

          await ctx.erizo['sub'].setRemoteDescription(answer);
          if (!ctx.candidates['sub']) {
            await ctx.client['sub'].waitForCandidates();
            ctx.candidates['sub'] = await ctx.client['sub'].getAllCandidates();
            for (const candidate of ctx.candidates['sub']) {
              await ctx.erizo['sub'].addIceCandidate(candidate);
            }
          }

          await ctx.client['sub'].waitForConnected();
          await ctx.erizo['sub'].waitForReadyMessage();
          await ctx.deleteErizoStream(erizoStream);
        });

        ctx.checkErizoSentCorrectOffer(() => offer, false);
        ctx.checkClientSentCorrectAnswer(() => answer, false);
        ctx.checkClientSentCandidates('sub');
        ctx.checkErizoFinishedSuccessfully('sub');
        ctx.checkClientHasNotFailed('sub');
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
                await ctx.client['pub'].addStream(clientStream);
                break;
              case 'client-add-stream-and-process-erizo-offer':
                addStreamPromise = ctx.client['pub'].addStream(clientStream);
                processOfferPromise = ctx.client['sub'].setRemoteDescription(erizoOffer);
                await addStreamPromise;
                await processOfferPromise;
                break;
              case 'client-get-offer-and-process-erizo-offer':
                processOfferPromise = ctx.client['sub'].setRemoteDescription(erizoOffer);
                clientOfferPromise = ctx.client['pub'].setLocalDescription();
                await clientOfferPromise;
                clientOffer = { type: 'offer', sdp: await ctx.client['pub'].getLocalDescription() };
                break;
              case 'erizo-publish-stream':
                await ctx.erizo['pub'].publishStream(clientStream);
                break;
              case 'erizo-subscribe-stream':
                await ctx.erizo['sub'].subscribeStream(erizoStream);
                break;
              case 'erizo-get-offer':
                await ctx.erizo['sub'].setLocalDescription();
                erizoOffer = { type: 'offer', sdp: await ctx.erizo['sub'].getLocalDescription() };
                break;
              case 'erizo-get-answer':
                await ctx.erizo['pub'].setLocalDescription();
                erizoAnswer = { type: 'answer', sdp: await ctx.erizo['pub'].getLocalDescription() };
                break;
              case 'client-get-offer':
                await ctx.client['pub'].setLocalDescription();
                clientOffer = { type: 'offer', sdp: await ctx.client['pub'].getLocalDescription() };
                break;
              case 'client-get-answer':
                await ctx.client['sub'].setLocalDescription();
                clientAnswer = { type: 'answer', sdp: await ctx.client['sub'].getLocalDescription() };
                break;
              case 'erizo-process-offer':
                await ctx.erizo['pub'].setRemoteDescription(clientOffer);
                break;
              case 'erizo-process-answer':
                await ctx.erizo['sub'].setRemoteDescription(clientAnswer);
                break;
              case 'client-process-offer':
                await ctx.client['sub'].setRemoteDescription(erizoOffer);
                break;

              case 'client-process-answer':
                await ctx.client['pub'].setRemoteDescription(erizoAnswer);
                break;
              case 'get-and-process-candidates':
                if (!ctx.candidates['pub'] && ctx.client['pub']) {
                  await ctx.client['pub'].waitForCandidates();
                  ctx.candidates['pub'] = await ctx.client['pub'].getAllCandidates();
                  for (const candidate of ctx.candidates['pub']) {
                    await ctx.erizo['pub'].addIceCandidate(candidate);
                  }
                }

                if (!ctx.candidates['sub'] && ctx.client['sub']) {
                  await ctx.client['sub'].waitForCandidates();
                  ctx.candidates['sub'] = await ctx.client['sub'].getAllCandidates();
                  for (const candidate of ctx.candidates['sub']) {
                    await ctx.erizo['sub'].addIceCandidate(candidate);
                  }
                }
                break;
              case 'wait-for-being-connected':
                await ctx.erizo['pub'].waitForStartedMessage();
                await ctx.client['pub'].waitForConnected();
                await ctx.erizo['sub'].waitForStartedMessage();
                await ctx.client['sub'].waitForConnected();
                break;
              case '':
                await ctx.erizo['sub'].subscribeStream(erizoStream);
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
