/* global require, describe, it, beforeEach, afterEach */

 /* eslint-disable no-unused-expressions */

const mocks = require('../utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;

describe('Erizo Controller / Room Controller', () => {
  let amqperMock;
  let licodeConfigMock;
  let ecchInstanceMock;
  let ecchMock;
  let spec;
  let controller;

  beforeEach(() => {
    global.config = { logger: { configFile: true }, erizoController: {} };
    licodeConfigMock = mocks.start(mocks.licodeConfig);
    amqperMock = mocks.start(mocks.amqper);
    ecchInstanceMock = mocks.ecchInstance;
    ecchMock = mocks.start(mocks.ecch);
    spec = {
      amqper: amqperMock,
      ecch: ecchMock.EcCloudHandler(),
    };
    // eslint-disable-next-line global-require
    controller = require('../../erizoController/roomController').RoomController(spec);
  });

  afterEach(() => {
    mocks.stop(ecchMock);
    mocks.stop(amqperMock);
    mocks.stop(licodeConfigMock);
    mocks.deleteRequireCache();
    mocks.reset();
    global.config = { logger: { configFile: true } };
  });

  it('should have a known API', () => {
    expect(controller.addEventListener).not.to.be.undefined;
    expect(controller.addExternalInput).not.to.be.undefined;
    expect(controller.addExternalOutput).not.to.be.undefined;
    expect(controller.processSignaling).not.to.be.undefined;
    expect(controller.addPublisher).not.to.be.undefined;
    expect(controller.addSubscriber).not.to.be.undefined;
    expect(controller.removePublisher).not.to.be.undefined;
    expect(controller.removeSubscriber).not.to.be.undefined;
    expect(controller.removeSubscriptions).not.to.be.undefined;
  });

  describe('External Input', () => {
    const kArbitraryId = 'id1';
    const kArbitraryUrl = 'url1';

    it('should call Erizo\'s addExternalInput', () => {
      const callback = sinon.stub();
      ecchInstanceMock.getErizoJS.callsArgWith(2, 'erizoId');

      controller.addExternalInput(kArbitraryId, kArbitraryUrl, callback);
      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(amqperMock.callRpc.args[0][1]).to.equal('addExternalInput');
    });

    it('should fail if it already exists', () => {
      const callback = sinon.stub();
      ecchInstanceMock.getErizoJS.callsArgWith(2, 'erizoId');

      controller.addExternalInput(kArbitraryId, kArbitraryUrl, callback);
      expect(amqperMock.callRpc.callCount).to.equal(1);
    });
  });

  describe('External Output', () => {
    const kArbitraryId = 'id1';
    const kArbitraryUrl = 'url1';
    const kArbitraryOptions = {};
    const kArbitraryUnknownId = 'unknownId';
    const kArbitraryOutputUrl = 'url2';

    beforeEach(() => {
      const callback = sinon.stub();
      ecchInstanceMock.getErizoJS.callsArgWith(2, 'erizoId');

      controller.addExternalInput(kArbitraryId, kArbitraryUrl, callback);
    });

    it('should call Erizo\'s addExternalOutput', () => {
      const callback = sinon.stub();
      controller.addExternalOutput(kArbitraryId, kArbitraryUrl, kArbitraryOptions, callback);

      expect(amqperMock.callRpc.callCount).to.equal(2);
      expect(amqperMock.callRpc.args[1][1]).to.equal('addExternalOutput');

      expect(callback.withArgs('success').callCount).to.equal(1);
    });

    it('should fail if it already exists', () => {
      const callback = sinon.stub();
      ecchInstanceMock.getErizoJS.callsArgWith(2, 'erizoId');

      controller.addExternalOutput(kArbitraryUnknownId, kArbitraryOutputUrl,
        kArbitraryOptions, callback);

      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(callback.withArgs('error').callCount).to.equal(1);
    });

    describe('Remove', () => {
      beforeEach(() => {
        const callback = sinon.stub();
        controller.addExternalOutput(kArbitraryId, kArbitraryUrl, kArbitraryOptions, callback);
      });

      it('should call Erizo\'s removeExternalOutput', () => {
        const callback = sinon.stub();
        controller.removeExternalOutput(kArbitraryUrl, callback);

        expect(amqperMock.callRpc.callCount).to.equal(3);
        expect(amqperMock.callRpc.args[2][1]).to.equal('removeExternalOutput');

        expect(callback.withArgs(true).callCount).to.equal(1);
      });

      it('should fail if publisher does not exist', () => {
        controller.removePublisher(kArbitraryId, kArbitraryId);
        expect(amqperMock.callRpc.withArgs('ErizoJS_erizoId', 'removePublisher').callCount, 1);
        const cb = amqperMock.callRpc.withArgs('ErizoJS_erizoId', 'removePublisher')
                    .args[0][3].callback;
        cb(true);

        const callback = sinon.stub();
        controller.removeExternalOutput(kArbitraryUrl, callback);

        expect(callback.withArgs(null, 'This stream is not being recorded').callCount).to.equal(1);
      });
    });
  });

  describe('Add Publisher', () => {
    const kArbitraryClientId = 'id1';
    const kArbitraryStreamId = 'id2';
    const kArbitraryOptions = {};

    it('should call Erizo\'s addPublisher', () => {
      const callback = sinon.stub();
      ecchInstanceMock.getErizoJS.callsArgWith(2, 'erizoId');

      controller.addPublisher(kArbitraryClientId, kArbitraryStreamId, kArbitraryOptions, callback);

      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(amqperMock.callRpc.args[0][1]).to.equal('addPublisher');

      amqperMock.callRpc.args[0][3].callback({ type: 'initializing' });

      expect(callback.callCount).to.equal(1);
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
      amqperMock.callRpc.args[1][3].callback('timeout');  // First retry
      amqperMock.callRpc.args[2][3].callback('timeout');  // Second retry
      amqperMock.callRpc.args[3][3].callback('timeout');  // Third retry

      expect(callback.callCount).to.equal(1);
      expect(callback.args[0][0]).to.equal('timeout-erizojs');
    });

    it('should fail on callback if it has been already removed', () => {
      const callback = sinon.stub();
      ecchInstanceMock.getErizoJS.callsArgWith(2, 'erizoId');

      controller.addPublisher(kArbitraryClientId, kArbitraryStreamId, kArbitraryOptions, callback);

      amqperMock.callRpc.args[0][3].callback('timeout');
      amqperMock.callRpc.args[1][3].callback('timeout');  // First retry
      amqperMock.callRpc.args[2][3].callback('timeout');  // Second retry
      amqperMock.callRpc.args[3][3].callback('timeout');  // Third retry

      controller.removePublisher(kArbitraryClientId, kArbitraryStreamId);

      expect(callback.callCount).to.equal(1);
      expect(callback.args[0][0]).to.equal('timeout-erizojs');
    });
  });

  describe('Process Signaling', () => {
    const kArbitraryStreamId = 'id3';
    const kArbitraryClientId = 'id4';
    const kArbitraryPubOptions = {};

    beforeEach(() => {
      ecchInstanceMock.getErizoJS.callsArgWith(2, 'erizoId');
      controller.addPublisher(kArbitraryClientId, kArbitraryStreamId,
        kArbitraryPubOptions, sinon.stub());
    });

    it('should call Erizo\'s processSignaling', () => {
      const kArbitraryMsg = 'message';

      controller.processSignaling(null, kArbitraryStreamId, kArbitraryMsg);

      expect(amqperMock.callRpc.callCount).to.equal(2);
      expect(amqperMock.callRpc.args[1][1]).to.equal('processSignaling');
    });
  });

  describe('Add Subscriber', () => {
    const kArbitraryClientId = 'id1';
    const kArbitraryOptions = {};
    const kArbitraryStreamId = 'id2';
    const kArbitraryPubOptions = {};

    beforeEach(() => {
      ecchInstanceMock.getErizoJS.callsArgWith(2, 'erizoId');
      controller.addPublisher(kArbitraryClientId, kArbitraryStreamId,
         kArbitraryPubOptions, sinon.stub());
    });

    it('should call Erizo\'s addSubscriber', () => {
      const callback = sinon.stub();

      controller.addSubscriber(kArbitraryClientId, kArbitraryStreamId,
        kArbitraryOptions, callback);
      expect(amqperMock.callRpc.callCount).to.equal(2);
      expect(amqperMock.callRpc.args[1][1]).to.equal('addSubscriber');

      amqperMock.callRpc.args[1][3].callback({ type: 'initializing' });

      expect(callback.callCount).to.equal(1);
    });

    it('should return error on Publisher timeout', () => {
      const callback = sinon.stub();

      controller.addSubscriber(kArbitraryClientId, kArbitraryStreamId,
        kArbitraryOptions, callback);

      expect(amqperMock.callRpc.callCount).to.equal(2);
      expect(amqperMock.callRpc.args[1][1]).to.equal('addSubscriber');

      amqperMock.callRpc.args[1][3].callback('timeout');
      amqperMock.callRpc.args[2][3].callback('timeout');  // First retry
      amqperMock.callRpc.args[3][3].callback('timeout');  // Second retry
      amqperMock.callRpc.args[4][3].callback('timeout');  // Third retry

      expect(callback.callCount).to.equal(1);
      expect(callback.args[0][0]).to.equal('timeout');
    });

    it('should fail if clientId is null', () => {
      const callback = sinon.stub();

      controller.addSubscriber(null, kArbitraryStreamId, kArbitraryOptions, callback);
      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(callback.args[0][0]).to.equal('Error: null clientId');
    });

    it('should fail if Publisher does not exist', () => {
      const kArbitraryUnknownId = 'unknownId';
      const callback = sinon.stub();

      controller.addSubscriber(kArbitraryClientId, kArbitraryUnknownId,
        kArbitraryOptions, callback);
      expect(amqperMock.callRpc.callCount).to.equal(1);
    });

    describe('And Remove', () => {
      beforeEach(() => {
        controller.addSubscriber(kArbitraryClientId, kArbitraryStreamId,
            kArbitraryOptions, sinon.stub());

        amqperMock.callRpc.args[1][3].callback({ type: 'initializing' });
      });

      it('should call Erizo\'s removeSubscriber', () => {
        controller.removeSubscriber(kArbitraryClientId, kArbitraryStreamId);

        expect(amqperMock.callRpc.callCount).to.equal(3);
        expect(amqperMock.callRpc.args[2][1]).to.equal('removeSubscriber');
      });

      it('should fail if subscriberId does not exist', () => {
        const kArbitraryUnknownId = 'unknownId';
        controller.removeSubscriber(kArbitraryUnknownId, kArbitraryStreamId);

        expect(amqperMock.callRpc.callCount).to.equal(2);
      });

      it('should fail if publisherId does not exist', () => {
        const kArbitraryUnknownId = 'unknownId';
        controller.removeSubscriber(kArbitraryClientId, kArbitraryUnknownId);

        expect(amqperMock.callRpc.callCount).to.equal(2);
      });

      it('should call Erizo\'s removeSubscriptions', () => {
        controller.removeSubscriptions(kArbitraryClientId);

        expect(amqperMock.callRpc.callCount).to.equal(3);
        expect(amqperMock.callRpc.args[2][1]).to.equal('removeSubscriber');
      });
    });
  });
});
