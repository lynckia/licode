/* global require, describe, it, beforeEach, afterEach, before, after */

/* eslint-disable global-require */

const mocks = require('../utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;

describe('Erizo Agent', () => {
  let childProcessMock;
  let spawnMock;
  let erizoAgentReporterMock;
  let awslibMock;
  let osMock;
  let fsMock;
  let amqperMock;
  // eslint-disable-next-line no-unused-vars
  let erizoAgent;
  let licodeConfigMock;
  let processArgsBackup;

  const kDefaultOpts = {
    detached: true, stdio: ['ignore', 'pipe', 'pipe'],
  };

  // Allows passing arguments to mocha
  before(() => {
    processArgsBackup = [...process.argv];
    process.argv = [];
  });

  after(() => {
    process.argv = [...processArgsBackup];
  });

  beforeEach(() => {
    global.config = { logger: { configFile: true } };

    licodeConfigMock = mocks.start(mocks.licodeConfig);
    spawnMock = mocks.spawn;
    childProcessMock = mocks.start(mocks.childProcess);
    awslibMock = mocks.start(mocks.awslib);
    osMock = mocks.start(mocks.os);
    fsMock = mocks.start(mocks.fs);
    erizoAgentReporterMock = mocks.start(mocks.erizoAgentReporter);
    amqperMock = mocks.start(mocks.amqper);
  });

  afterEach(() => {
    mocks.stop(amqperMock);
    mocks.stop(erizoAgentReporterMock);
    mocks.stop(fsMock);
    mocks.stop(osMock);
    mocks.stop(awslibMock);
    mocks.stop(childProcessMock);
    mocks.stop(licodeConfigMock);
    mocks.deleteRequireCache();
    mocks.reset();
    global.config = {};
  });

  it('should create default globals', () => {
    erizoAgent = require('../../erizoAgent/erizoAgent');

    expect(global.config.erizoAgent.maxProcesses).to.equal(1);
    expect(global.config.erizoAgent.prerunProcesses).to.equal(1);
    expect(global.config.erizoAgent.publicIP).to.equal('');
    expect(global.config.erizoAgent.instanceLogDir).to.equal('.');
    expect(global.config.erizoAgent.useIndividualLogFiles).to.equal(false);
  });

  describe('Launch Erizo', () => {
    it('should launch 1 erizo by default', () => {
      erizoAgent = require('../../erizoAgent/erizoAgent');

      expect(childProcessMock.spawn.callCount).to.equal(1);
      expect(childProcessMock.spawn.args[0][0]).to.equal('./launch.sh');
      expect(childProcessMock.spawn.args[0][1][0]).to.equal('./../erizoJS/erizoJS.js');
      expect(childProcessMock.spawn.args[0][2]).to.deep.equal(kDefaultOpts);
      expect(spawnMock.stdout.setEncoding.callCount).to.equal(1);
      expect(spawnMock.stdout.on.callCount).to.equal(1);
      expect(fsMock.openSync.callCount).to.equal(0);
    });

    it('should launch erizos according to prerunProcesses', () => {
      const kArbitraryPrerunProcesses = 3;
      licodeConfigMock.erizoAgent.prerunProcesses = kArbitraryPrerunProcesses;
      licodeConfigMock.erizoAgent.maxProcesses = kArbitraryPrerunProcesses;

      erizoAgent = require('../../erizoAgent/erizoAgent');

      expect(childProcessMock.spawn.callCount).to.equal(kArbitraryPrerunProcesses);
      expect(spawnMock.stdout.setEncoding.callCount).to.equal(kArbitraryPrerunProcesses);
      expect(spawnMock.stdout.on.callCount).to.equal(kArbitraryPrerunProcesses);
      expect(spawnMock.stderr.setEncoding.callCount).to.equal(kArbitraryPrerunProcesses);
      expect(spawnMock.stderr.on.callCount).to.equal(kArbitraryPrerunProcesses);
      expect(fsMock.openSync.callCount).to.equal(0);
      for (let index = 0; index < kArbitraryPrerunProcesses; index += 1) {
        expect(childProcessMock.spawn.args[index][0]).to.equal('./launch.sh');
        expect(childProcessMock.spawn.args[index][1][0]).to.equal('./../erizoJS/erizoJS.js');
        expect(childProcessMock.spawn.args[index][2]).to.deep.equal(kDefaultOpts);
      }
    });

    it('should relaunch erizos when they close', () => {
      spawnMock.on.withArgs('close').onCall(0).callsArg(1);

      erizoAgent = require('../../erizoAgent/erizoAgent');

      expect(childProcessMock.spawn.callCount).to.equal(2);
    });

    it('should launch 1 erizo with individual log files', () => {
      licodeConfigMock.erizoAgent.useIndividualLogFiles = true;
      erizoAgent = require('../../erizoAgent/erizoAgent');

      expect(childProcessMock.spawn.callCount).to.equal(1);
      expect(childProcessMock.spawn.args[0][0]).to.equal('./launch.sh');
      expect(childProcessMock.spawn.args[0][1][0]).to.equal('./../erizoJS/erizoJS.js');
      expect(spawnMock.stdout.setEncoding.callCount).to.equal(0);
      expect(spawnMock.stdout.on.callCount).to.equal(0);
      expect(fsMock.openSync.callCount).to.equal(2);
    });
  });

  describe('Erizo Agent API', () => {
    let erizoAgentPublicApi;
    const kArbitraryPrerunProcesses = 0;
    const kArbitraryMaxProcesses = 3;

    beforeEach(() => {
      amqperMock.setPublicRPC = (api) => {
        erizoAgentPublicApi = api;
      };
      licodeConfigMock.erizoAgent.prerunProcesses = kArbitraryPrerunProcesses;
      licodeConfigMock.erizoAgent.maxProcesses = kArbitraryMaxProcesses;

      erizoAgent = require('../../erizoAgent/erizoAgent');
    });

    it('should create erizos', () => {
      const callback = sinon.stub();

      erizoAgentPublicApi.createErizoJS(undefined, callback);

      expect(childProcessMock.spawn.callCount).to.equal(1);
      expect(childProcessMock.spawn.args[0][0]).to.equal('./launch.sh');
      expect(childProcessMock.spawn.args[0][1][0]).to.equal('./../erizoJS/erizoJS.js');
      expect(childProcessMock.spawn.args[0][2]).to.deep.equal(kDefaultOpts);
      expect(spawnMock.stdout.setEncoding.callCount).to.equal(1);
      expect(spawnMock.stdout.on.callCount).to.equal(1);
      expect(fsMock.openSync.callCount).to.equal(0);
      expect(callback.callCount).to.equal(1);
    });

    it('should delete erizos', () => {
      let erizoId;
      const callback = sinon.stub();
      erizoAgentPublicApi.createErizoJS(undefined, (type, info) => {
        erizoId = info.erizoId;
      });

      erizoAgentPublicApi.deleteErizoJS(erizoId, callback);

      expect(spawnMock.kill.callCount).to.equal(1);
      expect(callback.callCount).to.equal(1);
    });
  });
});
