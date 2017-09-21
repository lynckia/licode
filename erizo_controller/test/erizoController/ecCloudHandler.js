/*global require, describe, it, beforeEach, afterEach*/
'use strict';
var mocks = require('../utils');
var sinon = require('sinon');
var expect  = require('chai').expect;

describe('Erizo Controller / ec Cloud Handler', function() {
  var amqperMock,
      licodeConfigMock,
      spec,
      ecCloudHandler;

  beforeEach(function() {
    global.config = {logger: {configFile: true}};
    global.config.erizoController = {cloudHandlerPolicy: 'default_policy'};
    licodeConfigMock = mocks.start(mocks.licodeConfig);
    amqperMock = mocks.start(mocks.amqper);
    spec = {
      amqper: amqperMock
    };
    ecCloudHandler = require('../../erizoController/ecCloudHandler').EcCloudHandler(spec);
  });

  afterEach(function() {
    mocks.stop(amqperMock);
    mocks.stop(licodeConfigMock);
    mocks.deleteRequireCache();
    mocks.reset();
    global.config = {logger: {configFile: true}};
  });

  it('should have a known API', function() {
    expect(ecCloudHandler.getErizoJS).not.to.be.undefined;  // jshint ignore:line
    expect(ecCloudHandler.deleteErizoJS).not.to.be.undefined;  // jshint ignore:line
  });

  describe('getErizoAgents', function() {
    it('should call getErizoAgents', function() {
      var arbitraryAgentId = 'agentId';
      amqperMock.broadcast.callsArgWith(2, {info: {id: arbitraryAgentId}});

      ecCloudHandler.getErizoAgents();

      expect(amqperMock.broadcast.callCount).to.equal(1);
      expect(amqperMock.broadcast.args[0][1]).to.deep.equal(
        {method: 'getErizoAgents', args: []}
      );
    });
  });

  describe('getErizoJS', function() {
    it('should call createErizoJS', function() {
      var callback = sinon.stub();

      ecCloudHandler.getErizoJS(callback);

      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(amqperMock.callRpc.args[0][1]).to.equal('createErizoJS');
    });

    it('should succeed if ErizoJS is created', function() {
      var arbitraryErizoId = 'erizoId';
      var arbitraryAgentId = 'agentId';
      var callback = sinon.stub();

      ecCloudHandler.getErizoJS(callback);
      var sendResponse = amqperMock.callRpc.args[0][3].callback;
      sendResponse({erizoId: arbitraryErizoId, agentId: arbitraryAgentId});

      expect(callback.callCount).to.equal(1);
      expect(callback.args[0]).to.deep.equal([arbitraryErizoId, arbitraryAgentId]);
    });

    it('should succeed after retry less than 5 times if ErizoJS is not created', function() {
      var arbitraryErizoId = 'erizoId';
      var agentsAttemps = 4;
      var callback = sinon.stub();
      var sendResponse;

      ecCloudHandler.getErizoJS(callback);
      for (var count = 0; count <= agentsAttemps; count++) {
       sendResponse = amqperMock.callRpc.args[count][3].callback;
       sendResponse('timeout');
      }
      sendResponse = amqperMock.callRpc.args[count][3].callback;
      sendResponse(arbitraryErizoId);

      expect(callback.callCount).to.equal(1);
      expect(callback.args[0][0]).to.deep.equal(arbitraryErizoId);
    });

    it('should retry 5 times if ErizoJS is not created', function() {
      var agentsAttemps = 5;
      var callback = sinon.stub();

      ecCloudHandler.getErizoJS(callback);
      for (var count = 0; count <= agentsAttemps; count++) {
       var sendResponse = amqperMock.callRpc.args[count][3].callback;
       sendResponse('timeout');
      }

      expect(callback.callCount).to.equal(1);
      expect(callback.args[0][0]).to.deep.equal('timeout');
    });
  });

  describe('deleteErizoJS', function() {
    it('should call deleteErizoJS', function() {
      var arbitraryErizoId = 'erizoId';
      ecCloudHandler.deleteErizoJS(arbitraryErizoId);

      expect(amqperMock.broadcast.callCount).to.equal(1);
      expect(amqperMock.broadcast.args[0][1]).to.deep.equal(
        {method: 'deleteErizoJS', args: [arbitraryErizoId]}
      );
    });
  });
});
