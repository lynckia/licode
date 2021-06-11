const path = require('path');
const puppeteer = require('puppeteer-core');
const expect = require('chai').expect;

const BrowserInstaller = require('./BrowserInstaller');
const ClientStream = require('./ClientStream');
const ClientConnection = require('./ClientConnection');
const ErizoConnection = require('./ErizoConnection');
const SdpChecker = require('./SdpUtils');

let browser, browser2;
let currentErizoStreamId = 10;

before(async function() {
  this.timeout(30000);
  if (!BrowserInstaller.installed) {
    this.timeout(300000);
    await BrowserInstaller.install('canary');
  }
  browser = await puppeteer.launch({
    headless: false,
    dumpio: false,
    executablePath: BrowserInstaller.revisionInfo.executablePath,
    args: [
      '--use-fake-ui-for-media-stream',
      '--use-fake-device-for-media-stream',
      '-enable-logging',
      '--v=1',
      '--vmodule=*/webrtc/*=2,*=-1',
      '--enable-logging=stderr',
    ]
  });

  browser2 = await puppeteer.launch({
    headless: false,
    dumpio: false,
    executablePath: BrowserInstaller.revisionInfo.executablePath,
    args: [
      '--use-fake-ui-for-media-stream',
      '--use-fake-device-for-media-stream',
      '-enable-logging',
      '--v=1',
      '--vmodule=*/webrtc/*=2,*=-1',
      '--enable-logging=stderr',
    ]
  });

  const internalPage = await browser.newPage();
  await internalPage.goto(`chrome://webrtc-internals`);

  const internalPage2 = await browser2.newPage();
  await internalPage2.goto(`chrome://webrtc-internals`);
});

after(async function() {
  await browser.close();
  await browser2.close();
});

