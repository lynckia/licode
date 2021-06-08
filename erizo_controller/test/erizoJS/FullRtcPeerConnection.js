/* global require, describe, it, beforeEach, afterEach */

/* eslint-disable no-unused-expressions */

// eslint-disable-next-line no-unused-expressions
const mocks = require('../utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');
// eslint-disable-next-line import/no-extraneous-dependencies
const chai = require('chai');
const utils = require('util');
// eslint-disable-next-line import/no-extraneous-dependencies
const chaiAsPromised = require('chai-as-promised');

const mediaConfig = require('./../../../rtp_media_config');
const sdpUtils = require('./../sdps');
const SemanticSdp = require('./../../common/semanticSdp/SemanticSdp');
const Setup = require('./../../common/semanticSdp/Setup');

let describeTest = describe.skip;

// TODO(javier): We enable these tests only for MacOS because there are some issues with
// Linux.
if (process.env.ONLY_FULL_RTC === '1') {
  describeTest = describe.only;
}

const audioPlusVideo = direction => sdpUtils.getChromePublisherSdp([
  { mid: 0, label: 'stream1', ssrc1: '00000', kind: 'audio', direction },
  { mid: 1, label: 'stream1', ssrc1: '1111', ssrc2: '1112', kind: 'video', direction }]);

const audioOnly = direction => sdpUtils.getChromePublisherSdp([
  { mid: 0, label: 'stream1', ssrc1: '00000', kind: 'audio', direction }]);

const videoOnly = direction => sdpUtils.getChromePublisherSdp([
  { mid: 0, label: 'stream1', ssrc1: '00000', kind: 'video', direction }]);

const audioPlusVideoReceiver = direction => sdpUtils.getChromePublisherSdp([
  { mid: 0, label: 'stream1', kind: 'audio', direction },
  { mid: 1, label: 'stream1', kind: 'video', direction }]);

chai.use(chaiAsPromised);
chai.should();
const expect = chai.expect;

// Helpers
const timeout = utils.promisify(setTimeout);

const promisifyOnce = (connection, event) =>
  new Promise(resolve => connection.once(event, resolve));


const expectMediaWith = (media, { id, port, type, direction, setup }) => {
  id && expect(media.id).to.be.equals(id);
  port && expect(media.port).to.be.equals(port);
  type && expect(media.type).to.be.equals(type);
  direction && expect(media.getDirectionString()).to.be.equals(direction);
  setup && expect(Setup.toString(media.getSetup())).to.be.equals(setup);
  expect(media.extensions.size).to.be.greaterThan(0);
  expect(media.codecs.size).to.be.greaterThan(0);
};

