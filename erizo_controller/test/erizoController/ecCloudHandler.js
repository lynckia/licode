/* global require, describe, it, beforeEach, afterEach */

/* eslint-disable no-unused-expressions */

const mocks = require('../utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;

describe('Erizo Controller / ec Cloud Handler', () => {
  let amqperMock;
  let licodeConfigMock;
  let spec;
  let ecCloudHandler;

  beforeEach(() => {
    global.config = { logger: { configFile: true } };
    global.config.erizoController = { cloudHandlerPolicy: 'default_policy' };
    licodeConfigMock = mocks.start(mocks.licodeConfig);
    amqperMock = mocks.start(mocks.amqper);
    spec = {
      amqper: amqperMock,
    };
    // eslint-disable-next-line global-require
    ecCloudHandler = require('../../erizoController/ecCloudHandler').EcCloudHandler(spec);
  });

  afterEach(() => {
    mocks.stop(amqperMock);
    mocks.stop(licodeConfigMock);
    mocks.deleteRequireCache();
    mocks.reset();
    global.config = { logger: { configFile: true } };
  });

  it('should have a known API', () => {
    expect(ecCloudHandler.getErizoJS).not.to.be.undefined;
    expect(ecCloudHandler.deleteErizoJS).not.to.be.undefined;
  });

  describe('getErizoAgents', () => {
    it('should call getErizoAgents', () => {
      const arbitraryAgentId = 'agentId';
      amqperMock.broadcast.callsArgWith(2, { info: { id: arbitraryAgentId } });

      ecCloudHandler.getErizoAgents();

      expect(amqperMock.broadcast.callCount).to.equal(1);
      expect(amqperMock.broadcast.args[0][1]).to.deep.equal(
        { method: 'getErizoAgents', args: [] });
    });
  });

  describe('getErizoJS', () => {
    it('should call createErizoJS', () => {
      const callback = sinon.stub();

      ecCloudHandler.getErizoJS(undefined, undefined, callback);

      expect(amqperMock.callRpc.callCount).to.equal(1);
      expect(amqperMock.callRpc.args[0][1]).to.equal('createErizoJS');
    });

    it('should succeed if ErizoJS is created', () => {
      const arbitraryErizoId = 'erizoId';
      const arbitraryAgentId = 'agentId';
      const arbitraryInternalId = 'internalId';
      const callback = sinon.stub();

      ecCloudHandler.getErizoJS(undefined, undefined, callback);
      const sendResponse = amqperMock.callRpc.args[0][3].callback;
      sendResponse({ erizoId: arbitraryErizoId,
        agentId: arbitraryAgentId,
        internalId: arbitraryInternalId });

      expect(callback.callCount).to.equal(1);
      expect(callback.args[0]).to
        .deep.equal([arbitraryErizoId, arbitraryAgentId, arbitraryInternalId]);
    });

    it('should succeed after retry less than 5 times if ErizoJS is not created', () => {
      const arbitraryErizoId = 'erizoId';
      const arbitraryAgentId = 'agentId';
      const arbitraryInternalId = 'internalId';
      const agentsAttemps = 4;
      const callback = sinon.stub();
      let count = 0;
      let sendResponse;

      ecCloudHandler.getErizoJS(undefined, undefined, callback);
      for (count = 0; count <= agentsAttemps; count += 1) {
        sendResponse = amqperMock.callRpc.args[count][3].callback;
        sendResponse('timeout');
      }
      sendResponse = amqperMock.callRpc.args[count][3].callback;
      sendResponse({ erizoId: arbitraryErizoId,
        agentId: arbitraryAgentId,
        internalId: arbitraryInternalId });

      expect(callback.callCount).to.equal(1);
      expect(callback.args[0][0]).to.deep.equal(arbitraryErizoId);
    });

    it('should retry 5 times if ErizoJS is not created', () => {
      const agentsAttemps = 5;
      const callback = sinon.stub();

      ecCloudHandler.getErizoJS(undefined, undefined, callback);
      for (let count = 0; count <= agentsAttemps; count += 1) {
        const sendResponse = amqperMock.callRpc.args[count][3].callback;
        sendResponse('timeout');
      }

      expect(callback.callCount).to.equal(1);
      expect(callback.args[0][0]).to.deep.equal('timeout');
    });
  });

  describe('deleteErizoJS', () => {
    it('should call deleteErizoJS', () => {
      const arbitraryErizoId = 'erizoId';
      ecCloudHandler.deleteErizoJS(arbitraryErizoId);

      expect(amqperMock.broadcast.callCount).to.equal(1);
      expect(amqperMock.broadcast.args[0][1]).to.deep.equal(
        { method: 'deleteErizoJS', args: [arbitraryErizoId] });
    });
  });
});