const describeStreamSwitchTest = function(title, test, only = false) {
  const describeImpl = only ? describe.only : describe;
  describeImpl(title, function() {
    this.timeout(500000);

    const ctx = {
      clientStreams: {},
      erizoStreams: {},
      expectedClientSdpVersion: 2,
      expectedErizoSdpVersion: 0,
      erizo: {},
      client: {},
    };

    let page, page2;
    const erizoConns = {};

    ctx.createClientStream = async function(idx, color, frequency) {
      const stream = new ClientStream(page2, color, frequency);
      ctx.clientStreams[idx] = stream;
      await stream.registerLocalVideoCreator();
      await stream.init();
      await stream.waitForAccepted();
      await stream.getLabel();
      return stream;
    };

    ctx.deleteClientStream = async function(idx) {
      if (ctx.clientStreams[idx]) {
        ctx.clientStreams[idx].remove();
        delete ctx.clientStreams[idx];
      }
    };

    ctx.createErizoStream = async function(idx, label, audio, video) {
      const id = parseInt(currentErizoStreamId++, 0);
      const stream = { id, label: label || id, audio, video, addedToConnection: false };
      ctx.erizoStreams[idx] = stream;
      return stream;
    };

    ctx.deleteErizoStream = async function(idx) {
      delete ctx.erizoStreams[idx];
    };

    ctx.createClientConnection = async function(idx) {
      const connectionId = parseInt(Math.random() * 1000, 0);
      const conn = new ClientConnection(idx === 'pub' ? page2 : page, connectionId);
      ctx.client[idx] = conn;
      await conn.init();
      await conn.registerFrameProcessingFunctions();
      return conn;
    };

    ctx.createErizoConnection = async function(idx) {
      const connectionId = parseInt(Math.random() * 1000, 0);
      const conn = new ErizoConnection(connectionId, idx === 'pub');
      ctx.erizo[idx] = conn;
      return conn;
    };

    after(async function() {
      await page.close();
      await page2.close();
      await ctx.erizo['pub'].removeAllStreams();
      ctx.erizo['pub'].close();
    });

    before(async function() {
      const currentProcessPath = process.cwd();
      const htmlPath = path.join(currentProcessPath, '../../extras/basic_example/public/index.html');
      page = await browser.newPage();

      // page.on('console', msg => console.log('PAGE LOG:', msg.text()));
      await page.goto(`file://${htmlPath}?forceStart=1`);

      page2 = await browser2.newPage();

      // page.on('console', msg => console.log('PAGE LOG:', msg.text()));
      await page2.goto(`file://${htmlPath}?forceStart=1`);
    });

    before(async function() {
      await ctx.createClientConnection('pub');
      await ctx.createErizoConnection('pub');
      await ctx.createClientConnection('sub');
      await ctx.createErizoConnection('sub');
    });

    ctx.checkClientSentCorrectOffer = (getOffer, clientId) => {
      it('Client should send the correct offer', function() {
        const sdp = new SdpChecker(getOffer());
        sdp.expectType('offer');
        sdp.expectToHaveStreams(ctx.client[clientId].streams);
      });
    };

    ctx.checkErizoSentCorrectOffer = (getOffer, erizoId) => {
      it('Erizo should send the correct offer', function() {
        const sdp = new SdpChecker(getOffer());
        sdp.expectType('offer');
        sdp.expectToIncludeCandidates();
        sdp.expectToHaveStreams(ctx.erizo[erizoId].streams);
      });
    };

    ctx.checkClientSentCorrectAnswer = (getAnswer, clientId) => {
      it('Client should send the correct answer', function() {
        const sdp = new SdpChecker(getAnswer());
        sdp.expectType('answer');
        sdp.expectToHaveStreams(ctx.client[clientId].streams);
      });
    };

    ctx.checkErizoSentCorrectAnswer = (getAnswer, erizoId) => {
      it('Erizo should send an answer', function() {
        const sdp = new SdpChecker(getAnswer());
        sdp.expectType('answer');
        sdp.expectToIncludeCandidates();
        sdp.expectToHaveStreams(ctx.erizo[erizoId].streams);
      });
    };

    ctx.checkClientSentCandidates = (clientId) => {
      it('Client should send candidates', function() {
        for (const candidate of ctx.client[clientId].candidates) {
          const sdp = new SdpChecker(candidate);
          sdp.expectType('candidate');
        }
      });
    };

    ctx.checkErizoFinishedSuccessfully = (idx) => {
      it('Erizo should finish successfully', function() {
        expect(ctx.erizo[idx].isReady).to.be.true;
        // expect(erizo.isReady).to.be.true;
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

    ctx.publishStream = async function(clientId, erizoId, clientStreamId) {
      describe('Publish One Client Stream', function() {
        let offer, answer, client, erizo, clientStream;

        before(async function() {
          client = ctx.client[clientId];
          erizo = ctx.erizo[erizoId];
          clientStream = ctx.clientStreams[clientStreamId];
          await client.addStream(clientStream);
          await erizo.publishStream(clientStream);

          await client.setLocalDescription();
          offer = { type: 'offer', sdp: await client.getLocalDescription() };
          await erizo.setRemoteDescription(offer);
          await erizo.createAnswer();
          answer = { type: 'answer', sdp:  await erizo.getLocalDescription()};
          console.log(offer,answer);
          await client.setRemoteDescription(answer);
          if (!client.candidates) {
            await client.waitForCandidates();
            client.candidates = await client.getAllCandidates();
            for (const candidate of client.candidates) {
              await erizo.addIceCandidate(candidate);
            }
          }
          await client.waitForConnected();
          await erizo.waitForReadyMessage();
          await clientStream.show();
        });

        ctx.checkClientSentCorrectOffer(() => offer, clientId);
        ctx.checkErizoSentCorrectAnswer(() => answer, erizoId);
        ctx.checkClientSentCandidates(clientId);
        ctx.checkErizoFinishedSuccessfully(erizoId);
        ctx.checkClientHasNotFailed(clientId);
      });
    };

    ctx.unpublishStream = async function(client, erizo, clientStream) {
      describe('Unpublish One Client Stream', function() {
        let offer, answer;
        before(async function() {
          await client.removeStream(clientStream);
          await erizo.unpublishStream(clientStream);

          await client.setLocalDescription();
          offer = { type: 'offer', sdp: await client.getLocalDescription() };
          await erizo.setRemoteDescription(offer);
          await erizo.createAnswer();
          answer = { type: 'answer', sdp: await erizo.getLocalDescription() };
          await client.setRemoteDescription(answer);

          if (!client.candidates) {
            await client.waitForCandidates();
            client.candidates = await client.getAllCandidates();
            for (const candidate of client.candidates) {
              await erizo.addIceCandidate(candidate);
            }
          }

          await client.waitForConnected();
          await erizo.waitForReadyMessage();

          await ctx.deleteClientStream(clientStream);
        });

        ctx.checkClientSentCorrectOffer(() => offer, client);
        ctx.checkErizoSentCorrectAnswer(() => answer, erizo);
        ctx.checkClientSentCandidates(client);
        ctx.checkErizoFinishedSuccessfully(erizo);
        ctx.checkClientHasNotFailed(client);
      });
    };

    function RGBToHex(r,g,b) {
      r = r.toString(16);
      g = g.toString(16);
      b = b.toString(16);

      if (r.length == 1)
        r = "0" + r;
      if (g.length == 1)
        g = "0" + g;
      if (b.length == 1)
        b = "0" + b;

      return "#" + r + g + b;
    }

    function hexToRGB(hex) {
      var result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
      return result ? [parseInt(result[1], 16), parseInt(result[2], 16), parseInt(result[3], 16)] : null;
    }

    function colorDistance(v1, v2){
      var i,
          d = 0;

      for (i = 0; i < v1.length; i++) {
          d += (v1[i] - v2[i])*(v1[i] - v2[i]);
      }
      return Math.sqrt(d);
    };

    ctx.linkSubToPub = function(clientId, erizoPubId, erizoSubId, streamPubId, streamSubId, wait = 5) {
      describe('Link Subscriber to Publisher', async function() {
        let erizoPub, erizoSub, publisher, subscriber, client, erizoStream, pubStream;
        before(async function() {
          client = ctx.client[clientId];
          erizoPub = ctx.erizo[erizoPubId];
          erizoSub = ctx.erizo[erizoSubId];
          pubStream = ctx.clientStreams[streamPubId];
          publisher = erizoPub.connection.getStream(pubStream.id);
          const subStream = ctx.erizoStreams[streamSubId];
          subscriber = erizoSub.connection.getStream(subStream.id);
          if (!publisher.muxer) {
            publisher.muxer = erizoPub.createOneToManyProcessor();
            publisher.setAudioReceiver(publisher.muxer);
            publisher.setVideoReceiver(publisher.muxer);
            publisher.muxer.setPublisher(publisher);
            publisher.muteStream(false, false);
            await erizoSub.sleep(300);
          }
          publisher.muxer.addSubscriber(subscriber, subscriber.id);
          subscriber.muteStream(false, false);
          await erizoSub.sleep(1000 * wait);

          erizoStream = ctx.erizoStreams[streamSubId];
        });

        it('should have muxer with publisher', async () => {
          expect(publisher.muxer).to.not.be.null;
          expect(publisher.muxer.hasPublisher()).to.be.true;
        });

        // it('should be receiving the right stream', async() => {
        //   const imageData = await client.getImageData(erizoStream);
        //   const imagePixel = [imageData[0], imageData[1], imageData[2]];
        //   const pixelColor = hexToRGB(pubStream.color);
        //   const distance = colorDistance(imagePixel, pixelColor);
        //   expect(distance).to.be.lessThan(100);
        // });

        // it('should be receiving video', async() => {
        //   const imageData = await client.getImageData(erizoStream);
        //   await erizoSub.sleep(100);
        //   const imageData2 = await client.getImageData(erizoStream);
        //   expect(imageData).to.not.be.deep.equals(imageData2);
        // });
      });
    };

    ctx.unlinkSubToPub = function(clientId, erizoPubId, erizoSubId, streamPubId, streamSubId) {
      describe('Unlink Subscriber to Publisher', function() {
        let erizoPub, erizoSub, publisher, subscriber, erizoStream, client;
        before(async function() {
          client = ctx.client[clientId];
          erizoPub = ctx.erizo[erizoPubId];
          erizoSub = ctx.erizo[erizoSubId];
          const pubStream = ctx.clientStreams[streamPubId];
          publisher = erizoPub.connection.getStream(pubStream.id);
          const subStream = ctx.erizoStreams[streamSubId];
          subscriber = erizoSub.connection.getStream(subStream.id);
          if (publisher.muxer) {
            publisher.muxer.removeSubscriber(subscriber.id);
          }
          await erizoSub.sleep(0);

          erizoStream = ctx.erizoStreams[streamSubId];
        });

        it('should have muxer', () => {
          expect(publisher.muxer).to.not.be.null;
          expect(publisher.muxer.hasPublisher()).to.be.true;
        });

        it('should not be receiving video', async() => {
        });
      });
    };

    ctx.subscribeToStream = function(clientId, erizoId, erizoStreamId, erizoPubId) {
      describe('Subscribe One Erizo Stream', function() {
        let offer, answer, client, erizo, erizoPub, erizoStream;
        before(async function() {
          client = ctx.client[clientId];
          erizo = ctx.erizo[erizoId];
          erizoPub = ctx.erizo[erizoPubId];
          erizoStream = ctx.erizoStreams[erizoStreamId];
          const negotiationNeeded = erizo.onceNegotiationIsNeeded();
          await erizo.subscribeStream(erizoStream);
          await negotiationNeeded;

          erizo.connection.copySdpInfoFromConnection(erizoPub.connection);

          await erizo.setLocalDescription();
          offer = { type: 'offer', sdp: await erizo.getLocalDescription() };
          await client.setRemoteDescription(offer);
          await client.setLocalDescription();
          answer = { type: 'answer', sdp: await client.getLocalDescription() };
          await erizo.setRemoteDescription(answer);
          if (!client.candidates) {
            await client.waitForCandidates();
            client.candidates = await client.getAllCandidates();
            for (const candidate of client.candidates) {
              await erizo.addIceCandidate(candidate);
            }
          }

          await client.waitForConnected();
          await erizo.waitForReadyMessage();
          await client.showStream(erizoStream);
        });

        ctx.checkErizoSentCorrectOffer(() => offer, erizoId);
        ctx.checkClientSentCorrectAnswer(() => answer, clientId);
        ctx.checkClientSentCandidates(clientId);
        ctx.checkErizoFinishedSuccessfully(erizoId);
        ctx.checkClientHasNotFailed(clientId);
      });
    };

    ctx.unsubscribeStreamStep = function(client, erizo, erizoStream) {
      describe('Unsubscribe One Erizo Stream', function() {
        let offer, answer, erizoStream;

        before(async function() {
          const negotiationNeeded = erizo.onceNegotiationIsNeeded();
          await erizo.unsubscribeStream(erizoStream);
          await negotiationNeeded;
          await erizo.setLocalDescription();
          offer = { type: 'offer', sdp: await erizo.getLocalDescription() };

          await client.setRemoteDescription(offer);
          answer = { type: 'answer', sdp: await client.getLocalDescription() };

          await erizo.setRemoteDescription(answer);
          if (!client.candidates) {
            await client.waitForCandidates();
            client.candidates = await client.getAllCandidates();
            for (const candidate of client.candidates) {
              await erizo.addIceCandidate(candidate);
            }
          }

          await client.waitForConnected();
          await erizo.waitForReadyMessage();
          await ctx.deleteErizoStream(erizoStream);
        });

        ctx.checkErizoSentCorrectOffer(() => offer, false);
        ctx.checkClientSentCorrectAnswer(() => answer, false);
        ctx.checkClientSentCandidates('sub');
        ctx.checkErizoFinishedSuccessfully('sub');
        ctx.checkClientHasNotFailed('sub');
      });
    };

    test.call(this, ctx);
  });
};



module.exports = describeStreamSwitchTest;