describeTest('RTCPeerConnection with WebRtcConnection', () => {
  let connection;
  let erizo;
  let erizoApiMock;
  let RTCPeerConnection;
  let threadPool;
  let ioThreadPool;
  let webRtcConnectionConfiguration;
  let SessionDescription;

  beforeEach('Mock Process', () => {
    this.originalExit = process.exit;
    process.exit = sinon.stub();
  });

  afterEach('Unmock Process', () => {
    process.exit = this.originalExit;
  });

  beforeEach(() => {
    global.mediaConfig = mediaConfig;
    global.bwDistributorConfig = { defaultType: 'TargetVideoBW' };
    global.config = { logger: { configFile: true },
      erizo: {
        addon: 'addonDebug',
        useConnectionQualityCheck: true,

        stunserver: '',
        stunport: 0,
        minport: 60000,
        maxport: 61000,
        turnserver: '',
        turnport: 0,
        turnusername: '',
        turnpass: '',
        networkinterface: '',
      },
    };
    erizoApiMock = mocks.start(mocks.realErizoAPI);
    // eslint-disable-next-line global-require, import/no-unresolved
    erizo = require('./../../../erizoAPI/build/Release/addonDebug');
    // eslint-disable-next-line global-require
    RTCPeerConnection = require('./../../erizoJS/models/RTCPeerConnection');
    // eslint-disable-next-line global-require
    SessionDescription = require('./../../erizoJS/models/SessionDescription');
    threadPool = new erizo.ThreadPool(5);
    threadPool.start();

    ioThreadPool = new erizo.IOThreadPool(1);
    ioThreadPool.start();

    webRtcConnectionConfiguration = {
      threadPool,
      ioThreadPool,
      trickleIce: false,
      mediaConfiguration: 'default',
      id: 'connection1',
      erizoControllerId: 'erizoControllerTest1',
      clientId: 'clientTest1',
      encryptTransport: false,
      options: {
      },
    };
  });

  afterEach(async () => {
    if (connection && !connection.isClosed) {
      await connection.close();
    }
    global.config = {};
    mocks.stop(erizoApiMock);
    mocks.deleteRequireCache();
    mocks.stopAll();
  });

  it('should provide the known API', () => {
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();
    expect(connection.addStream).not.to.be.undefined;
    expect(connection.removeStream).not.to.be.undefined;
    expect(connection.createOffer).not.to.be.undefined;
    expect(connection.createAnswer).not.to.be.undefined;
    expect(connection.addIceCandidate).not.to.be.undefined;
    expect(connection.setLocalDescription).not.to.be.undefined;
    expect(connection.setRemoteDescription).not.to.be.undefined;
    expect(connection.close).not.to.be.undefined;
    expect(connection.internalConnection).not.to.be.undefined;
    const onClose = sinon.spy(connection.internalConnection, 'close');
    connection.close();
    expect(onClose.called).to.be.true;
    expect(connection.isClosed).to.be.true;
  });

  it('should initialize everything', async () => {
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();
    const init = sinon.spy(connection.internalConnection, 'init');
    expect(init).to.be.called;
    expect(connection.localDescription).to.be.null;
    expect(connection.remoteDescription).to.be.null;
    expect(connection.pendingLocalDescription).to.be.null;
    expect(connection.currentLocalDescription).to.be.null;
    expect(connection.remoteDescription).to.be.null;
    expect(connection.pendingRemoteDescription).to.be.null;
    expect(connection.currentRemoteDescription).to.be.null;
    expect(connection.lastCreateOffer).to.be.empty;
    expect(connection.lastCreateAnswer).to.be.empty;
    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;
    expect(connection.isClosed).to.be.false;
    await expect(connection.onInitialized).to.eventually.be.fulfilled;
  });

  it('should trigger a negotiationneeded event when adding the first stream', async () => {
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();

    await connection.onInitialized;
    const onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');

    await connection.addStream(1, { audio: true, video: true }, false, true);
    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;
  });

  it('should finish negotiation successfully when adding and removing remote streams', async () => {
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();
    const label = 'stream1';
    await connection.onInitialized;

    await connection.addStream(1, { label, audio: true, video: true }, true, true);

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setRemoteDescription({ type: 'offer', sdp: audioPlusVideo('sendrecv') });
    expect(connection.signalingState).to.be.equals('have-remote-offer');

    await connection.setLocalDescription();
    const answer1 = connection.localDescription;
    let sdpInfo = SemanticSdp.SDPInfo.processString(answer1);
    expect(sdpInfo.medias.length).to.be.equals(2);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('audio');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('recvonly');
    expect(sdpInfo.medias[1].id).to.be.equals(1);
    expect(sdpInfo.medias[1].type).to.be.equals('video');
    expect(sdpInfo.medias[1].getDirectionString()).to.be.equals('recvonly');
    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    await connection.removeStream(1);
    expect(connection.signalingState).to.be.equals('stable');
    await connection.setRemoteDescription({ type: 'offer', sdp: audioPlusVideo('recvonly') });
    expect(connection.signalingState).to.be.equals('have-remote-offer');

    await connection.setLocalDescription();
    const answer2 = connection.localDescription;
    sdpInfo = SemanticSdp.SDPInfo.processString(answer2);
    expect(sdpInfo.medias.length).to.be.equals(2);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('inactive');
    expect(sdpInfo.medias[0].type).to.be.equals('audio');
    expect(sdpInfo.medias[1].id).to.be.equals(1);
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('inactive');
    expect(sdpInfo.medias[1].type).to.be.equals('video');
    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    await connection.addStream(1, { label, audio: true, video: true }, true, true);

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setRemoteDescription({ type: 'offer', sdp: audioPlusVideo('sendrecv') });
    expect(connection.signalingState).to.be.equals('have-remote-offer');

    await connection.setLocalDescription();
    const answer3 = connection.localDescription;
    sdpInfo = SemanticSdp.SDPInfo.processString(answer3);
    expect(sdpInfo.medias.length).to.be.equals(2);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('audio');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('recvonly');
    expect(sdpInfo.medias[1].id).to.be.equals(1);
    expect(sdpInfo.medias[1].type).to.be.equals('video');
    expect(sdpInfo.medias[1].getDirectionString()).to.be.equals('recvonly');
    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;
  });

  it('should answer with the proper media lines when it is subscribed to multiple streams', async () => {
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();
    const clientSDP1 = sdpUtils.getChromePublisherSdp([
      { mid: 0, kind: 'audio', direction: 'sendrecv' },
      { mid: 1, kind: 'video', direction: 'sendrecv' },
    ]);
    const clientSDP2 = sdpUtils.getChromePublisherSdp([
      { mid: 0, label: 'stream1', ssrc1: '00000', kind: 'audio', direction: 'sendrecv' },
      { mid: 1, label: 'stream1', ssrc1: '1111', ssrc2: '1112', kind: 'video', direction: 'sendrecv' },
    ]);

    await connection.onInitialized;

    let onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');
    await connection.addStream(1, { label: 'stream1', audio: true, video: true }, true, true); // Publisher
    await connection.addStream(2, { label: 'stream2', audio: true, video: true }, false, true); // Subscriber
    await onNegotiationNeededPromise;

    await connection.setLocalDescription();
    let sdpInfo = SemanticSdp.SDPInfo.processString(connection.localDescription);
    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({ type: 'answer', sdp: clientSDP1 });
    expect(connection.signalingState).to.be.equals('stable');

    expect(sdpInfo.medias.length).to.be.equals(2);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('audio');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('sendrecv');
    expect(sdpInfo.medias[1].id).to.be.equals(1);
    expect(sdpInfo.medias[1].type).to.be.equals('video');
    expect(sdpInfo.medias[1].getDirectionString()).to.be.equals('sendrecv');

    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    await connection.setRemoteDescription({ type: 'offer', sdp: clientSDP2 });
    expect(connection.signalingState).to.be.equals('have-remote-offer');
    await connection.setLocalDescription();
    sdpInfo = SemanticSdp.SDPInfo.processString(connection.localDescription);

    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');
    await connection.removeStream(2);
    await onNegotiationNeededPromise;

    await connection.setLocalDescription();
    sdpInfo = SemanticSdp.SDPInfo.processString(connection.localDescription);

    expect(sdpInfo.medias.length).to.be.equals(2);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('audio');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('recvonly');
    expect(sdpInfo.medias[1].id).to.be.equals(1);
    expect(sdpInfo.medias[1].type).to.be.equals('video');
    expect(sdpInfo.medias[1].getDirectionString()).to.be.equals('recvonly');
  });

  it('should finish negotiation successfully when adding and removing remote streams (with createAnswer)', async () => {
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();
    const label = 'stream1';
    await connection.onInitialized;

    await connection.addStream(1, { label, audio: true, video: true }, true, true);

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setRemoteDescription({ type: 'offer', sdp: audioPlusVideo('sendrecv') });
    expect(connection.signalingState).to.be.equals('have-remote-offer');

    const answer1 = await connection.createAnswer();
    await connection.setLocalDescription(answer1);
    let sdpInfo = SemanticSdp.SDPInfo.processString(answer1.sdp);
    expect(sdpInfo.medias.length).to.be.equals(2);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('audio');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('recvonly');
    expect(sdpInfo.medias[1].id).to.be.equals(1);
    expect(sdpInfo.medias[1].type).to.be.equals('video');
    expect(sdpInfo.medias[1].getDirectionString()).to.be.equals('recvonly');
    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    await connection.removeStream(1);
    expect(connection.signalingState).to.be.equals('stable');
    await connection.setRemoteDescription({ type: 'offer', sdp: audioPlusVideo('recvonly') });
    expect(connection.signalingState).to.be.equals('have-remote-offer');

    const answer2 = await connection.createAnswer();
    await connection.setLocalDescription(answer2);
    sdpInfo = SemanticSdp.SDPInfo.processString(answer2.sdp);
    expect(sdpInfo.medias.length).to.be.equals(2);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('inactive');
    expect(sdpInfo.medias[0].type).to.be.equals('audio');
    expect(sdpInfo.medias[1].id).to.be.equals(1);
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('inactive');
    expect(sdpInfo.medias[1].type).to.be.equals('video');
    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    await connection.addStream(1, { label, audio: true, video: true }, true, true);

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setRemoteDescription({ type: 'offer', sdp: audioPlusVideo('sendrecv') });
    expect(connection.signalingState).to.be.equals('have-remote-offer');

    const answer3 = await connection.createAnswer();
    await connection.setLocalDescription(answer3);
    sdpInfo = SemanticSdp.SDPInfo.processString(answer3.sdp);
    expect(sdpInfo.medias.length).to.be.equals(2);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('audio');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('recvonly');
    expect(sdpInfo.medias[1].id).to.be.equals(1);
    expect(sdpInfo.medias[1].type).to.be.equals('video');
    expect(sdpInfo.medias[1].getDirectionString()).to.be.equals('recvonly');
    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;
  });

  it('should finish negotiation successfully when adding and removing streams', async () => {
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();
    const label = 'stream10';

    await connection.onInitialized;
    let onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');

    await connection.addStream(1, { label, audio: true, video: true }, false, true);
    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setLocalDescription();
    let offer = connection.localDescription;
    let sdpInfo = SemanticSdp.SDPInfo.processString(offer);
    expect(sdpInfo.medias.length).to.be.equals(2);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('audio');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('sendrecv');
    expect(sdpInfo.medias[1].id).to.be.equals(1);
    expect(sdpInfo.medias[1].type).to.be.equals('video');
    expect(sdpInfo.medias[1].getDirectionString()).to.be.equals('sendrecv');

    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({ type: 'answer', sdp: audioPlusVideoReceiver('recvonly') });
    expect(connection.signalingState).to.be.equals('stable');

    expect(connection.negotiationNeeded).to.be.false;

    onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');
    await connection.removeStream(1);
    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');

    await connection.setLocalDescription();
    offer = connection.localDescription;

    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({ type: 'answer', sdp: audioPlusVideo('inactive') });

    expect(connection.signalingState).to.be.equals('stable');

    sdpInfo = SemanticSdp.SDPInfo.processString(offer);
    expect(sdpInfo.medias.length).to.be.equals(2);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('audio');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('inactive');
    expect(sdpInfo.medias[1].id).to.be.equals(1);
    expect(sdpInfo.medias[1].type).to.be.equals('video');
    expect(sdpInfo.medias[1].getDirectionString()).to.be.equals('inactive');
    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');
    await connection.addStream(1, { label, audio: true, video: true }, false, true);

    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setLocalDescription();
    offer = connection.localDescription;
    sdpInfo = SemanticSdp.SDPInfo.processString(offer);
    expect(sdpInfo.medias.length).to.be.equals(3);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('audio');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('inactive');
    expect(sdpInfo.medias[1].id).to.be.equals(2);
    expect(sdpInfo.medias[1].type).to.be.equals('audio');
    expect(sdpInfo.medias[1].getDirectionString()).to.be.equals('sendrecv');
    expect(sdpInfo.medias[2].id).to.be.equals(3);
    expect(sdpInfo.medias[2].type).to.be.equals('video');
    expect(sdpInfo.medias[2].getDirectionString()).to.be.equals('sendrecv');

    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({ type: 'answer', sdp: audioPlusVideoReceiver('recvonly') });

    expect(connection.signalingState).to.be.equals('stable');

    expect(connection.negotiationNeeded).to.be.false;
  });

  it('should negotiate audio only', async () => {
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();
    const label = 'stream10';

    await connection.onInitialized;
    let onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');

    await connection.addStream(1, { label, audio: true, video: false }, false, true);
    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setLocalDescription();
    let offer = connection.localDescription;
    let sdpInfo = SemanticSdp.SDPInfo.processString(offer);
    expect(sdpInfo.medias.length).to.be.equals(1);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('audio');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('sendrecv');

    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({ type: 'answer', sdp: audioOnly('recvonly') });
    expect(connection.signalingState).to.be.equals('stable');

    expect(connection.negotiationNeeded).to.be.false;

    onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');
    await connection.removeStream(1);
    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');

    await connection.setLocalDescription();
    offer = connection.localDescription;

    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({ type: 'answer', sdp: audioOnly('inactive') });

    expect(connection.signalingState).to.be.equals('stable');

    sdpInfo = SemanticSdp.SDPInfo.processString(offer);
    expect(sdpInfo.medias.length).to.be.equals(1);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('audio');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('inactive');
    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');
    await connection.addStream(1, { label, audio: true, video: true }, false, true);

    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setLocalDescription();
    offer = connection.localDescription;
    sdpInfo = SemanticSdp.SDPInfo.processString(offer);
    expect(sdpInfo.medias.length).to.be.equals(3);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('audio');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('inactive');
    expect(sdpInfo.medias[1].id).to.be.equals(1);
    expect(sdpInfo.medias[1].type).to.be.equals('audio');
    expect(sdpInfo.medias[1].getDirectionString()).to.be.equals('sendrecv');
    expect(sdpInfo.medias[2].id).to.be.equals(2);
    expect(sdpInfo.medias[2].type).to.be.equals('video');
    expect(sdpInfo.medias[2].getDirectionString()).to.be.equals('sendrecv');

    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({ type: 'answer', sdp: audioOnly('recvonly') });

    expect(connection.signalingState).to.be.equals('stable');

    expect(connection.negotiationNeeded).to.be.false;
  });

  it('should negotiate video only', async () => {
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();
    const label = 'stream10';

    await connection.onInitialized;
    let onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');

    await connection.addStream(1, { label, audio: false, video: true }, false, true);
    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setLocalDescription();
    let offer = connection.localDescription;
    let sdpInfo = SemanticSdp.SDPInfo.processString(offer);
    expect(sdpInfo.medias.length).to.be.equals(1);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('video');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('sendrecv');

    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({ type: 'answer', sdp: videoOnly('recvonly') });
    expect(connection.signalingState).to.be.equals('stable');

    expect(connection.negotiationNeeded).to.be.false;

    onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');
    await connection.removeStream(1);
    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');

    await connection.setLocalDescription();
    offer = connection.localDescription;

    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({ type: 'answer', sdp: videoOnly('inactive') });

    expect(connection.signalingState).to.be.equals('stable');

    sdpInfo = SemanticSdp.SDPInfo.processString(offer);
    expect(sdpInfo.medias.length).to.be.equals(1);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('video');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('inactive');
    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');
    await connection.addStream(1, { label, audio: true, video: true }, false, true);

    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setLocalDescription();
    offer = connection.localDescription;
    sdpInfo = SemanticSdp.SDPInfo.processString(offer);
    expect(sdpInfo.medias.length).to.be.equals(3);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('video');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('inactive');
    expect(sdpInfo.medias[1].id).to.be.equals(1);
    expect(sdpInfo.medias[1].type).to.be.equals('audio');
    expect(sdpInfo.medias[1].getDirectionString()).to.be.equals('sendrecv');
    expect(sdpInfo.medias[2].id).to.be.equals(2);
    expect(sdpInfo.medias[2].type).to.be.equals('video');
    expect(sdpInfo.medias[2].getDirectionString()).to.be.equals('sendrecv');

    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({ type: 'answer', sdp: videoOnly('recvonly') });

    expect(connection.signalingState).to.be.equals('stable');

    expect(connection.negotiationNeeded).to.be.false;
  });

  it('should negotiate two publishers (Audio+Video after Video Only)', async () => {
    const config = Object.assign({}, webRtcConnectionConfiguration);
    connection = new RTCPeerConnection(config);
    connection.init();
    let label = 'stream1';

    await connection.onInitialized;

    await connection.addStream('stream1', { label, audio: false, video: true }, true);

    expect(connection.negotiationNeeded).to.be.false;

    await connection.setRemoteDescription({
      type: 'offer',
      sdp: sdpUtils.getChromePublisherSdp([
        { mid: 0, label, kind: 'video', direction: 'sendrecv', setup: 'actpass' },
      ]),
    });

    expect(connection.signalingState).to.be.equals('have-remote-offer');
    await connection.setLocalDescription();
    let answer = connection.localDescription;
    let sdpInfo = SemanticSdp.SDPInfo.processString(answer);

    expect(sdpInfo.medias.length).to.be.equals(1);

    (sdpInfo.medias[0], { id: 0, port: 9, type: 'video', direction: 'recvonly', setup: 'active' });

    expect(connection.signalingState).to.be.equals('stable');

    expect(connection.negotiationNeeded).to.be.false;

    label = 'stream2';

    await connection.addStream('stream2', { label, audio: true, video: true }, true);

    expect(connection.negotiationNeeded).to.be.false;
    expect(connection.signalingState).to.be.equals('stable');
    await connection.setRemoteDescription({
      type: 'offer',
      sdp: sdpUtils.getChromePublisherSdp([
        { mid: 0, label: 'stream1', kind: 'video', direction: 'sendrecv', setup: 'active' },
        { mid: 1, label, kind: 'audio', direction: 'sendrecv', setup: 'actpass' },
        { mid: 2, label, kind: 'video', direction: 'sendrecv', setup: 'actpass' },
      ]),
    });

    expect(connection.signalingState).to.be.equals('have-remote-offer');

    await connection.setLocalDescription();
    answer = connection.localDescription;
    sdpInfo = SemanticSdp.SDPInfo.processString(answer);
    expect(sdpInfo.medias.length).to.be.equals(3);
    expectMediaWith(sdpInfo.medias[0], { id: 0, port: 9, type: 'video', direction: 'recvonly', setup: 'active' });
    expectMediaWith(sdpInfo.medias[1], { id: 1, port: 9, type: 'audio', direction: 'recvonly', setup: 'active' });
    expectMediaWith(sdpInfo.medias[2], { id: 2, port: 9, type: 'video', direction: 'recvonly', setup: 'active' });

    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;
  });

  it('should negotiate audio only reusing senders', async () => {
    const config = Object.assign({ }, webRtcConnectionConfiguration);
    connection = new RTCPeerConnection(config);
    connection.init();
    const label = 'stream10';

    await connection.onInitialized;
    let onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');

    await connection.addStream(1, { label, audio: true, video: false }, false, true);
    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setLocalDescription();
    let offer = connection.localDescription;
    let sdpInfo = SemanticSdp.SDPInfo.processString(offer);
    expect(sdpInfo.medias.length).to.be.equals(1);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('audio');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('sendrecv');

    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({ type: 'answer', sdp: audioOnly('recvonly') });
    expect(connection.signalingState).to.be.equals('stable');

    expect(connection.negotiationNeeded).to.be.false;

    onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');
    await connection.removeStream(1);
    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');

    await connection.setLocalDescription();
    offer = connection.localDescription;

    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({ type: 'answer', sdp: audioOnly('inactive') });

    expect(connection.signalingState).to.be.equals('stable');

    sdpInfo = SemanticSdp.SDPInfo.processString(offer);
    expect(sdpInfo.medias.length).to.be.equals(1);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('audio');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('inactive');
    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');
    await connection.addStream(1, { label, audio: true, video: true }, false, true);

    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setLocalDescription();
    offer = connection.localDescription;
    sdpInfo = SemanticSdp.SDPInfo.processString(offer);
    expect(sdpInfo.medias.length).to.be.equals(3);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('audio');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('inactive');
    expect(sdpInfo.medias[1].id).to.be.equals(1);
    expect(sdpInfo.medias[1].type).to.be.equals('audio');
    expect(sdpInfo.medias[1].getDirectionString()).to.be.equals('sendrecv');
    expect(sdpInfo.medias[2].id).to.be.equals(2);
    expect(sdpInfo.medias[2].type).to.be.equals('video');
    expect(sdpInfo.medias[2].getDirectionString()).to.be.equals('sendrecv');

    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({ type: 'answer', sdp: audioOnly('recvonly') });

    expect(connection.signalingState).to.be.equals('stable');

    expect(connection.negotiationNeeded).to.be.false;
  });

  it('should negotiate video only reusing senders', async () => {
    const config = Object.assign({ }, webRtcConnectionConfiguration);
    connection = new RTCPeerConnection(config);
    connection.init();
    const label = 'stream10';

    await connection.onInitialized;
    let onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');

    await connection.addStream(1, { label, audio: false, video: true }, false);
    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setLocalDescription();
    let offer = connection.localDescription;
    let sdpInfo = SemanticSdp.SDPInfo.processString(offer);
    expect(sdpInfo.medias.length).to.be.equals(1);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('video');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('sendrecv');

    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({ type: 'answer', sdp: audioOnly('recvonly') });
    expect(connection.signalingState).to.be.equals('stable');

    expect(connection.negotiationNeeded).to.be.false;

    onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');
    await connection.removeStream(1);
    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');

    await connection.setLocalDescription();
    offer = connection.localDescription;

    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({ type: 'answer', sdp: audioOnly('inactive') });

    expect(connection.signalingState).to.be.equals('stable');

    sdpInfo = SemanticSdp.SDPInfo.processString(offer);
    expect(sdpInfo.medias.length).to.be.equals(1);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('video');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('inactive');
    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');
    await connection.addStream(1, { label, audio: true, video: true }, false, true);

    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setLocalDescription();
    offer = connection.localDescription;
    sdpInfo = SemanticSdp.SDPInfo.processString(offer);
    expect(sdpInfo.medias.length).to.be.equals(3);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('video');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('inactive');
    expect(sdpInfo.medias[1].id).to.be.equals(1);
    expect(sdpInfo.medias[1].type).to.be.equals('audio');
    expect(sdpInfo.medias[1].getDirectionString()).to.be.equals('sendrecv');
    expect(sdpInfo.medias[2].id).to.be.equals(2);
    expect(sdpInfo.medias[2].type).to.be.equals('video');
    expect(sdpInfo.medias[2].getDirectionString()).to.be.equals('sendrecv');

    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({ type: 'answer', sdp: audioOnly('recvonly') });

    expect(connection.signalingState).to.be.equals('stable');

    expect(connection.negotiationNeeded).to.be.false;
  });

  it('should negotiate AV reusing senders', async () => {
    const config = Object.assign({}, webRtcConnectionConfiguration);
    const publisherSdpInfo = SemanticSdp.SDPInfo.processString(sdpUtils.getChromePublisherSdp([
      { mid: 0, label: '1', kind: 'audio', direction: 'recvonly', setup: 'active' },
      { mid: 1, label: '1', kind: 'video', direction: 'recvonly', setup: 'active' },
    ]));
    const publisherDescription = new SessionDescription(publisherSdpInfo, 'default');
    connection = new RTCPeerConnection(config);
    connection.init();
    const label = 'stream10';

    await connection.onInitialized;
    let onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');

    await connection.addStream('1', { label, audio: true, video: true }, false);
    connection.internalConnection.wrtc
      .copySdpToLocalDescription(publisherDescription.connectionDescription);
    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setLocalDescription();
    let offer = connection.localDescription;
    let sdpInfo = SemanticSdp.SDPInfo.processString(offer);

    expect(sdpInfo.medias.length).to.be.equals(2);
    expectMediaWith(sdpInfo.medias[0], { id: 0, port: 9, type: 'audio', direction: 'sendrecv', setup: 'actpass' });
    expectMediaWith(sdpInfo.medias[1], { id: 1, port: 9, type: 'video', direction: 'sendrecv', setup: 'actpass' });

    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({ type: 'answer', sdp: videoOnly('recvonly') });
    expect(connection.signalingState).to.be.equals('stable');

    expect(connection.negotiationNeeded).to.be.false;

    onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');
    await connection.removeStream('1');
    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');

    await connection.setLocalDescription();
    offer = connection.localDescription;

    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({ type: 'answer', sdp: videoOnly('inactive') });

    expect(connection.signalingState).to.be.equals('stable');

    sdpInfo = SemanticSdp.SDPInfo.processString(offer);
    expect(sdpInfo.medias.length).to.be.equals(2);
    expectMediaWith(sdpInfo.medias[0], { id: 0, port: 9, type: 'audio', direction: 'inactive', setup: 'passive' });
    expectMediaWith(sdpInfo.medias[1], { id: 0, port: 0, type: 'video', direction: 'inactive', setup: 'actpass' });

    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');
    await connection.addStream('2', { label, audio: true, video: true }, false);
    connection.internalConnection.wrtc
      .copySdpToLocalDescription(publisherDescription.connectionDescription);
    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setLocalDescription();
    offer = connection.localDescription;
    sdpInfo = SemanticSdp.SDPInfo.processString(offer);
    expect(sdpInfo.medias.length).to.be.equals(3);
    expectMediaWith(sdpInfo.medias[0], { id: 0, port: 9, type: 'audio', direction: 'inactive', setup: 'passive' });
    expectMediaWith(sdpInfo.medias[1], { id: 2, port: 9, type: 'audio', direction: 'sendrecv', setup: 'actpass' });
    expectMediaWith(sdpInfo.medias[2], { id: 3, port: 9, type: 'video', direction: 'sendrecv', setup: 'actpass' });

    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({
      type: 'answer',
      sdp: sdpUtils.getChromePublisherSdp([
        { mid: 0, label: undefined, kind: 'video', direction: 'inactive', setup: 'active' },
        { mid: 2, label, kind: 'audio', direction: 'recvonly', setup: 'active' },
        { mid: 3, label, kind: 'video', direction: 'recvonly', setup: 'active' },
      ]),
    });

    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');
    await connection.removeStream('2');
    await connection.addStream('3', { label, audio: true, video: true }, false);

    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');

    await connection.setLocalDescription();
    offer = connection.localDescription;
    sdpInfo = SemanticSdp.SDPInfo.processString(offer);

    expect(connection.signalingState).to.be.equals('have-local-offer');
    await connection.setRemoteDescription({
      type: 'answer',
      sdp: sdpUtils.getChromePublisherSdp([
        { mid: 0, label: undefined, kind: 'video', direction: 'inactive', setup: 'active' },
        { mid: 2, label, kind: 'audio', direction: 'inactive', setup: 'active' },
        { mid: 3, label, kind: 'video', direction: 'inactive', setup: 'active' },
        { mid: 4, label, kind: 'audio', direction: 'recvonly', setup: 'active' },
        { mid: 5, label, kind: 'video', direction: 'recvonly', setup: 'active' },
      ]),
    });

    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    expect(sdpInfo.medias.length).to.be.equals(5);
    expectMediaWith(sdpInfo.medias[0], { id: 0, port: 9, type: 'audio', direction: 'inactive', setup: 'passive' });
    expectMediaWith(sdpInfo.medias[1], { id: 2, port: 0, type: 'audio', direction: 'inactive', setup: 'actpass' });
    expectMediaWith(sdpInfo.medias[2], { id: 3, port: 0, type: 'video', direction: 'inactive', setup: 'actpass' });
    expectMediaWith(sdpInfo.medias[3], { id: 4, port: 9, type: 'audio', direction: 'sendrecv', setup: 'actpass' });
    expectMediaWith(sdpInfo.medias[4], { id: 5, port: 9, type: 'video', direction: 'sendrecv', setup: 'actpass' });

    onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');
    await connection.removeStream('3');

    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setLocalDescription();
    offer = connection.localDescription;
    sdpInfo = SemanticSdp.SDPInfo.processString(offer);

    expect(connection.signalingState).to.be.equals('have-local-offer');
    expect(sdpInfo.medias.length).to.be.equals(5);
    expectMediaWith(sdpInfo.medias[0], { id: 0, port: 9, type: 'audio', direction: 'inactive', setup: 'passive' });
    expectMediaWith(sdpInfo.medias[1], { id: 2, port: 0, type: 'audio', direction: 'inactive', setup: 'actpass' });
    expectMediaWith(sdpInfo.medias[2], { id: 3, port: 0, type: 'video', direction: 'inactive', setup: 'actpass' });
    expectMediaWith(sdpInfo.medias[3], { id: 4, port: 0, type: 'audio', direction: 'inactive', setup: 'actpass' });
    expectMediaWith(sdpInfo.medias[4], { id: 5, port: 0, type: 'video', direction: 'inactive', setup: 'actpass' });
  });

  it('should negotiate AV reusing receivers', async () => {
    const config = Object.assign({}, webRtcConnectionConfiguration);
    connection = new RTCPeerConnection(config);
    connection.init();
    let label = 'stream1';

    await connection.onInitialized;

    await connection.addStream('stream1', { label, audio: true, video: true }, true);

    expect(connection.negotiationNeeded).to.be.false;

    await connection.setRemoteDescription({
      type: 'offer',
      sdp: sdpUtils.getChromePublisherSdp([
        { mid: 0, label, kind: 'audio', direction: 'sendrecv', setup: 'actpass' },
        { mid: 1, label, kind: 'video', direction: 'sendrecv', setup: 'actpass' },
      ]),
    });

    expect(connection.signalingState).to.be.equals('have-remote-offer');
    await connection.setLocalDescription();
    let answer = connection.localDescription;
    let sdpInfo = SemanticSdp.SDPInfo.processString(answer);

    expect(sdpInfo.medias.length).to.be.equals(2);
    expectMediaWith(sdpInfo.medias[0], { id: 0, port: 9, type: 'audio', direction: 'recvonly', setup: 'active' });
    expectMediaWith(sdpInfo.medias[1], { id: 1, port: 9, type: 'video', direction: 'recvonly', setup: 'active' });

    expect(connection.signalingState).to.be.equals('stable');

    expect(connection.negotiationNeeded).to.be.false;

    await connection.removeStream('stream1');
    expect(connection.negotiationNeeded).to.be.false;

    await connection.setRemoteDescription({
      type: 'offer',
      sdp: sdpUtils.getChromePublisherSdp([
        { mid: 0, label: undefined, kind: 'audio', direction: 'inactive', setup: 'active' },
        { mid: 1, label: undefined, kind: 'video', direction: 'inactive', setup: 'actpass' },
      ]),
    });

    expect(connection.signalingState).to.be.equals('have-remote-offer');

    await connection.setLocalDescription();
    answer = connection.localDescription;

    expect(connection.signalingState).to.be.equals('stable');

    sdpInfo = SemanticSdp.SDPInfo.processString(answer);
    expect(sdpInfo.medias.length).to.be.equals(2);
    expectMediaWith(sdpInfo.medias[0], { id: 0, port: 9, type: 'audio', direction: 'inactive', setup: 'active' });
    expectMediaWith(sdpInfo.medias[1], { id: 0, port: 0, type: 'video', direction: 'inactive', setup: 'actpass' });

    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    label = 'stream2';

    await connection.addStream('stream2', { label, audio: true, video: true }, true);

    expect(connection.negotiationNeeded).to.be.false;
    expect(connection.signalingState).to.be.equals('stable');
    await connection.setRemoteDescription({
      type: 'offer',
      sdp: sdpUtils.getChromePublisherSdp([
        { mid: 0, label: undefined, kind: 'video', direction: 'inactive', setup: 'active' },
        { mid: 2, label, kind: 'audio', direction: 'sendrecv', setup: 'actpass' },
        { mid: 3, label, kind: 'video', direction: 'sendrecv', setup: 'actpass' },
      ]),
    });

    expect(connection.signalingState).to.be.equals('have-remote-offer');

    await connection.setLocalDescription();
    answer = connection.localDescription;
    sdpInfo = SemanticSdp.SDPInfo.processString(answer);

    expect(sdpInfo.medias.length).to.be.equals(3);
    expectMediaWith(sdpInfo.medias[0], { id: 0, port: 9, type: 'audio', direction: 'inactive', setup: 'active' });
    expectMediaWith(sdpInfo.medias[1], { id: 2, port: 9, type: 'audio', direction: 'recvonly', setup: 'active' });
    expectMediaWith(sdpInfo.medias[2], { id: 3, port: 9, type: 'video', direction: 'recvonly', setup: 'active' });

    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    label = 'stream3';
    await connection.removeStream('stream2');
    await connection.addStream('stream3', { label, audio: true, video: true }, true);

    expect(connection.negotiationNeeded).to.be.false;

    expect(connection.signalingState).to.be.equals('stable');

    await connection.setRemoteDescription({
      type: 'offer',
      sdp: sdpUtils.getChromePublisherSdp([
        { mid: 0, label: undefined, kind: 'video', direction: 'inactive', setup: 'passive' },
        { mid: 2, label: undefined, kind: 'audio', direction: 'inactive', setup: 'passive' },
        { mid: 3, label: undefined, kind: 'video', direction: 'inactive', setup: 'passive' },
        { mid: 4, label, kind: 'audio', direction: 'sendrecv', setup: 'actpass' },
        { mid: 5, label, kind: 'video', direction: 'sendrecv', setup: 'actpass' },
      ]),
    });

    expect(connection.signalingState).to.be.equals('have-remote-offer');

    await connection.setLocalDescription();
    answer = connection.localDescription;
    sdpInfo = SemanticSdp.SDPInfo.processString(answer);

    expect(connection.signalingState).to.be.equals('stable');

    expect(connection.negotiationNeeded).to.be.false;

    expect(sdpInfo.medias.length).to.be.equals(5);
    expectMediaWith(sdpInfo.medias[0], { id: 0, port: 9, type: 'audio', direction: 'inactive', setup: 'active' });
    expectMediaWith(sdpInfo.medias[1], { id: 2, port: 0, type: 'audio', direction: 'inactive', setup: 'actpass' });
    expectMediaWith(sdpInfo.medias[2], { id: 3, port: 0, type: 'video', direction: 'inactive', setup: 'actpass' });
    expectMediaWith(sdpInfo.medias[3], { id: 4, port: 9, type: 'audio', direction: 'recvonly', setup: 'active' });
    expectMediaWith(sdpInfo.medias[4], { id: 5, port: 9, type: 'video', direction: 'recvonly', setup: 'active' });

    await connection.removeStream('stream3');

    expect(connection.negotiationNeeded).to.be.false;

    await connection.setRemoteDescription({
      type: 'offer',
      sdp: sdpUtils.getChromePublisherSdp([
        { mid: 0, label: undefined, kind: 'video', direction: 'inactive', setup: 'passive' },
        { mid: 2, label: undefined, kind: 'audio', direction: 'inactive', setup: 'passive' },
        { mid: 3, label: undefined, kind: 'video', direction: 'inactive', setup: 'passive' },
        { mid: 4, label: undefined, kind: 'audio', direction: 'inactive', setup: 'passive' },
        { mid: 5, label: undefined, kind: 'video', direction: 'inactive', setup: 'passive' },
      ]),
    });

    expect(connection.signalingState).to.be.equals('have-remote-offer');

    await connection.setLocalDescription();
    answer = connection.localDescription;
    sdpInfo = SemanticSdp.SDPInfo.processString(answer);

    expect(connection.signalingState).to.be.equals('stable');

    expect(sdpInfo.medias.length).to.be.equals(5);
    expectMediaWith(sdpInfo.medias[0], { id: 0, port: 9, type: 'audio', direction: 'inactive', setup: 'active' });
    expectMediaWith(sdpInfo.medias[1], { id: 2, port: 0, type: 'audio', direction: 'inactive', setup: 'actpass' });
    expectMediaWith(sdpInfo.medias[2], { id: 3, port: 0, type: 'video', direction: 'inactive', setup: 'actpass' });
    expectMediaWith(sdpInfo.medias[3], { id: 4, port: 0, type: 'audio', direction: 'inactive', setup: 'actpass' });
    expectMediaWith(sdpInfo.medias[4], { id: 5, port: 0, type: 'video', direction: 'inactive', setup: 'actpass' });
  });

  it('should negotiate AV with multiple sending streams', async () => {
    const kNumberOfPublishers = 10;
    const config = Object.assign({}, webRtcConnectionConfiguration);
    connection = new RTCPeerConnection(config);
    const originalCreateOffer = connection.internalConnection.wrtc
      .createOffer.bind(connection.internalConnection.wrtc);
    const createOffer = sinon.stub(connection.internalConnection.wrtc, 'createOffer');
    const originalGetLocalDescription = connection.internalConnection.wrtc
      .getLocalDescription.bind(connection.internalConnection.wrtc);
    const getLocalDescription = sinon.stub(connection.internalConnection.wrtc, 'getLocalDescription');
    createOffer.callsFake(bundle => new Promise((resolve) => {
      setTimeout(async () => {
        await originalCreateOffer(bundle);
        resolve();
      }, 20);
    }));
    getLocalDescription.callsFake(() => new Promise((resolve) => {
      setTimeout(async () => {
        const sdp = await originalGetLocalDescription();
        resolve(sdp);
      }, 20);
    }));
    connection.init();
    const label = 'stream0';

    await connection.onInitialized;

    await connection.addStream('stream0', { label, audio: true, video: true }, true);

    expect(connection.signalingState).to.be.equals('stable');
    const promises = [];
    await connection.setRemoteDescription({
      type: 'offer',
      sdp: sdpUtils.getChromePublisherSdp([
        { mid: 0, label, kind: 'audio', direction: 'sendrecv', setup: 'actpass' },
        { mid: 1, label, kind: 'video', direction: 'sendrecv', setup: 'actpass' },
      ]),
    });
    expect(connection.signalingState).to.be.equals('have-remote-offer');

    for (let index = 1; index < kNumberOfPublishers; index += 1) {
      const label2 = `stream${index}`;
      // eslint-disable-next-line no-loop-func
      promises.push(new Promise((resolve) => {
        connection.addStream(index, { label: label2, audio: true, video: true }, true).then(() => {
          resolve();
        });
      }));
    }
    promises.push(connection.setLocalDescription());
    await Promise.all(promises);
    let answer = connection.localDescription;
    let sdpInfo = SemanticSdp.SDPInfo.processString(answer);

    expect(sdpInfo.medias.length).to.be.equals(2);
    expectMediaWith(sdpInfo.medias[0], { id: 0, port: 9, type: 'audio', direction: 'recvonly', setup: 'active' });
    expectMediaWith(sdpInfo.medias[1], { id: 1, port: 9, type: 'video', direction: 'recvonly', setup: 'active' });

    expect(connection.signalingState).to.be.equals('stable');

    const sdp = [
      { mid: 0, label, kind: 'audio', direction: 'sendrecv', setup: 'passive' },
      { mid: 1, label, kind: 'video', direction: 'sendrecv', setup: 'passive' },
    ];
    for (let index = 2; index < 2 * kNumberOfPublishers; index += 1) {
      const label2 = `stream${Math.floor(index / 2)}`;
      sdp.push({ mid: index, label: label2, kind: index % 2 ? 'video' : 'audio', direction: 'sendrecv', setup: 'actpass' });
    }
    await connection.setRemoteDescription({
      type: 'offer',
      sdp: sdpUtils.getChromePublisherSdp(sdp),
    });
    expect(connection.signalingState).to.be.equals('have-remote-offer');

    await connection.setLocalDescription();
    answer = connection.localDescription;
    sdpInfo = SemanticSdp.SDPInfo.processString(answer);

    expect(sdpInfo.medias.length).to.be.equals(kNumberOfPublishers * 2);
    expect(connection.signalingState).to.be.equals('stable');
    expectMediaWith(sdpInfo.medias[0], { id: 0, port: 9, type: 'audio', direction: 'recvonly', setup: 'active' });
    expectMediaWith(sdpInfo.medias[1], { id: 1, port: 9, type: 'video', direction: 'recvonly', setup: 'active' });
    for (let index = 2; index <= kNumberOfPublishers; index += 1) {
      expectMediaWith(sdpInfo.medias[(index * 2) - 2], { id: (index * 2) - 2, port: 9, type: 'audio', direction: 'recvonly', setup: 'active' });
      expectMediaWith(sdpInfo.medias[(index * 2) - 1], { id: (index * 2) - 1, port: 9, type: 'video', direction: 'recvonly', setup: 'active' });
    }
    createOffer.reset();
    getLocalDescription.reset();
  });

  it('should negotiate AV with multiple receiving streams', async () => {
    const kNumberOfSubscribers = 10;
    const config = Object.assign({}, webRtcConnectionConfiguration);
    const publisherSdpInfo = SemanticSdp.SDPInfo.processString(sdpUtils.getChromePublisherSdp([
      { mid: 0, label: '1', kind: 'audio', direction: 'recvonly', setup: 'active' },
      { mid: 1, label: '1', kind: 'video', direction: 'recvonly', setup: 'active' },
    ]));
    const publisherDescription = new SessionDescription(publisherSdpInfo, 'default');
    connection = new RTCPeerConnection(config);
    const originalCreateOffer = connection.internalConnection.wrtc
      .createOffer.bind(connection.internalConnection.wrtc);
    const createOffer = sinon.stub(connection.internalConnection.wrtc, 'createOffer');
    const originalGetLocalDescription = connection.internalConnection.wrtc
      .getLocalDescription.bind(connection.internalConnection.wrtc);
    const getLocalDescription = sinon.stub(connection.internalConnection.wrtc, 'getLocalDescription');
    createOffer.callsFake(bundle => new Promise((resolve) => {
      setTimeout(async () => {
        await originalCreateOffer(bundle);
        resolve();
      }, 20);
    }));
    getLocalDescription.callsFake(() => new Promise((resolve) => {
      setTimeout(async () => {
        const sdp = await originalGetLocalDescription();
        resolve(sdp);
      }, 20);
    }));
    connection.init();
    const label = 'stream10';

    await connection.onInitialized;
    let onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');

    await connection.addStream('1', { label, audio: true, video: true }, false);
    connection.internalConnection.wrtc
      .copySdpToLocalDescription(publisherDescription.connectionDescription);
    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');
    const promises = [];
    promises.push(connection.setLocalDescription());
    for (let index = 2; index <= kNumberOfSubscribers; index += 1) {
      const label2 = `stream${index}`;
      // eslint-disable-next-line no-loop-func
      promises.push(new Promise((resolve) => {
        setTimeout(() => {
          connection.addStream(index, { label: label2, audio: true, video: true }, false)
            .then(() => {
              connection.internalConnection.wrtc
                .copySdpToLocalDescription(publisherDescription.connectionDescription);
              resolve();
            });
        }, 30);
      }));
    }
    await Promise.all(promises);
    let offer = connection.localDescription;
    let sdpInfo = SemanticSdp.SDPInfo.processString(offer);

    expect(sdpInfo.medias.length).to.be.equals(2);
    expectMediaWith(sdpInfo.medias[0], { id: 0, port: 9, type: 'audio', direction: 'sendrecv', setup: 'actpass' });
    expectMediaWith(sdpInfo.medias[1], { id: 1, port: 9, type: 'video', direction: 'sendrecv', setup: 'actpass' });

    expect(connection.signalingState).to.be.equals('have-local-offer');
    onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');
    await connection.setRemoteDescription({
      type: 'answer',
      sdp: sdpUtils.getChromePublisherSdp([
        { mid: 0, label, kind: 'audio', direction: 'recvonly', setup: 'active' },
        { mid: 1, label, kind: 'video', direction: 'recvonly', setup: 'active' },
      ]),
    });

    expect(connection.signalingState).to.be.equals('stable');
    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    await connection.setLocalDescription();
    offer = connection.localDescription;
    sdpInfo = SemanticSdp.SDPInfo.processString(offer);

    expect(sdpInfo.medias.length).to.be.equals(kNumberOfSubscribers * 2);
    expect(connection.signalingState).to.be.equals('have-local-offer');
    expectMediaWith(sdpInfo.medias[0], { id: 0, port: 9, type: 'audio', direction: 'sendrecv', setup: 'passive' });
    expectMediaWith(sdpInfo.medias[1], { id: 1, port: 9, type: 'video', direction: 'sendrecv', setup: 'passive' });
    for (let index = 2; index <= kNumberOfSubscribers; index += 1) {
      expectMediaWith(sdpInfo.medias[(index * 2) - 2], { id: (index * 2) - 2, port: 9, type: 'audio', direction: 'sendrecv', setup: 'actpass' });
      expectMediaWith(sdpInfo.medias[(index * 2) - 1], { id: (index * 2) - 1, port: 9, type: 'video', direction: 'sendrecv', setup: 'actpass' });
    }
    createOffer.reset();
    getLocalDescription.reset();
  });

  it('should fail when setting a remote offer when there is a local offer', async () => {
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();

    await connection.onInitialized;
    const onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');

    await connection.addStream(1, { audio: true, video: true }, false, true);
    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setLocalDescription();

    expect(connection.signalingState).to.be.equals('have-local-offer');

    const srdPromise = connection.setRemoteDescription({ type: 'offer', sdp: audioPlusVideo('sendrecv') });
    srdPromise.catch(() => {});

    await expect(srdPromise).to.be.eventually.rejectedWith('Rollback needed and not implemented');
    expect(connection.signalingState).to.be.equals('have-local-offer');
  });

  it('should fail when setting a remote answer when state is stable', async () => {
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();

    await connection.onInitialized;

    expect(connection.signalingState).to.be.equals('stable');
    const srdPromise = connection.setRemoteDescription({ type: 'answer', sdp: audioPlusVideoReceiver('recvonly') });

    await expect(srdPromise).to.be.eventually.rejectedWith('Rollback needed and not implemented');
    expect(connection.signalingState).to.be.equals('stable');
  });

  it('should success when setting a remote offer when state is have-remote-offer', async () => {
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();

    await connection.onInitialized;

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setRemoteDescription({ type: 'offer', sdp: audioPlusVideo('sendrecv') });

    expect(connection.signalingState).to.be.equals('have-remote-offer');

    const srdPromise = connection.setRemoteDescription({ type: 'offer', sdp: audioPlusVideo('sendrecv') });
    await expect(srdPromise).to.be.eventually.fulfilled;
    expect(connection.signalingState).to.be.equals('have-remote-offer');
  });

  it('should success when setting a local offer when state is have-local-offer', async () => {
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();

    await connection.onInitialized;

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setLocalDescription();

    expect(connection.signalingState).to.be.equals('have-local-offer');

    const srdPromise = connection.setLocalDescription();
    await expect(srdPromise).to.be.eventually.fulfilled;
    expect(connection.signalingState).to.be.equals('have-local-offer');
  });

  it('should fail when creating a local answer when state is stable', async () => {
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();

    await connection.onInitialized;

    expect(connection.signalingState).to.be.equals('stable');

    const caPromise = connection.createAnswer();
    await expect(caPromise).to.be.eventually.rejectedWith('InvalidStateError');
    expect(connection.signalingState).to.be.equals('stable');
  });

  it('should fail when creating a local answer when state is stable', async () => {
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();

    await connection.onInitialized;

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setLocalDescription();

    expect(connection.signalingState).to.be.equals('have-local-offer');

    const caPromise = connection.createAnswer();
    await expect(caPromise).to.be.eventually.rejectedWith('InvalidStateError');
    expect(connection.signalingState).to.be.equals('have-local-offer');
  });

  it('should fail calls to its API when connection is already closed', async () => {
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();
    await connection.close();
    expect(connection.signalingState).to.be.equals('closed');
    expect(connection.isClosed).to.be.true;

    let promise = connection.createOffer();
    await expect(promise).to.be.eventually.rejectedWith('InvalidStateError');

    promise = connection.createAnswer();
    await expect(promise).to.be.eventually.rejectedWith('InvalidStateError');

    promise = connection.setLocalDescription();
    await expect(promise).to.be.eventually.rejectedWith('InvalidStateError');

    promise = connection.setRemoteDescription({ type: 'offer', sdp: '' });
    await expect(promise).to.be.eventually.rejectedWith('InvalidStateError');

    promise = connection.addStream(1, { audio: true, video: true }, false, true);
    await expect(promise).to.be.eventually.rejectedWith('InvalidStateError');

    promise = connection.removeStream(1);
    await expect(promise).to.be.eventually.rejectedWith('InvalidStateError');
  });

  it('should fail calls to its API when connection is closing', async () => {
    const test = async (operation, ...args) => {
      connection = new RTCPeerConnection(webRtcConnectionConfiguration);
      connection.init();
      expect(connection.signalingState).to.be.equals('stable');
      const promise = connection[operation](...args);
      await connection.close();
      expect(connection.isClosed).to.be.true;
      let value;
      try {
        value = await promise;
      } catch (e) {
        value = e;
      }
      return value;
    };

    let result = await test('createOffer');
    expect(result).to.be.equals('InvalidStateError');

    result = await test('createOffer');
    expect(result).to.be.equals('InvalidStateError');

    result = await test('setLocalDescription');
    expect(result).to.be.equals('InvalidStateError');

    result = await test('setRemoteDescription', { type: 'offer', sdp: audioPlusVideo('sendrecv') });
    expect(result).to.be.equals('InvalidStateError');

    result = await test('addStream', 1, { audio: true, video: true }, false, true);
    expect(result).to.be.equals(undefined);

    result = await test('removeStream', 1);
    expect(result).to.be.equals(undefined);
  });

  // Test that negogiation flag is not updated until it finishes negotiation steps
  it('should trigger negotiationneeded only when signaling state becomes stable', async () => {
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();
    const label = 'stream1';
    await connection.onInitialized;

    await connection.addStream(1, { label, audio: true, video: true }, true, true);

    await connection.setRemoteDescription({ type: 'offer', sdp: audioPlusVideo('sendonly') });

    expect(connection.signalingState).to.be.equals('have-remote-offer');
    expect(connection.negotiationNeeded).to.be.false;
    await connection.addStream(2, { label: 'other', audio: true, video: true }, false, true);
    await timeout(20);
    expect(connection.negotiationNeeded).to.be.false;
    const onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');
    await connection.setLocalDescription();
    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;
    const answer = connection.localDescription;
    const sdpInfo = SemanticSdp.SDPInfo.processString(answer);
    expect(sdpInfo.medias.length).to.be.equals(2);
    expect(sdpInfo.getStreams().size).to.be.equals(0);
    expect(sdpInfo.medias[0].id).to.be.equals(0);
    expect(sdpInfo.medias[0].type).to.be.equals('audio');
    expect(sdpInfo.medias[0].getDirectionString()).to.be.equals('recvonly');
    expect(sdpInfo.medias[1].id).to.be.equals(1);
    expect(sdpInfo.medias[1].type).to.be.equals('video');
    expect(sdpInfo.medias[1].getDirectionString()).to.be.equals('recvonly');
  });
});
