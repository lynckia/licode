/*global require, describe, it, beforeEach, afterEach*/
'use strict';
var mocks = require('../utils');
var sinon = require('sinon');
var expect  = require('chai').expect;

describe('Erizo Agent', function() {
  var childProcessMock,
      spawnMock,
      erizoAgentReporterMock,
      awslibMock,
      osMock,
      fsMock,
      amqperMock,
      erizoAgent,
      licodeConfigMock,
      kDefaultOpts = {detached: true, stdio: ['ignore', 'pipe', 'pipe']};

  beforeEach(function() {
    licodeConfigMock = mocks.start(mocks.licodeConfig);
    spawnMock = mocks.spawn;
    childProcessMock = mocks.start(mocks.childProcess);
    awslibMock = mocks.start(mocks.awslib);
    osMock = mocks.start(mocks.os);
    fsMock = mocks.start(mocks.fs);
    erizoAgentReporterMock = mocks.start(mocks.erizoAgentReporter);
    amqperMock = mocks.start(mocks.amqper);
  });

  afterEach(function() {
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

  it('should create default globals', function() {
    erizoAgent = require('../../erizoAgent/erizoAgent');

    expect(global.config.erizoAgent.maxProcesses).to.equal(1);
    expect(global.config.erizoAgent.prerunProcesses).to.equal(1);
    expect(global.config.erizoAgent.publicIP).to.equal('');
    expect(global.config.erizoAgent.instanceLogDir).to.equal('.');
    expect(global.config.erizoAgent.useIndividualLogFiles).to.equal(false);
  });

  describe('Launch Erizo', function() {

    it('should launch 1 erizo by default', function() {
      erizoAgent = require('../../erizoAgent/erizoAgent');

      expect(childProcessMock.spawn.callCount).to.equal(1);
      expect(childProcessMock.spawn.args[0][0]).to.equal('./launch.sh');
      expect(childProcessMock.spawn.args[0][1][0]).to.equal('./../erizoJS/erizoJS.js');
      expect(childProcessMock.spawn.args[0][2]).to.deep.equal(kDefaultOpts);
      expect(spawnMock.stdout.setEncoding.callCount).to.equal(1);
      expect(spawnMock.stdout.on.callCount).to.equal(1);
      expect(fsMock.openSync.callCount).to.equal(0);
    });

    it('should launch erizos according to prerunProcesses', function() {
      var kArbitraryPrerunProcesses = 3;
      licodeConfigMock.erizoAgent.prerunProcesses = kArbitraryPrerunProcesses;
      licodeConfigMock.erizoAgent.maxProcesses = kArbitraryPrerunProcesses;

      erizoAgent = require('../../erizoAgent/erizoAgent');

      expect(childProcessMock.spawn.callCount).to.equal(kArbitraryPrerunProcesses);
      expect(spawnMock.stdout.setEncoding.callCount).to.equal(kArbitraryPrerunProcesses);
      expect(spawnMock.stdout.on.callCount).to.equal(kArbitraryPrerunProcesses);
      expect(spawnMock.stderr.setEncoding.callCount).to.equal(kArbitraryPrerunProcesses);
      expect(spawnMock.stderr.on.callCount).to.equal(kArbitraryPrerunProcesses);
      expect(fsMock.openSync.callCount).to.equal(0);
      for (var index = 0; index < kArbitraryPrerunProcesses; index++) {
        expect(childProcessMock.spawn.args[index][0]).to.equal('./launch.sh');
        expect(childProcessMock.spawn.args[index][1][0]).to.equal('./../erizoJS/erizoJS.js');
        expect(childProcessMock.spawn.args[index][2]).to.deep.equal(kDefaultOpts);
      }
    });

    it('should relaunch erizos when they close', function() {
      spawnMock.on.withArgs('close').onCall(0).callsArg(1);

      erizoAgent = require('../../erizoAgent/erizoAgent');

      expect(childProcessMock.spawn.callCount).to.equal(2);
    });

    it('should launch 1 erizo with individual log files', function() {
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

  describe('Erizo Agent API', function() {
    var erizoAgentPublicApi;
    var kArbitraryPrerunProcesses = 0;
    var kArbitraryMaxProcesses = 3;

    beforeEach(function() {
      amqperMock.setPublicRPC = function(api) {
        erizoAgentPublicApi = api;
      };
      licodeConfigMock.erizoAgent.prerunProcesses = kArbitraryPrerunProcesses;
      licodeConfigMock.erizoAgent.maxProcesses = kArbitraryMaxProcesses;

      erizoAgent = require('../../erizoAgent/erizoAgent');
    });

    it('should create erizos', function() {
      var callback = sinon.stub();

      erizoAgentPublicApi.createErizoJS(callback);

      expect(childProcessMock.spawn.callCount).to.equal(1);
      expect(childProcessMock.spawn.args[0][0]).to.equal('./launch.sh');
      expect(childProcessMock.spawn.args[0][1][0]).to.equal('./../erizoJS/erizoJS.js');
      expect(childProcessMock.spawn.args[0][2]).to.deep.equal(kDefaultOpts);
      expect(spawnMock.stdout.setEncoding.callCount).to.equal(1);
      expect(spawnMock.stdout.on.callCount).to.equal(1);
      expect(fsMock.openSync.callCount).to.equal(0);
      expect(callback.callCount).to.equal(1);
    });

    it('should delete erizos', function() {
      var erizoId;
      var callback = sinon.stub();
      erizoAgentPublicApi.createErizoJS(function(type, info) {
        erizoId = info.erizoId;
      });

      erizoAgentPublicApi.deleteErizoJS(erizoId, callback);

      expect(spawnMock.kill.callCount).to.equal(1);
      expect(callback.callCount).to.equal(1);
    });
  });
});
