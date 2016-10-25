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
    broadcast: sinon.stub(),
    setPublicRPC: sinon.stub(),
    callRpc: sinon.stub(),
    bind: sinon.stub(),
    bindBroadcast: sinon.stub()
  });

  module.exports.ErizoAgentReporterInstance = {
    getErizoAgent: sinon.stub()
  };

  module.exports.erizoAgentReporter = createMock('../erizoAgent/erizoAgentReporter', {
    Reporter: sinon.stub().returns(module.exports.ErizoAgentReporterInstance)
  });

  module.exports.OneToManyProcessor = {
    addExternalOutput: sinon.stub(),
    setExternalPublisher: sinon.stub(),
    setPublisher: sinon.stub(),
    addSubscriber: sinon.stub(),
    removeSubscriber: sinon.stub(),
    close: sinon.stub(),
  };

  module.exports.WebRtcConnection = {
    wrtcId: '',
    minVideoBW: '',
    scheme:'',
    periodicPlis:'',
    init: sinon.stub(),
    setAudioReceiver: sinon.stub(),
    setVideoReceiver: sinon.stub(),
    close: sinon.stub(),
    getStats: sinon.stub(),
    generatePLIPacket: sinon.stub(),
    createOffer: sinon.stub(),
    setRemoteSdp: sinon.stub(),
    addRemoteCandidate: sinon.stub(),
    setSlideShowMode: sinon.stub()
  };

  module.exports.ExternalInput = {
    wrtcId: '',
    init: sinon.stub(),
    setAudioReceiver: sinon.stub(),
    setVideoReceiver: sinon.stub()
  };

  module.exports.ExternalOutput = {
    wrtcId: '',
    init: sinon.stub()
  };

  module.exports.erizoAPI = createMock('../../erizoAPI/build/Release/addon', {
    OneToManyProcessor: sinon.stub().returns(module.exports.OneToManyProcessor),
    WebRtcConnection: sinon.stub().returns(module.exports.WebRtcConnection),
    ExternalInput: sinon.stub().returns(module.exports.ExternalInput),
    ExternalOutput: sinon.stub().returns(module.exports.ExternalOutput)
  });

  module.exports.ecchInstance = {
    getErizoJS: sinon.stub(),
    deleteErizoJS: sinon.stub()
  };

  module.exports.ecch = createMock('../erizoController/ecCloudHandler', {
    EcCloudHandler: sinon.stub().returns(module.exports.ecchInstance)
  });
};

reset();
