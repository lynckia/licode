/*globals require*/
'use strict';
var mock = require('mock-require');
var sinon = require('sinon');

module.exports.start = function(mockObject) {
  mock(mockObject.mockName, mockObject);
  return mockObject;
};

module.exports.stop = function(mockObject) {
  mock.stop(mockObject.mockName);
};

var createMock = function(name, object) {
  object.mockName = name;
  return object;
};

module.exports.deleteRequireCache = function() {
  for (var requiredModule in require.cache) {
    delete require.cache[requiredModule];
  }
};

// Mocks
var reset = module.exports.reset = function() {
  module.exports.licodeConfig = createMock('./../../licode_config', {
    logger: {configFile: true},
    cloudProvider: {host: ''},
    erizoAgent: {}
  });

  module.exports.spawn = {
    stdout: {
      setEncoding: sinon.stub(),
      on: sinon.stub()
    },
    on: sinon.stub(),
    unref: sinon.stub(),
    kill: sinon.stub()
  };

  module.exports.childProcess = createMock('child_process', {
    spawn: sinon.stub().returns(module.exports.spawn)
  });

  module.exports.ec2Client = {
    call: sinon.stub()
  };

  module.exports.awslib = createMock('aws-lib', {
    createEC2Client: sinon.stub().returns(module.exports.ec2Client)
  });

  module.exports.os = createMock('os', {
    networkInterfaces: sinon.stub()
  });

  module.exports.fs = createMock('fs', {
    openSync: sinon.stub(),
    close: sinon.stub()
  });

  module.exports.amqper = createMock('../common/amqper', {
    connect: sinon.stub().callsArg(0),
    setPublicRPC: sinon.stub(),
    bind: sinon.stub(),
    bindBroadcast: sinon.stub()
  });

  module.exports.ErizoAgentReporterInstance = {
    getErizoAgent: sinon.stub()
  };

  module.exports.erizoAgentReporter = createMock('../erizoAgent/erizoAgentReporter', {
    Reporter: sinon.stub().returns(module.exports.ErizoAgentReporterInstance)
  });
};

reset();
