/* global require, describe, it, beforeEach, afterEach */

/* eslint-disable no-unused-expressions */

const mocks = require('../utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;

const StreamStates = require('../../erizoController/models/Stream').StreamStates;

describe('Erizo Controller / Room Controller', () => {
  let amqperMock;
  let licodeConfigMock;
  let ecchInstanceMock;
  let streamManagerMock;
  let publishedStreamMock;
  let ecchMock;
  let spec;
  let controller;

  beforeEach(() => {
    global.config = { logger: { configFile: true }, erizoController: {} };
    licodeConfigMock = mocks.start(mocks.licodeConfig);
    amqperMock = mocks.start(mocks.amqper);
    ecchInstanceMock = mocks.ecchInstance;
    ecchMock = mocks.start(mocks.ecch);
    streamManagerMock = mocks.start(mocks.StreamManager);
    publishedStreamMock = mocks.start(mocks.PublishedStream);
    spec = {
      amqper: amqperMock,
      ecch: ecchMock.EcCloudHandler(),
      streamManager: streamManagerMock,
    };
    // eslint-disable-next-line global-require
    controller = require('../../erizoController/roomController').RoomController(spec);
  });

  afterEach(() => {
    mocks.stop(ecchMock);
    mocks.stop(amqperMock);
    mocks.stop(licodeConfigMock);
    mocks.stop(streamManagerMock);
    mocks.stop(publishedStreamMock);
    mocks.deleteRequireCache();
    mocks.reset();
    global.config = { logger: { configFile: true } };
  });

  it('should have a known API', () => {
    expect(controller.addEventListener).not.to.be.undefined;
    expect(controller.addExternalInput).not.to.be.undefined;
    expect(controller.addExternalOutput).not.to.be.undefined;
    expect(controller.addPublisher).not.to.be.undefined;
    expect(controller.addSubscriber).not.to.be.undefined;
    expect(controller.removePublisher).not.to.be.undefined;
    expect(controller.removeSubscriber).not.to.be.undefined;
    expect(controller.processConnectionMessageFromClient).not.to.be.undefined;
    expect(controller.processStreamMessageFromClient).not.to.be.undefined;
  });

  describe('External Input', () => {
    const kArbitraryStreamId = 'id1';
    const kArbitraryClientId = 'clientid1';
    const kArbitraryLabel = 'label1';
    const kArbitraryUrl = 'url1';
    let callback;

    beforeEach(() => {
      callback = sinon.stub();
      streamManagerMock.getPublishedStreamState.returns(StreamStates.PUBLISHER_CREATED);
    });

    it('should call Erizo\'s addExternalInput if publisherStream state is CREATED', () => {
      ecchInstanceMock.getErizoJS.callsArgWith(2, 'erizoId');

      controller.addExternalInput(kArbitraryClientId, kArbitraryStreamId,
        kArbitraryUrl, kArbitraryLabel, callback);
      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(amqperMock.callRpc.args[0][1]).to.equal('addExternalInput');

      amqperMock.callRpc.args[0][3].callback({ type: 'initializing' });

      expect(callback.callCount).to.equal(1);
    });

    it('should fail if it already exists', () => {
      ecchInstanceMock.getErizoJS.callsArgWith(2, 'erizoId');

      controller.addExternalInput(kArbitraryClientId, kArbitraryStreamId,
        kArbitraryUrl, kArbitraryLabel, callback);
      expect(amqperMock.callRpc.callCount).to.equal(1);
    });
  });

  describe('External Output', () => {
    const kArbitraryId = 'id1';
    const kArbitraryUrl = 'url1';
    const kArbitraryOptions = {};
    const kArbitraryUnknownId = 'unknownId';
    const kArbitraryOutputUrl = 'url2';
    let callback;
    let mockPublisherStream;

    beforeEach(() => {
      ecchInstanceMock.getErizoJS.callsArgWith(2, 'erizoId');
      streamManagerMock.getErizoIdForPublishedStreamId.returns('erizoId');
      streamManagerMock.hasPublishedStream.returns(true);
      mockPublisherStream = {
        getExternalOutputSubscriberState: sinon.stub(),
      };
      mockPublisherStream.getExternalOutputSubscriberState.returns(StreamStates.SUBSCRIBER_CREATED);
      streamManagerMock.getPublishedStreamById.returns(mockPublisherStream);
    });

    it('should call Erizo\'s addExternalOutput', () => {
      callback = sinon.stub();

      controller.addExternalOutput(kArbitraryId, kArbitraryUrl, kArbitraryOptions, callback);

      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(amqperMock.callRpc.args[0][1]).to.equal('addExternalOutput');

      expect(callback.callCount).to.equal(1);
    });

    it('should fail if it already exists', () => {
      callback = sinon.stub();
      ecchInstanceMock.getErizoJS.callsArgWith(2, 'erizoId');

      controller.addExternalOutput(kArbitraryUnknownId, kArbitraryOutputUrl,
        kArbitraryOptions, callback);

      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(callback.withArgs('error').callCount).to.equal(0);
    });

    describe('Remove', () => {
      beforeEach(() => {
        streamManagerMock.getErizoIdForPublishedStreamId.returns('erizoId');
      });

      it('should call Erizo\'s removeExternalOutput', () => {
        callback = sinon.stub();
        controller.removeExternalOutput(kArbitraryId, kArbitraryUrl, callback);

        expect(amqperMock.callRpc.callCount).to.equal(1);
        expect(amqperMock.callRpc.args[0][1]).to.equal('removeExternalOutput');

        expect(callback.withArgs(true).callCount).to.equal(1);
      });
    });
  });

  describe('Add Publisher', () => {
    const kArbitraryClientId = 'id1';
    const kArbitraryStreamId = 'id2';
    const kArbitraryOptions = {};

    beforeEach(() => {
      streamManagerMock.getPublishedStreamState.returns(StreamStates.PUBLISHER_CREATED);
    });

    it('should call Erizo\'s addPublisher if publisherStream state is CREATED', () => {
      const callback = sinon.stub();
      ecchInstanceMock.getErizoJS.callsArgWith(2, 'erizoId');

      controller.addPublisher(kArbitraryClientId, kArbitraryStreamId, kArbitraryOptions, callback);

      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(amqperMock.callRpc.args[0][1]).to.equal('addPublisher');

      amqperMock.callRpc.args[0][3].callback({ type: 'initializing' });

      expect(callback.callCount).to.equal(1);
    });

    it('should not call Erizo\'s addPublisher if publisherStream state anything but CREATED', () => {
      const callback = sinon.stub();
      streamManagerMock.getPublishedStreamState.returns(1);
      ecchInstanceMock.getErizoJS.callsArgWith(2, 'erizoId');

      controller.addPublisher(kArbitraryClientId, kArbitraryStreamId, kArbitraryOptions, callback);

      expect(amqperMock.callRpc.callCount).to.equal(0);
    });


    it('should call send error on erizoJS timeout', () => {
      const callback = sinon.stub();
      ecchInstanceMock.getErizoJS.callsArgWith(2, 'timeout');

      controller.addPublisher(kArbitraryClientId, kArbitraryStreamId, kArbitraryOptions, callback);

      expect(amqperMock.callRpc.callCount).to.equal(0);

      expect(callback.callCount).to.equal(1);
      expect(callback.args[0][0]).to.equal('timeout-agent');
    });

    it('should return error on Publisher timeout', () => {
      const callback = sinon.stub();
      ecchInstanceMock.getErizoJS.callsArgWith(2, 'erizoId');

      controller.addPublisher(kArbitraryClientId, kArbitraryStreamId, kArbitraryOptions, callback);

      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(amqperMock.callRpc.args[0][1]).to.equal('addPublisher');

      amqperMock.callRpc.args[0][3].callback('timeout');
      amqperMock.callRpc.args[1][3].callback('timeout'); // First retry
      amqperMock.callRpc.args[2][3].callback('timeout'); // Second retry
      amqperMock.callRpc.args[3][3].callback('timeout'); // Third retry

      expect(callback.callCount).to.equal(4);
      expect(callback.args[0][0]).to.equal('timeout-erizojs-retry');
    });

    it('should fail on callback if it has been already removed', () => {
      const callback = sinon.stub();
      ecchInstanceMock.getErizoJS.callsArgWith(2, 'erizoId');

      controller.addPublisher(kArbitraryClientId, kArbitraryStreamId, kArbitraryOptions, callback);

      amqperMock.callRpc.args[0][3].callback('timeout');
      amqperMock.callRpc.args[1][3].callback('timeout'); // First retry
      amqperMock.callRpc.args[2][3].callback('timeout'); // Second retry
      amqperMock.callRpc.args[3][3].callback('timeout'); // Third retry

      controller.removePublisher(kArbitraryClientId, kArbitraryStreamId);

      expect(callback.callCount).to.equal(4);
      expect(callback.args[0][0]).to.equal('timeout-erizojs-retry');
    });
  });

  describe('Process Signaling', () => {
    const kArbitraryStreamId = 'id3';

    beforeEach(() => {
      ecchInstanceMock.getErizoJS.callsArgWith(2, 'erizoId');
    });

    it('should call Erizo\'s processConnectionMessage', () => {
      const kArbitraryMsg = 'message';

      controller.processConnectionMessageFromClient(null, kArbitraryStreamId, kArbitraryMsg);

      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(amqperMock.callRpc.args[0][1]).to.equal('processConnectionMessage');
    });

    it('should call Erizo\'s processStreamMessage', () => {
      const kArbitraryMsg = 'message';

      controller.processStreamMessageFromClient(null, kArbitraryStreamId, kArbitraryMsg);

      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(amqperMock.callRpc.args[0][1]).to.equal('processStreamMessage');
    });
  });

  describe('Add Subscriber', () => {
    const kArbitraryClientId = 'id1';
    const kArbitraryOptions = {};
    const kArbitraryStreamId = 'id2';
    let mockPublisherStream;

    beforeEach(() => {
      ecchInstanceMock.getErizoJS.callsArgWith(2, 'erizoId');
      streamManagerMock.getErizoIdForPublishedStreamId.returns('erizoId');
      streamManagerMock.hasPublishedStream.returns(true);
      mockPublisherStream = {
        getAvSubscriberState: sinon.stub(),
      };
      mockPublisherStream.getAvSubscriberState.returns(StreamStates.SUBSCRIBER_CREATED);
      streamManagerMock.getPublishedStreamById.returns(mockPublisherStream);
    });

    it('should call Erizo\'s addSubscriber', () => {
      const callback = sinon.stub();

      controller.addSubscriber(kArbitraryClientId, kArbitraryStreamId,
        kArbitraryOptions, callback);
      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(amqperMock.callRpc.args[0][1]).to.equal('addSubscriber');

      amqperMock.callRpc.args[0][3].callback({ type: 'initializing' });

      expect(callback.callCount).to.equal(1);
    });

    it('should return error on Subscriber timeout', () => {
      const callback = sinon.stub();

      controller.addSubscriber(kArbitraryClientId, kArbitraryStreamId,
        kArbitraryOptions, callback);

      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(amqperMock.callRpc.args[0][1]).to.equal('addSubscriber');

      amqperMock.callRpc.args[0][3].callback('timeout');
      amqperMock.callRpc.args[1][3].callback('timeout'); // First retry
      amqperMock.callRpc.args[2][3].callback('timeout'); // Second retry
      amqperMock.callRpc.args[3][3].callback('timeout'); // Third retry

      expect(callback.callCount).to.equal(1);
      expect(callback.args[0][0]).to.equal('timeout');
    });

    it('should fail if clientId is null', () => {
      const callback = sinon.stub();

      controller.addSubscriber(null, kArbitraryStreamId, kArbitraryOptions, callback);
      expect(amqperMock.callRpc.callCount).to.equal(0);
      expect(callback.args[0][0]).to.equal('Error: null clientId');
    });

    describe('And Remove', () => {
      beforeEach(() => {
        streamManagerMock.getErizoIdForPublishedStreamId.returns('erizoId');
      });

      it('should call Erizo\'s removeSubscriber', () => {
        controller.removeSubscriber(kArbitraryClientId, kArbitraryStreamId);

        expect(amqperMock.callRpc.callCount).to.equal(1);
        expect(amqperMock.callRpc.args[0][1]).to.equal('removeSubscriber');
      });
    });
  });

  describe('Add Multiple Subscribers', () => {
    const kArbitraryClientId = 'id1';
    const kArbitraryOptions = {};
    const kArbitraryStreamId = 'id2';

    beforeEach(() => {
      ecchInstanceMock.getErizoJS.callsArgWith(2, 'erizoId');
      streamManagerMock.getErizoIdForPublishedStreamId.returns('erizoId');
      streamManagerMock.hasPublishedStream.returns(true);
    });

    it('should call Erizo\'s addMultipleSubscribers', () => {
      const callback = sinon.stub();

      controller.addMultipleSubscribers(kArbitraryClientId, [kArbitraryStreamId],
        kArbitraryOptions, callback);
      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(amqperMock.callRpc.args[0][1]).to.equal('addMultipleSubscribers');

      amqperMock.callRpc.args[0][3].callback({ type: 'initializing' });

      expect(callback.callCount).to.equal(1);
    });

    it('should call Erizo\'s addMultipleSubscribers only once', () => {
      const callback = sinon.stub();

      controller.addMultipleSubscribers(kArbitraryClientId, [kArbitraryStreamId],
        kArbitraryOptions, callback);

      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(amqperMock.callRpc.args[0][1]).to.equal('addMultipleSubscribers');

      amqperMock.callRpc.args[0][3].callback({ type: 'multiple-initializing', streamIds: [kArbitraryStreamId] });

      controller.addMultipleSubscribers(kArbitraryClientId, [kArbitraryStreamId],
        kArbitraryOptions, callback);
      expect(amqperMock.callRpc.callCount).to.equal(2);

      expect(callback.callCount).to.equal(1);
    });


    it('should call Erizo\'s removeMultipleSubscribers', () => {
      const callback = sinon.stub();

      controller.removeMultipleSubscribers(kArbitraryClientId, [kArbitraryStreamId], callback);
      amqperMock.callRpc.args[0][3].callback({ type: 'multiple-initializing', streamIds: [kArbitraryStreamId] });
      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(amqperMock.callRpc.args[0][1]).to.equal('removeMultipleSubscribers');

      expect(callback.callCount).to.equal(1);
    });

    it('should fail if clientId is null', () => {
      const callback = sinon.stub();

      controller.addMultipleSubscribers(null, [kArbitraryStreamId], kArbitraryOptions, callback);
      expect(amqperMock.callRpc.callCount).to.equal(0);
      expect(callback.args[0][0]).to.equal('Error: null clientId');
    });

    describe('And Remove', () => {
      beforeEach(() => {
        controller.addMultipleSubscribers(kArbitraryClientId, [kArbitraryStreamId],
          kArbitraryOptions, sinon.stub());

        amqperMock.callRpc.args[0][3].callback({ type: 'multiple-initializing', streamIds: [kArbitraryStreamId] });
      });

      it('should call Erizo\'s removeMultipleSubscribers', () => {
        controller.removeMultipleSubscribers(kArbitraryClientId, [kArbitraryStreamId]);

        expect(amqperMock.callRpc.callCount).to.equal(2);
        expect(amqperMock.callRpc.args[1][1]).to.equal('removeMultipleSubscribers');
      });

      it('should fail if clientId does not exist', () => {
        const kArbitraryUnknownId = 'unknownId';
        controller.removeMultipleSubscribers(kArbitraryUnknownId, [kArbitraryStreamId]);

        expect(amqperMock.callRpc.callCount).to.equal(2);
      });

      it('should fail if subscriberId does not exist', () => {
        const kArbitraryUnknownId = 'unknownId';
        controller.removeMultipleSubscribers(kArbitraryClientId, [kArbitraryUnknownId]);

        expect(amqperMock.callRpc.callCount).to.equal(2);
      });
    });
  });
});
