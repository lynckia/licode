/*global require, describe, it, beforeEach, afterEach*/
'use strict';
var mocks = require('../utils');
var sinon = require('sinon');
var expect  = require('chai').expect;

describe('Erizo Controller / Room Controller', function() {
  var amqperMock,
      licodeConfigMock,
      ecchInstanceMock,
      ecchMock,
      spec,
      controller;

  beforeEach(function() {
    global.config = {logger: {configFile: true}};
    licodeConfigMock = mocks.start(mocks.licodeConfig);
    amqperMock = mocks.start(mocks.amqper);
    ecchInstanceMock = mocks.ecchInstance;
    ecchMock = mocks.start(mocks.ecch);
    spec = {
      amqper: amqperMock,
      ecch: ecchMock.EcCloudHandler()
    };
    controller = require('../../erizoController/roomController').RoomController(spec);
  });

  afterEach(function() {
    mocks.stop(ecchMock);
    mocks.stop(amqperMock);
    mocks.stop(licodeConfigMock);
    mocks.deleteRequireCache();
    mocks.reset();
    global.config = {logger: {configFile: true}};
  });

  it('should have a known API', function() {
    expect(controller.addEventListener).not.to.be.undefined;  // jshint ignore:line
    expect(controller.addExternalInput).not.to.be.undefined;  // jshint ignore:line
    expect(controller.addExternalOutput).not.to.be.undefined;  // jshint ignore:line
    expect(controller.processSignaling).not.to.be.undefined;  // jshint ignore:line
    expect(controller.addPublisher).not.to.be.undefined;  // jshint ignore:line
    expect(controller.addSubscriber).not.to.be.undefined;  // jshint ignore:line
    expect(controller.removePublisher).not.to.be.undefined;  // jshint ignore:line
    expect(controller.removeSubscriber).not.to.be.undefined;  // jshint ignore:line
    expect(controller.removeSubscriptions).not.to.be.undefined;  // jshint ignore:line
  });

  describe('External Input', function() {
    var kArbitraryId = 'id1',
        kArbitraryUrl = 'url1';

    it('should call Erizo\'s addExternalInput', function() {
      var callback = sinon.stub();
      ecchInstanceMock.getErizoJS.callsArgWith(0, 'erizoId');

      controller.addExternalInput(kArbitraryId, kArbitraryUrl, callback);
      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(amqperMock.callRpc.args[0][1]).to.equal('addExternalInput');
    });

    it('should fail if it already exists', function() {
      var callback = sinon.stub();
      ecchInstanceMock.getErizoJS.callsArgWith(0, 'erizoId');

      controller.addExternalInput(kArbitraryId, kArbitraryUrl, callback);

      controller.addExternalInput(kArbitraryId, kArbitraryUrl, callback);
      expect(amqperMock.callRpc.callCount).to.equal(1);
    });
  });

  describe('External Output', function() {
    var kArbitraryId = 'id1',
        kArbitraryUrl = 'url1',
        kArbitraryUnknownId = 'unknownId',
        kArbitraryOutputUrl = 'url2';

    beforeEach(function() {
      var callback = sinon.stub();
      ecchInstanceMock.getErizoJS.callsArgWith(0, 'erizoId');

      controller.addExternalInput(kArbitraryId, kArbitraryUrl, callback);
    });

    it('should call Erizo\'s addExternalOutput', function() {
      var callback = sinon.stub();
      controller.addExternalOutput(kArbitraryId, kArbitraryUrl, callback);

      expect(amqperMock.callRpc.callCount).to.equal(2);
      expect(amqperMock.callRpc.args[1][1]).to.equal('addExternalOutput');

      expect(callback.withArgs('success').callCount).to.equal(1);
    });

    it('should fail if it already exists', function() {
      var callback = sinon.stub();
      ecchInstanceMock.getErizoJS.callsArgWith(0, 'erizoId');

      controller.addExternalOutput(kArbitraryUnknownId, kArbitraryOutputUrl, callback);

      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(callback.withArgs('error').callCount).to.equal(1);
    });

    describe('Remove', function() {
      beforeEach(function() {
        var callback = sinon.stub();
        controller.addExternalOutput(kArbitraryId, kArbitraryUrl, callback);
      });

      it('should call Erizo\'s removeExternalOutput', function() {
        var callback = sinon.stub();
        controller.removeExternalOutput(kArbitraryUrl, callback);

        expect(amqperMock.callRpc.callCount).to.equal(3);
        expect(amqperMock.callRpc.args[2][1]).to.equal('removeExternalOutput');

        expect(callback.withArgs(true).callCount).to.equal(1);
      });

      it('should fail if publisher does not exist', function() {
        controller.removePublisher(kArbitraryId);

        var callback = sinon.stub();
        controller.removeExternalOutput(kArbitraryUrl, callback);

        expect(callback.withArgs(null, 'This stream is not being recorded').callCount).to.equal(1);
      });
    });
  });

  describe('Add Publisher', function() {
    var kArbitraryId = 'id1',
        kArbitraryOptions = {};

    it('should call Erizo\'s addPublisher', function() {
      var callback = sinon.stub();
      ecchInstanceMock.getErizoJS.callsArgWith(0, 'erizoId');

      controller.addPublisher(kArbitraryId, kArbitraryOptions, callback);

      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(amqperMock.callRpc.args[0][1]).to.equal('addPublisher');

      amqperMock.callRpc.args[0][3].callback({type: 'initializing'});

      expect(callback.callCount).to.equal(1);
    });

    it('should call send error on erizoJS timeout', function() {
      var callback = sinon.stub();
      ecchInstanceMock.getErizoJS.callsArgWith(0, 'timeout');

      controller.addPublisher(kArbitraryId, kArbitraryOptions, callback);

      expect(amqperMock.callRpc.callCount).to.equal(0);

      expect(callback.callCount).to.equal(1);
      expect(callback.args[0][0]).to.equal('timeout-agent');
    });

    it('should return error on Publisher timeout', function() {
      var callback = sinon.stub();
      ecchInstanceMock.getErizoJS.callsArgWith(0, 'erizoId');

      controller.addPublisher(kArbitraryId, kArbitraryOptions, callback);

      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(amqperMock.callRpc.args[0][1]).to.equal('addPublisher');

      amqperMock.callRpc.args[0][3].callback('timeout');
      amqperMock.callRpc.args[1][3].callback('timeout');  // First retry
      amqperMock.callRpc.args[2][3].callback('timeout');  // Second retry
      amqperMock.callRpc.args[3][3].callback('timeout');  // Third retry

      expect(callback.callCount).to.equal(1);
      expect(callback.args[0][0]).to.equal('timeout-erizojs');
    });

    it('should fail on callback if it has been already removed', function() {
      var callback = sinon.stub();
      ecchInstanceMock.getErizoJS.callsArgWith(0, 'erizoId');

      controller.addPublisher(kArbitraryId, kArbitraryOptions, callback);

      amqperMock.callRpc.args[0][3].callback('timeout');
      amqperMock.callRpc.args[1][3].callback('timeout');  // First retry
      amqperMock.callRpc.args[2][3].callback('timeout');  // Second retry
      amqperMock.callRpc.args[3][3].callback('timeout');  // Third retry

      controller.removePublisher(kArbitraryId);

      expect(callback.callCount).to.equal(1);
      expect(callback.args[0][0]).to.equal('timeout-erizojs');
    });
  });

  describe('Process Signaling', function() {
    var kArbitraryPubId = 'id2',
        kArbitraryPubOptions = {};

    beforeEach(function() {
      ecchInstanceMock.getErizoJS.callsArgWith(0, 'erizoId');
      controller.addPublisher(kArbitraryPubId, kArbitraryPubOptions, sinon.stub());
    });

    it('should call Erizo\'s processSignaling', function() {
      var kArbitraryMsg = 'message';

      controller.processSignaling(kArbitraryPubId, null, kArbitraryMsg);

      expect(amqperMock.callRpc.callCount).to.equal(2);
      expect(amqperMock.callRpc.args[1][1]).to.equal('processSignaling');
    });
  });

  describe('Add Subscriber', function() {
    var kArbitraryId = 'id1',
        kArbitraryOptions = {},
        kArbitraryPubId = 'id2',
        kArbitraryPubOptions = {};

    beforeEach(function() {
      ecchInstanceMock.getErizoJS.callsArgWith(0, 'erizoId');
      controller.addPublisher(kArbitraryPubId, kArbitraryPubOptions, sinon.stub());
    });

    it('should call Erizo\'s addSubscriber', function() {
      var callback = sinon.stub();

      controller.addSubscriber(kArbitraryId, kArbitraryPubId, kArbitraryOptions, callback);

      expect(amqperMock.callRpc.callCount).to.equal(2);
      expect(amqperMock.callRpc.args[1][1]).to.equal('addSubscriber');

      amqperMock.callRpc.args[1][3].callback({type: 'initializing'});

      expect(callback.callCount).to.equal(1);
    });

    it('should return error on Publisher timeout', function() {
      var callback = sinon.stub();

      controller.addSubscriber(kArbitraryId, kArbitraryPubId, kArbitraryOptions, callback);

      expect(amqperMock.callRpc.callCount).to.equal(2);
      expect(amqperMock.callRpc.args[1][1]).to.equal('addSubscriber');

      amqperMock.callRpc.args[1][3].callback('timeout');
      amqperMock.callRpc.args[2][3].callback('timeout');  // First retry
      amqperMock.callRpc.args[3][3].callback('timeout');  // Second retry
      amqperMock.callRpc.args[4][3].callback('timeout');  // Third retry

      expect(callback.callCount).to.equal(1);
      expect(callback.args[0][0]).to.equal('timeout');
    });

    it('should fail if subscriberId is null', function() {
      var callback = sinon.stub();

      controller.addSubscriber(null, kArbitraryPubId, kArbitraryOptions, callback);
      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(callback.args[0][0]).to.equal('Error: null subscriberId');
    });

    it('should fail if Publisher does not exist', function() {
      var kArbitraryUnknownId = 'unknownId';
      var callback = sinon.stub();

      controller.addSubscriber(kArbitraryId, kArbitraryUnknownId, kArbitraryOptions, callback);
      expect(amqperMock.callRpc.callCount).to.equal(1);
    });

    describe('And Remove', function() {
      beforeEach(function() {
        controller.addSubscriber(kArbitraryId, kArbitraryPubId, kArbitraryOptions, sinon.stub());

        amqperMock.callRpc.args[1][3].callback({type: 'initializing'});
      });

      it('should call Erizo\'s removeSubscriber', function() {
        controller.removeSubscriber(kArbitraryId, kArbitraryPubId);

        expect(amqperMock.callRpc.callCount).to.equal(3);
        expect(amqperMock.callRpc.args[2][1]).to.equal('removeSubscriber');
      });

      it('should fail if subscriberId does not exist', function() {
        var kArbitraryUnknownId = 'unknownId';
        controller.removeSubscriber(kArbitraryUnknownId, kArbitraryPubId);

        expect(amqperMock.callRpc.callCount).to.equal(2);
      });

      it('should fail if publisherId does not exist', function() {
        var kArbitraryUnknownId = 'unknownId';
        controller.removeSubscriber(kArbitraryId, kArbitraryUnknownId);

        expect(amqperMock.callRpc.callCount).to.equal(2);
      });

      it('should call Erizo\'s removeSubscriptions', function() {
        controller.removeSubscriptions(kArbitraryId);

        expect(amqperMock.callRpc.callCount).to.equal(3);
        expect(amqperMock.callRpc.args[2][1]).to.equal('removeSubscriber');
      });
    });
  });
});
