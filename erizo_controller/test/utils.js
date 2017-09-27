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
    nuve: {},
    erizoAgent: {},
    erizoController: {report: {}}
  });

  module.exports.spawn = {
    stdout: {
      setEncoding: sinon.stub(),
      on: sinon.stub()
    },
    stderr: {
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

  module.exports.Server = {
    listen: sinon.stub()
  };

  module.exports.signature = {
    update: sinon.stub(),
    digest: sinon.stub()
  };

  module.exports.crypto = createMock('crypto', {
    createHmac: sinon.stub().returns(module.exports.signature),
    randomBytes: sinon.stub().returns(new Buffer(16))
  });

  module.exports.http = createMock('http', {
    createServer: sinon.stub().returns(module.exports.Server),
    close: sinon.stub()
  });

  module.exports.socketInstance = {
    conn: {transport: {socket: {internalOnClose: undefined}}},
    disconnect: sinon.stub(),
    emit: sinon.stub(),
    on: sinon.stub()
  };

  module.exports.socketIoInstance = {
    set: sinon.stub(),
    sockets: {
      on: sinon.stub(),
      socket: sinon.stub().returns(module.exports.socketInstance),  // v0.9
      sockets:{'streamId1': module.exports.socketInstance,  // v2.0.3
               undefined: module.exports.socketInstance},
      indexOf: sinon.stub()
    }
  };

  module.exports.socketIo = createMock('socket.io', {
    listen: sinon.stub().returns(module.exports.socketIoInstance),
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
    getPeriodicStats: sinon.stub(),
    generatePLIPacket: sinon.stub(),
    createOffer: sinon.stub(),
    setRemoteSdp: sinon.stub(),
    addRemoteCandidate: sinon.stub(),
    setSlideShowMode: sinon.stub(),
    muteStream: sinon.stub()
  };

  module.exports.ExternalInput = {
    wrtcId: '',
    init: sinon.stub(),
    setAudioReceiver: sinon.stub(),
    setVideoReceiver: sinon.stub()
  };

  module.exports.ExternalOutput = {
    wrtcId: '',
    init: sinon.stub(),
    close: sinon.stub()
  };

  module.exports.erizoAPI = createMock('../../erizoAPI/build/Release/addon', {
    OneToManyProcessor: sinon.stub().returns(module.exports.OneToManyProcessor),
    WebRtcConnection: sinon.stub().returns(module.exports.WebRtcConnection),
    ExternalInput: sinon.stub().returns(module.exports.ExternalInput),
    ExternalOutput: sinon.stub().returns(module.exports.ExternalOutput)
  });

  module.exports.roomControllerInstance = {
    addEventListener: sinon.stub(),
    addExternalInput: sinon.stub(),
    addExternalOutput: sinon.stub(),
    processSignaling: sinon.stub(),
    addPublisher: sinon.stub(),
    addSubscriber: sinon.stub(),
    removePublisher: sinon.stub(),
    removeSubscriber: sinon.stub(),
    removeSubscriptions: sinon.stub(),
    removeExternalOutput: sinon.stub()
  };

  module.exports.roomController = createMock('../erizoController/roomController', {
    RoomController: sinon.stub().returns(module.exports.roomControllerInstance)
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
