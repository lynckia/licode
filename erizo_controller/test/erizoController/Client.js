/* global require, describe, it, beforeEach, afterEach */

/* eslint-disable no-unused-expressions */

const mocks = require('../utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;

global.config = { logger: { configFile: true } };
const Permission = require('../../erizoController/permission');

describe('Erizo Controller / Client', () => {
  let roomMock;
  let licodeConfigMock;
  let streamManagerMock;
  let roomControllerMock;
  let streamMock;
  let channelMock;
  let optionsMock;
  let tokenMock;
  let clientMock;
  let client;
  let ClientExport;

  beforeEach(() => {
    optionsMock = {
      p2p: false,
      data: true,
    };
    tokenMock = {
      userName: 'user1',
      role: 'testUser',
      mediaConfiguration: {},
    };
    licodeConfigMock = mocks.start(mocks.licodeConfig);
    licodeConfigMock.erizoController.roles = {
      testUser: {
        publish: true,
        subscribe: true,
        record: true,
        stats: true,
        controlHandlers: true,
      },
    };
    global.config = licodeConfigMock;
    streamMock = mocks.start(mocks.Stream);
    streamManagerMock = mocks.start(mocks.StreamManager);
    clientMock = mocks.start(mocks.Client);
    roomControllerMock = mocks.start(mocks.roomControllerInstance);
    roomMock = mocks.start(mocks.Room);
    roomMock.streamManager = streamManagerMock;
    roomMock.controller = roomControllerMock;
    channelMock = mocks.start(mocks.Channel);
    // eslint-disable-next-line global-require
    ClientExport = require('../../erizoController/models/Client').Client;
    client = new ClientExport(channelMock, tokenMock, optionsMock, roomMock);
  });

  afterEach(() => {
    mocks.stop(licodeConfigMock);
    mocks.stop(streamManagerMock);
    mocks.stop(clientMock);
    mocks.stop(roomControllerMock);
    mocks.stop(roomMock);
    mocks.stop(channelMock);
    mocks.stop(streamMock);
    optionsMock = {};
    tokenMock = {};
    mocks.deleteRequireCache();
    mocks.reset();
    global.config = { logger: { configFile: true } };
  });

  it('should have a known API', () => {
    expect(client.sendMessage).not.to.be.undefined;
    expect(client.on).not.to.be.undefined;
    expect(client.setNewChannel).not.to.be.undefined;
    expect(client.removeSubscriptions).not.to.be.undefined;
    expect(client.disconnect).not.to.be.undefined;
  });

  describe('onPublish', () => {
    const kArbitraryLabel = 'label1';
    let callback;
    let options;
    let sdp;

    beforeEach(() => {
      callback = sinon.stub();
      options = {
        audio: true,
        video: true,
        data: true,
        label: kArbitraryLabel,
        screen: false,
        attributes: {},
      };
      sdp = '';
    });

    it('should fail if user has no permissions', () => {
      client.user.permissions[Permission.PUBLISH] = undefined;

      client.onPublish(options, sdp, callback);
      expect(callback.callCount).to.equal(1);
      expect(callback.args[0][1]).to.equal('Unauthorized');
    });
    describe('p2p', () => {
      beforeEach(() => {
        roomMock.p2p = true;
      });
      it('should create stream and notify client', () => {
        client.onPublish(options, sdp, callback);
        expect(callback.callCount).to.equal(1);
        expect(streamManagerMock.addPublishedStream.callCount).to.equal(1);
        expect(roomMock.sendMessage.callCount).to.equal(1);
        expect(roomMock.sendMessage.args[0][0]).to.equal('onAddStream');
      });
    });
    describe('externalInput', () => {
      beforeEach(() => {
        options = {
          state: 'recording',
        };
      });
      it('should update the stream and send event to clients if addExternalInput succeeds', () => {
        roomControllerMock.addExternalInput.callsArgWith(4, 'success');
        client.onPublish(options, sdp, callback);
        expect(callback.callCount).to.equal(1);
        expect(mocks.PublishedStream.updateStreamState.args[0][0])
          .equal(streamMock.StreamStates.PUBLISHER_READY);
        expect(streamManagerMock.addPublishedStream.callCount).to.equal(1);
        expect(roomMock.sendMessage.callCount).to.equal(1);
        expect(roomMock.sendMessage.args[0][0]).to.equal('onAddStream');
      });
      it('should delete the stream and notify error if it does not succeed', () => {
        const kArbitraryErrorMessage = 'error';
        roomControllerMock.addExternalInput.callsArgWith(4, kArbitraryErrorMessage);
        client.onPublish(options, sdp, callback);
        expect(callback.callCount).to.equal(1);
        expect(callback.args[0][1]).to.equal(`Error adding External Input:${kArbitraryErrorMessage}`);
        expect(streamManagerMock.addPublishedStream.callCount).to.equal(1);
        expect(streamManagerMock.removePublishedStream.callCount).to.equal(1);
        expect(roomMock.sendMessage.callCount).to.equal(0);
      });
    });
    describe('erizo', () => {
      beforeEach(() => {
        options = {
          state: 'erizo',
        };
      });
      it('should update the stream and callback when stream is initializing', () => {
        mocks.StreamManager.hasPublishedStream.returns(true);
        roomControllerMock.addPublisher.callsArgWith(3, { type: 'initializing' });
        client.onPublish(options, sdp, callback);
        expect(callback.callCount).to.equal(1);
        expect(streamManagerMock.addPublishedStream.callCount).to.equal(1);
        expect(streamMock.PublishedStream.callCount).to.equal(1);
        expect(mocks.PublishedStream.updateStreamState.args[0][0])
          .equal(streamMock.StreamStates.PUBLISHER_INITAL);
        expect(roomMock.sendMessage.callCount).to.equal(0);
      });
      it('should update the stream and send event to clients when stream is ready', () => {
        mocks.StreamManager.hasPublishedStream.returns(true);
        roomControllerMock.addPublisher.callsArgWith(3, { type: 'ready' });
        client.onPublish(options, sdp, callback);
        expect(callback.callCount).to.equal(0);
        expect(streamManagerMock.addPublishedStream.callCount).to.equal(1);
        expect(mocks.PublishedStream.updateStreamState.args[0][0])
          .equal(streamMock.StreamStates.PUBLISHER_READY);
        expect(roomMock.sendMessage.callCount).to.equal(1);
        expect(roomMock.sendMessage.args[0][0]).to.equal('onAddStream');
      });
      it('should delete the stream and notify error if it does not succeed', () => {
        mocks.StreamManager.hasPublishedStream.returns(true);
        roomControllerMock.addPublisher.callsArgWith(3, { type: 'failed' });
        client.onPublish(options, sdp, callback);
        expect(callback.callCount).to.equal(0);
        expect(streamManagerMock.removePublishedStream.callCount).to.equal(1);
        expect(channelMock.sendMessage.callCount).to.equal(1);
        expect(channelMock.sendMessage.args[0][0]).to.equal('connection_failed');
      });
    });
  });

  describe('onUnpublish', () => {
    const kArbitraryStreamId = 'stream1';
    let callback;

    beforeEach(() => {
      callback = sinon.stub();
      streamManagerMock.getPublishedStreamById.returns(mocks.PublishedStream);
      mocks.PublishedStream.hasData.returns(true);
      mocks.PublishedStream.hasAudio.returns(true);
      mocks.PublishedStream.hasVideo.returns(true);
      mocks.PublishedStream.hasScreen.returns(true);
    });

    it('should fail if user has no permissions', () => {
      client.user.permissions[Permission.PUBLISH] = undefined;

      client.onUnpublish(kArbitraryStreamId, callback);
      expect(callback.callCount).to.equal(1);
      expect(callback.args[0][1]).to.equal('Unauthorized');
    });
    it('should fail if stream does not exist', () => {
      streamManagerMock.getPublishedStreamById.returns(undefined);
      client.onUnpublish(kArbitraryStreamId, callback);
      expect(callback.callCount).to.equal(0);
      expect(streamManagerMock.removePublishedStream.callCount).to.equal(0);
    });
    it('should mark the stream as being closed before the callback', () => {
      client.onUnpublish(kArbitraryStreamId, callback);
      expect(streamManagerMock.removePublishedStream.callCount).to.equal(0);
      expect(mocks.PublishedStream.updateStreamState.callCount).to.equal(1);
      expect(mocks.PublishedStream.updateStreamState.args[0][0])
        .equal(streamMock.StreamStates.PUBLISHER_REQUESTED_CLOSE);
      expect(roomMock.sendMessage.callCount).to.equal(0);
    });
    describe('p2p', () => {
      beforeEach(() => {
        roomMock.p2p = true;
      });
      it('should remove stream and notify clients', () => {
        client.onUnpublish(kArbitraryStreamId, callback);
        expect(callback.callCount).to.equal(1);
        expect(callback.args[0][0]).to.equal(true);
        expect(streamManagerMock.removePublishedStream.callCount).to.equal(1);
        expect(roomMock.sendMessage.callCount).to.equal(1);
        expect(roomMock.sendMessage.args[0][0]).to.equal('onRemoveStream');
      });
    });
    describe('erizo', () => {
      it('should remove the stream and notify the client', () => {
        roomControllerMock.removePublisher.callsArg(2);
        client.onUnpublish(kArbitraryStreamId, callback);
        expect(callback.callCount).to.equal(1);
        expect(streamManagerMock.removePublishedStream.callCount).to.equal(1);
        expect(mocks.PublishedStream.updateStreamState.args[0][0])
          .equal(streamMock.StreamStates.PUBLISHER_REQUESTED_CLOSE);
        expect(roomMock.sendMessage.callCount).to.equal(1);
        expect(roomMock.sendMessage.args[0][0]).to.equal('onRemoveStream');
      });
    });
  });

  describe('onSubscribe', () => {
    const kArbitraryLabel = 'label1';
    let callback;
    let options;
    let sdp;

    beforeEach(() => {
      callback = sinon.stub();
      options = {
        audio: true,
        video: true,
        data: true,
        label: kArbitraryLabel,
        screen: false,
        attributes: {},
      };
      streamManagerMock.getPublishedStreamById.returns(mocks.PublishedStream);
      mocks.PublishedStream.hasData.returns(true);
      mocks.PublishedStream.hasAudio.returns(true);
      mocks.PublishedStream.hasVideo.returns(true);
      mocks.PublishedStream.hasScreen.returns(true);
      sdp = '';
    });

    it('should fail if user has no permissions', () => {
      client.user.permissions[Permission.SUBSCRIBE] = undefined;

      client.onSubscribe(options, sdp, callback);
      expect(callback.callCount).to.equal(1);
      expect(callback.args[0][1]).to.equal('Unauthorized');
    });
    it('should subscribe to data if requested and available', () => {
      client.onSubscribe(options, sdp, callback);
      expect(mocks.PublishedStream.addDataSubscriber.callCount).to.equal(1);
    });
    it('should not subscribe to data if it is not requested', () => {
      options.data = false;
      client.onSubscribe(options, sdp, callback);
      expect(mocks.PublishedStream.addDataSubscriber.callCount).to.equal(0);
    });
    it('should fail if stream does not exist', () => {
      streamManagerMock.getPublishedStreamById.returns(undefined);
      client.onSubscribe(options, sdp, callback);
      expect(callback.callCount).to.equal(0);
    });
    it('should not add avSubscriber if stream has no video, audio or screen', () => {
      mocks.PublishedStream.hasAudio.returns(false);
      mocks.PublishedStream.hasVideo.returns(false);
      mocks.PublishedStream.hasScreen.returns(false);
      expect(mocks.PublishedStream.addAvSubscriber.callCount).to.equal(0);
      client.onSubscribe(options, sdp, callback);
      expect(callback.callCount).to.equal(1);
    });
    describe('p2p', () => {
      beforeEach(() => {
        roomMock.getClientById.returns(mocks.Client);
        roomMock.p2p = true;
      });
      it('should notify client to publish, add and update stream', () => {
        client.onSubscribe(options, sdp, callback);
        expect(mocks.PublishedStream.addAvSubscriber.callCount).to.equal(1);
        expect(mocks.PublishedStream.addAvSubscriber.args[0][0]).to.equal(client.id);
        expect(mocks.PublishedStream.updateAvSubscriberState.callCount).to.equal(1);
        expect(mocks.PublishedStream.updateAvSubscriberState.args[0][1])
          .to.equal(streamMock.StreamStates.SUBSCRIBER_READY);
        expect(mocks.Client.sendMessage.callCount).to.equal(1);
        expect(mocks.Client.sendMessage.args[0][0]).to.equal('publish_me');
      });
    });
    describe('erizo', () => {
      beforeEach(() => {
      });
      it('should do nothing if subscription already exists', () => {
        mocks.PublishedStream.hasAvSubscriber.returns(true);
        client.onSubscribe(options, sdp, callback);
        expect(callback.callCount).to.equal(0);
        expect(roomControllerMock.addSubscriber.callCount).to.equal(0);
        expect(roomMock.sendMessage.callCount).to.equal(0);
      });
      it('should update stream when state is initializing', () => {
        mocks.PublishedStream.hasAvSubscriber.onCall(1).returns(true);
        mocks.StreamManager.hasPublishedStream.returns(true);
        roomControllerMock.addSubscriber.callsArgWith(3, { type: 'initializing' });
        client.onSubscribe(options, sdp, callback);
        expect(mocks.PublishedStream.addAvSubscriber.callCount).to.equal(1);
        expect(mocks.PublishedStream.addAvSubscriber.args[0][0]).to.equal(client.id);
        expect(mocks.PublishedStream.updateAvSubscriberState.callCount).to.equal(1);
        expect(mocks.PublishedStream.updateAvSubscriberState.args[0][1])
          .to.equal(streamMock.StreamStates.SUBSCRIBER_INITIAL);
        expect(callback.callCount).to.equal(1);
      });
      it('should update the stream and not issue callback when stream is ready', () => {
        mocks.PublishedStream.hasAvSubscriber.onCall(1).returns(true);
        mocks.StreamManager.hasPublishedStream.returns(true);
        roomControllerMock.addSubscriber.callsArgWith(3, { type: 'ready' });
        client.onSubscribe(options, sdp, callback);
        expect(mocks.PublishedStream.addAvSubscriber.callCount).to.equal(1);
        expect(mocks.PublishedStream.addAvSubscriber.args[0][0]).to.equal(client.id);
        expect(mocks.PublishedStream.updateAvSubscriberState.callCount).to.equal(1);
        expect(mocks.PublishedStream.updateAvSubscriberState.args[0][1])
          .to.equal(streamMock.StreamStates.SUBSCRIBER_READY);
        expect(callback.callCount).to.equal(0);
      });
      it('should delete the stream and notify error if it fails', () => {
        mocks.PublishedStream.hasAvSubscriber.onCall(1).returns(true);
        mocks.StreamManager.hasPublishedStream.returns(true);
        roomControllerMock.addSubscriber.callsArgWith(3, { type: 'failed' });
        client.onSubscribe(options, sdp, callback);
        expect(mocks.PublishedStream.addAvSubscriber.callCount).to.equal(1);
        expect(mocks.PublishedStream.addAvSubscriber.args[0][0]).to.equal(client.id);
        expect(mocks.PublishedStream.removeAvSubscriber.callCount).to.equal(1);
        expect(mocks.PublishedStream.removeAvSubscriber.args[0][0]).to.equal(client.id);
        expect(channelMock.sendMessage.callCount).to.equal(1);
        expect(callback.callCount).to.equal(0);
      });
      it('should delete the stream and notify via callback if erizo times out', () => {
        mocks.PublishedStream.hasAvSubscriber.onCall(1).returns(true);
        mocks.StreamManager.hasPublishedStream.returns(true);
        roomControllerMock.addSubscriber.callsArgWith(3, 'timeout');
        client.onSubscribe(options, sdp, callback);
        expect(mocks.PublishedStream.addAvSubscriber.callCount).to.equal(1);
        expect(mocks.PublishedStream.addAvSubscriber.args[0][0]).to.equal(client.id);
        expect(mocks.PublishedStream.removeAvSubscriber.callCount).to.equal(1);
        expect(mocks.PublishedStream.removeAvSubscriber.args[0][0]).to.equal(client.id);
        expect(callback.callCount).to.equal(1);
        expect(callback.args[0][2]).to.equal('ErizoJS is not reachable');
      });
    });
  });
  describe('onUnsubscribe', () => {
    const kArbitraryStreamId = 'stream1';
    let callback;

    beforeEach(() => {
      callback = sinon.stub();
      streamManagerMock.getPublishedStreamById.returns(mocks.PublishedStream);
      mocks.PublishedStream.hasAvSubscriber.returns(true);
      mocks.PublishedStream.hasData.returns(true);
      mocks.PublishedStream.hasAudio.returns(true);
      mocks.PublishedStream.hasVideo.returns(true);
      mocks.PublishedStream.hasScreen.returns(true);
    });

    it('should fail if user has no permissions', () => {
      client.user.permissions[Permission.SUBSCRIBE] = undefined;
      client.onUnsubscribe(kArbitraryStreamId, callback);
      expect(callback.callCount).to.equal(1);
      expect(callback.args[0][1]).to.equal('Unauthorized');
    });
    it('should do nothing if publisher does not exist', () => {
      streamManagerMock.getPublishedStreamById.returns(undefined);
      client.onUnsubscribe(kArbitraryStreamId, callback);
      expect(callback.callCount).to.equal(0);
    });
    it('should only remove data if publisher does not have audio, video, or screen', () => {
      mocks.PublishedStream.hasAudio.returns(false);
      mocks.PublishedStream.hasVideo.returns(false);
      mocks.PublishedStream.hasScreen.returns(false);
      client.onUnsubscribe(kArbitraryStreamId, callback);
      expect(mocks.PublishedStream.removeDataSubscriber.callCount).to.equal(1);
      expect(mocks.PublishedStream.removeAvSubscriber.callCount).to.equal(0);
      expect(callback.callCount).to.equal(0);
    });
    it('should mark subscriber as being removed before executing unsubscribe', () => {
      client.onUnsubscribe(kArbitraryStreamId, callback);
      expect(mocks.PublishedStream.updateAvSubscriberState.callCount).to.equal(1);
      expect(mocks.PublishedStream.updateAvSubscriberState.args[0][1])
        .to.equal(streamMock.StreamStates.SUBSCRIBER_REQUESTED_CLOSE);
      expect(callback.callCount).to.equal(0);
    });
    describe('p2p', () => {
      beforeEach(() => {
        roomMock.p2p = true;
        roomMock.getClientById.returns(mocks.Client);
      });
      it('should remove subscribed stream and notify client ', () => {
        client.onUnsubscribe(kArbitraryStreamId, callback);
        expect(mocks.PublishedStream.removeAvSubscriber.callCount).to.equal(1);
        expect(clientMock.sendMessage.callCount).to.equal(1);
        expect(clientMock.sendMessage.args[0][0]).to.equal('unpublish_me');
        expect(callback.callCount).to.equal(1);
      });
    });
    describe('erizo', () => {
      it('should remove stream  and call callback on roomController callback', () => {
        const kArbitraryResult = 'arbitraryResult';
        roomControllerMock.removeSubscriber.callsArgWith(2, kArbitraryResult);
        client.onUnsubscribe(kArbitraryStreamId, callback);
        expect(mocks.PublishedStream.removeAvSubscriber.callCount).to.equal(1);
        expect(roomControllerMock.removeSubscriber.callCount).to.equal(1);
        expect(callback.callCount).to.equal(1);
        expect(callback.args[0][0]).to.equal(kArbitraryResult);
      });
    });
  });
  describe('recording', () => {
    const kArbitraryLabel = 'label1';
    let callback;
    let options;

    beforeEach(() => {
      callback = sinon.stub();
      options = {
        audio: true,
        video: true,
        data: true,
        label: kArbitraryLabel,
        screen: false,
        attributes: {},
      };
      streamManagerMock.getPublishedStreamById.returns(mocks.PublishedStream);
      mocks.PublishedStream.hasData.returns(true);
      mocks.PublishedStream.hasAudio.returns(true);
      mocks.PublishedStream.hasVideo.returns(true);
      mocks.PublishedStream.hasScreen.returns(true);
    });
    describe('onStartRecorder', () => {
      it('should fail if user has no permissions', () => {
        client.user.permissions[Permission.RECORD] = undefined;

        client.onStartRecorder(options, callback);
        expect(callback.callCount).to.equal(1);
        expect(callback.args[0][1]).to.equal('Unauthorized');
      });
      it('should fail if room is p2p', () => {
        roomMock.p2p = true;
        client.onStartRecorder(options, callback);
        expect(callback.callCount).to.equal(1);
        expect(callback.args[0][1]).to.equal('Stream can not be recorded');
      });
      it('should fail if stream is not present', () => {
        streamManagerMock.getPublishedStreamById.returns(undefined);
        client.onStartRecorder(options, callback);
        expect(callback.callCount).to.equal(1);
        expect(callback.args[0][1]).to.equal('Unable to subscribe to stream for recording, ' +
          'publisher not present');
      });
      it('should fail if stream has no video, audio or screen', () => {
        mocks.PublishedStream.hasAudio.returns(false);
        mocks.PublishedStream.hasVideo.returns(false);
        mocks.PublishedStream.hasScreen.returns(false);
        client.onStartRecorder(options, callback);
        expect(callback.callCount).to.equal(1);
        expect(callback.args[0][1]).to.equal('Stream can not be recorded');
      });
      it('should add externalOutput if success', () => {
        roomControllerMock.addExternalOutput.callsArg(3);
        client.onStartRecorder(options, callback);
        expect(roomControllerMock.addExternalOutput.callCount).to.equal(1);
        expect(mocks.PublishedStream.addExternalOutputSubscriber.callCount).to.equal(1);
        expect(callback.callCount).to.equal(1);
      });
    });
    describe('onStopRecorder', () => {
      beforeEach(() => {
        streamManagerMock.forEachPublishedStream.yields(mocks.PublishedStream);
        mocks.PublishedStream.hasExternalOutputSubscriber.returns(true);
      });
      it('should fail if user has no permissions', () => {
        client.user.permissions[Permission.RECORD] = undefined;

        client.onStopRecorder(options, callback);
        expect(callback.callCount).to.equal(1);
        expect(callback.args[0][1]).to.equal('Unauthorized');
      });
      it('should fail if stream does not exist', () => {
        mocks.PublishedStream.hasExternalOutputSubscriber.returns(false);
        client.onStopRecorder(options, callback);
        expect(callback.callCount).to.equal(1);
        expect(roomControllerMock.removeExternalOutput.callCount).to.equal(0);
        expect(callback.args[0][1]).to.equal('This stream is not being recorded');
      });
      it('should succeed stream exists', () => {
        client.onStopRecorder(options, callback);
        expect(callback.callCount).to.equal(1);
        expect(roomControllerMock.removeExternalOutput.callCount).to.equal(1);
      });
    });
  });
});
