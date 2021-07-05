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

const audioPlusVideo = direction => sdpUtils.getChromePublisherSdp([
  { mid: 0, label: 'stream1', ssrc1: '00000', kind: 'audio', direction },
  { mid: 1, label: 'stream1', ssrc1: '1111', ssrc2: '1112', kind: 'video', direction }]);

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

describe('RTCPeerConnection', () => {
  let RTCPeerConnection;
  let connection;
  let webRtcConnectionConfiguration;
  let erizoApiMock;
  let licodeConfigMock;

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
    licodeConfigMock = mocks.start(mocks.licodeConfig);
    erizoApiMock = mocks.start(mocks.erizoAPI);
    // eslint-disable-next-line global-require
    RTCPeerConnection = require('../../erizoJS/models/RTCPeerConnection');
    webRtcConnectionConfiguration = {
      trickleIce: false,
      mediaConfiguration: 'default',
      id: 'connection1',
      erizoControllerId: 'erizoControllerTest1',
      clientId: 'clientTest1',
      options: {
      },
    };
  });

  afterEach(() => {
    mocks.stop(erizoApiMock);
    mocks.stop(licodeConfigMock);
    mocks.deleteRequireCache();
    mocks.reset();
    global.config = {};
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

    await connection.addStream(1, {}, false, true);
    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;
  });

  it('should finish negotiation successfully when adding and removing remote streams', async () => {
    mocks.WebRtcConnection.init.returns(1).callsArgWith(0, 103, ''); // CONN_GATHERED
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();
    const label = 'stream1';
    await connection.onInitialized;

    await connection.addStream(1, { label }, true, true);

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setRemoteDescription({ type: 'offer', sdp: audioPlusVideo('sendrecv') });
    expect(connection.signalingState).to.be.equals('have-remote-offer');

    await connection.setLocalDescription();

    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    await connection.removeStream(1);
    expect(connection.signalingState).to.be.equals('stable');
    await connection.setRemoteDescription({ type: 'offer', sdp: audioPlusVideo('recvonly') });
    expect(connection.signalingState).to.be.equals('have-remote-offer');

    await connection.setLocalDescription();
    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    await connection.addStream(1, { label }, true, true);

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setRemoteDescription({ type: 'offer', sdp: audioPlusVideo('sendrecv') });
    expect(connection.signalingState).to.be.equals('have-remote-offer');

    await connection.setLocalDescription();
    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;
  });

  it('should finish negotiation successfully when adding and removing remote streams (with createAnswer)', async () => {
    mocks.WebRtcConnection.init.returns(1).callsArgWith(0, 103, ''); // CONN_GATHERED
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();
    const label = 'stream1';
    await connection.onInitialized;

    await connection.addStream(1, { label }, true, true);

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setRemoteDescription({ type: 'offer', sdp: audioPlusVideo('sendrecv') });
    expect(connection.signalingState).to.be.equals('have-remote-offer');

    const answer1 = await connection.createAnswer();
    await connection.setLocalDescription(answer1);

    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    await connection.removeStream(1);
    expect(connection.signalingState).to.be.equals('stable');
    await connection.setRemoteDescription({ type: 'offer', sdp: audioPlusVideo('recvonly') });
    expect(connection.signalingState).to.be.equals('have-remote-offer');

    const answer2 = await connection.createAnswer();
    await connection.setLocalDescription(answer2);
    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    await connection.addStream(1, { label }, true, true);

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setRemoteDescription({ type: 'offer', sdp: audioPlusVideo('sendrecv') });
    expect(connection.signalingState).to.be.equals('have-remote-offer');

    const answer3 = await connection.createAnswer();
    await connection.setLocalDescription(answer3);

    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;
  });

  it('should finish negotiation successfully when adding and removing streams', async () => {
    mocks.WebRtcConnection.init.returns(1).callsArgWith(0, 103, ''); // CONN_GATHERED
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();
    const label = 'stream10';

    await connection.onInitialized;
    let onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');

    await connection.addStream(1, { label }, false, true);
    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setLocalDescription();

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

    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({ type: 'answer', sdp: audioPlusVideo('inactive') });

    expect(connection.signalingState).to.be.equals('stable');

    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');
    await connection.addStream(1, { label }, false, true);

    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;

    expect(connection.signalingState).to.be.equals('stable');
    await connection.setLocalDescription();

    expect(connection.signalingState).to.be.equals('have-local-offer');

    await connection.setRemoteDescription({ type: 'answer', sdp: audioPlusVideoReceiver('recvonly') });

    expect(connection.signalingState).to.be.equals('stable');

    expect(connection.negotiationNeeded).to.be.false;
  });

  it('should fail when setting a remote offer when there is a local offer', async () => {
    mocks.WebRtcConnection.init.returns(1).callsArgWith(0, 103, ''); // CONN_GATHERED
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();

    await connection.onInitialized;
    const onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');

    await connection.addStream(1, {}, false, true);
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
    mocks.WebRtcConnection.init.returns(1).callsArgWith(0, 103, ''); // CONN_GATHERED
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
    mocks.WebRtcConnection.init.returns(1).callsArgWith(0, 103, ''); // CONN_GATHERED
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

    promise = connection.addStream(1, {}, false, true);
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
    expect(result).to.be.an.instanceOf(Error);
    expect(result.message).to.be.equals('InvalidStateError');

    result = await test('createOffer');
    expect(result).to.be.an.instanceOf(Error);
    expect(result.message).to.be.equals('InvalidStateError');

    result = await test('setLocalDescription');
    expect(result).to.be.an.instanceOf(Error);
    expect(result.message).to.be.equals('InvalidStateError');

    result = await test('setRemoteDescription', { type: 'offer', sdp: audioPlusVideo('sendrecv') });
    expect(result).to.be.an.instanceOf(Error);
    expect(result.message).to.be.equals('InvalidStateError');

    result = await test('addStream', 1, {}, false, true);
    expect(result).to.be.equals(undefined);

    result = await test('removeStream', 1);
    expect(result).to.be.equals(undefined);
  });

  // Test that negogiation flag is not updated until it finishes negotiation steps
  it('should trigger negotiationneeded only when signaling state becomes stable', async () => {
    mocks.WebRtcConnection.init.returns(1).callsArgWith(0, 103, ''); // CONN_GATHERED
    connection = new RTCPeerConnection(webRtcConnectionConfiguration);
    connection.init();
    const label = 'stream1';
    await connection.onInitialized;

    await connection.addStream(1, { label }, true, true);

    await connection.setRemoteDescription({ type: 'offer', sdp: audioPlusVideo('sendonly') });

    expect(connection.signalingState).to.be.equals('have-remote-offer');
    expect(connection.negotiationNeeded).to.be.false;
    await connection.addStream(2, { label: 'other' }, false, true);
    await timeout(20);
    expect(connection.negotiationNeeded).to.be.false;
    const onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');
    await connection.setLocalDescription();
    expect(connection.signalingState).to.be.equals('stable');
    expect(connection.negotiationNeeded).to.be.false;

    await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
    expect(connection.negotiationNeeded).to.be.true;
  });

  describe('operations chain', () => {
    beforeEach(async () => {
      connection = new RTCPeerConnection(webRtcConnectionConfiguration);
      connection.init();

      await connection.onInitialized;
    });

    it('should execute the first operation', async () => {
      const promise = connection.addToOperationChain(timeout.bind(null, 10));
      expect(connection.operations.length).to.be.equals(1);
      await promise;
      expect(connection.operations.length).to.be.equals(0);
    });

    it('should chain and execute a second operation', async () => {
      const promise1 = connection.addToOperationChain(timeout.bind(null, 10));
      expect(connection.operations.length).to.be.equals(1);
      const promise2 = connection.addToOperationChain(timeout.bind(null, 10));
      expect(connection.operations.length).to.be.equals(2);
      await promise1;
      expect(connection.operations.length).to.be.equals(1);
      await promise2;
      expect(connection.operations.length).to.be.equals(0);
    });

    it('should update the negotiation needed flag after last operation', async () => {
      const onNegotiationNeededPromise = promisifyOnce(connection, 'negotiationneeded');

      expect(connection.negotiationNeeded).to.be.false;

      const promise2 = connection.addToOperationChain(timeout.bind(null, 10));
      const promise1 = connection.addStream(1, {}, false, true);
      expect(connection.operations.length).to.be.equals(1);
      const promise3 = connection.addToOperationChain(timeout.bind(null, 10));
      await promise1;
      expect(connection.operations.length).to.be.equals(2);
      await promise2;
      expect(connection.operations.length).to.be.equals(1);
      await promise3;

      expect(connection.negotiationNeeded).to.be.false;
      await expect(onNegotiationNeededPromise).to.be.eventually.fulfilled;
      expect(connection.operations.length).to.be.equals(0);
      expect(connection.negotiationNeeded).to.be.true;
    });

    it('should not execute an operation if connection is closed', async () => {
      await connection.close();
      const promise = connection.addToOperationChain(timeout.bind(null, 10));
      expect(connection.operations.length).to.be.equals(0);
      await expect(promise).to.eventually.be.rejectedWith('InvalidStateError');
    });

    it('should not execute an operation if connection is being closed', async () => {
      const promise1 = connection.addToOperationChain(timeout.bind(null, 10));
      connection.close();
      expect(connection.operations.length).to.be.equals(1);
      const promise2 = connection.addToOperationChain(timeout.bind(null, 10));
      expect(connection.operations.length).to.be.equals(1);
      await expect(promise2).to.be.eventually.rejectedWith('InvalidStateError');
      await expect(promise1).to.be.eventually.rejectedWith('InvalidStateError');
    });
  });
});
