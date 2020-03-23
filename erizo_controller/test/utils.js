/* globals require */

/* eslint-disable no-param-reassign */

// eslint-disable-next-line import/no-extraneous-dependencies
const mock = require('mock-require');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');

const goodCrypto = require('crypto');

module.exports.start = (mockObject) => {
  mock(mockObject.mockName, mockObject);
  return mockObject;
};

module.exports.stop = (mockObject) => {
  mock.stop(mockObject.mockName);
};

const createMock = (name, object) => {
  object.mockName = name;
  return object;
};

module.exports.deleteRequireCache = () => {
  Object.keys(require.cache).forEach((requiredModule) => {
    delete require.cache[requiredModule];
  });
};

// Mocks
module.exports.reset = () => {
  module.exports.licodeConfig = createMock('./../../licode_config', {
    logger: { configFile: true },
    cloudProvider: { host: '' },
    nuve: {},
    erizoAgent: {},
    erizoController: { report: {} },
  });

  module.exports.spawn = {
    stdout: {
      setEncoding: sinon.stub(),
      on: sinon.stub(),
    },
    stderr: {
      setEncoding: sinon.stub(),
      on: sinon.stub(),
    },
    on: sinon.stub(),
    unref: sinon.stub(),
    kill: sinon.stub(),
  };

  module.exports.childProcess = createMock('child_process', {
    spawn: sinon.stub().returns(module.exports.spawn),
  });

  module.exports.ec2Client = {
    call: sinon.stub(),
  };

  module.exports.awslib = createMock('aws-lib', {
    createEC2Client: sinon.stub().returns(module.exports.ec2Client),
  });

  module.exports.os = createMock('os', {
    networkInterfaces: sinon.stub(),
  });

  module.exports.fs = createMock('fs', {
    openSync: sinon.stub(),
    close: sinon.stub(),
  });

  module.exports.Server = {
    listen: sinon.stub(),
  };

  module.exports.signature = {
    update: sinon.stub(),
    digest: sinon.stub(),
  };

  module.exports.crypto = createMock('crypto', {
    createHmac: sinon.stub().returns(module.exports.signature),
    randomBytes: () => goodCrypto.randomBytes(16),
  });

  module.exports.http = createMock('http', {
    createServer: sinon.stub().returns(module.exports.Server),
    close: sinon.stub(),
  });

  module.exports.socketInstance = {
    conn: { transport: { socket: { internalOnClose: undefined } } },
    disconnect: sinon.stub(),
    emit: sinon.stub(),
    on: sinon.stub(),
    removeListener: sinon.stub(),
  };

  module.exports.socketIoInstance = {
    set: sinon.stub(),
    sockets: {
      on: sinon.stub(),
      socket: sinon.stub().returns(module.exports.socketInstance), // v0.9
      sockets: { streamId1: module.exports.socketInstance, // v2.0.3
        undefined: module.exports.socketInstance },
      indexOf: sinon.stub(),
    },
  };

  module.exports.socketIo = createMock('socket.io', {
    listen: sinon.stub().returns(module.exports.socketIoInstance),
    close: sinon.stub(),
  });

  module.exports.amqper = createMock('../common/amqper', {
    connect: sinon.stub().callsArg(0),
    broadcast: sinon.stub(),
    setPublicRPC: sinon.stub(),
    callRpc: sinon.stub(),
    bind: sinon.stub(),
    bindBroadcast: sinon.stub(),
  });

  module.exports.ErizoAgentReporterInstance = {
    getErizoAgent: sinon.stub(),
  };

  module.exports.erizoAgentReporter = createMock('../erizoAgent/erizoAgentReporter', {
    Reporter: sinon.stub().returns(module.exports.ErizoAgentReporterInstance),
  });

  module.exports.OneToManyProcessor = {
    addExternalOutput: sinon.stub(),
    setExternalPublisher: sinon.stub(),
    setPublisher: sinon.stub(),
    addSubscriber: sinon.stub(),
    removeSubscriber: sinon.stub(),
    close: sinon.stub(),
  };

  module.exports.ConnectionDescription = {
    close: sinon.stub(),
    setRtcpMux: sinon.stub(),
    setProfile: sinon.stub(),
    setBundle: sinon.stub(),
    setAudioAndVideo: sinon.stub(),
    setVideoSsrcList: sinon.stub(),
    postProcessInfo: sinon.stub(),
    hasAudio: sinon.stub(),
    hasVideo: sinon.stub(),
  };

  module.exports.WebRtcConnection = {
    init: sinon.stub(),
    close: sinon.stub(),
    createOffer: sinon.stub(),
    setRemoteSdp: sinon.stub(),
    setRemoteDescription: sinon.stub(),
    getLocalDescription: sinon.stub()
      .returns(Promise.resolve(module.exports.ConnectionDescription)),
    addRemoteCandidate: sinon.stub(),
    addMediaStream: sinon.stub().returns(Promise.resolve()),
    removeMediaStream: sinon.stub(),
    getConnectionQualityLevel: sinon.stub().returns(2),
  };

  module.exports.MediaStream = {
    minVideoBW: '',
    scheme: '',
    periodicPlis: '',
    close: sinon.stub(),
    configure: sinon.stub(),
    setAudioReceiver: sinon.stub(),
    setVideoReceiver: sinon.stub(),
    setMaxVideoBW: sinon.stub(),
    getStats: sinon.stub(),
    getPeriodicStats: sinon.stub(),
    generatePLIPacket: sinon.stub(),
    setSlideShowMode: sinon.stub(),
    setPeriodicKeyframeRequests: sinon.stub(),
    muteStream: sinon.stub(),
    onMediaStreamEvent: sinon.stub(),
  };

  module.exports.ExternalInput = {
    init: sinon.stub(),
    setAudioReceiver: sinon.stub(),
    setVideoReceiver: sinon.stub(),
  };

  module.exports.ExternalOutput = {
    init: sinon.stub(),
    close: sinon.stub(),
  };

  module.exports.erizoAPI = createMock('../../erizoAPI/build/Release/addon', {
    OneToManyProcessor: sinon.stub().returns(module.exports.OneToManyProcessor),
    ConnectionDescription: sinon.stub().returns(module.exports.ConnectionDescription),
    WebRtcConnection: sinon.stub().returns(module.exports.WebRtcConnection),
    MediaStream: sinon.stub().returns(module.exports.MediaStream),
    ExternalInput: sinon.stub().returns(module.exports.ExternalInput),
    ExternalOutput: sinon.stub().returns(module.exports.ExternalOutput),
  });

  module.exports.StreamManager = {
    addPublishedStream: sinon.stub(),
    removePublishedStream: sinon.stub(),
    forEachPublishedStream: sinon.stub().returns(),
    getPublishedStreamById: sinon.stub(),
    hasPublishedStream: sinon.stub(),
    getPublishedStreamState: sinon.stub(),
    getErizoIdForPublishedStreamId: sinon.stub(),
    getExternalOutputSubscriber: sinon.stub(),
    getPublishersInErizoId: sinon.stub(),
    updateErizoIdForPublishedStream: sinon.stub(),
  };

  module.exports.Channel = {
    on: sinon.stub(),
    onToken: sinon.stub(),
    onDisconnect: sinon.stub(),
    socketOn: sinon.stub(),
    socketRemoveListener: sinon.stub(),
    onReconnected: sinon.stub(),
    sendMessage: sinon.stub(),
    getBuffer: sinon.stub(),
    sendBuffer: sinon.stub(),
    disconnect: sinon.stub(),
  };

  module.exports.Client = {
    sendMessage: sinon.stub(),
  };

  module.exports.Room = {
    getClientById: sinon.stub(),
    forEachClient: sinon.stub(),
    sendMessage: sinon.stub(),
  };

  module.exports.roomControllerInstance = {
    addEventListener: sinon.stub(),
    addExternalInput: sinon.stub(),
    addExternalOutput: sinon.stub(),
    processStreamMessageFromClient: sinon.stub(),
    processConnectionMessageFromClient: sinon.stub(),
    addPublisher: sinon.stub(),
    addMultipleSubscribers: sinon.stub(),
    removeMultipleSubscribers: sinon.stub(),
    addSubscriber: sinon.stub(),
    removePublisher: sinon.stub(),
    removeSubscriber: sinon.stub(),
    removeSubscriptions: sinon.stub(),
    removeExternalOutput: sinon.stub(),
    removeClient: sinon.stub(),
  };

  module.exports.roomController = createMock('../erizoController/roomController', {
    RoomController: sinon.stub().returns(module.exports.roomControllerInstance),
  });

  module.exports.PublishedStream = {
    getID: sinon.stub(),
    getClientId: sinon.stub(),
    hasVideo: sinon.stub(),
    hasAudio: sinon.stub(),
    hasData: sinon.stub(),
    hasScreen: sinon.stub(),
    updateErizoId: sinon.stub(),
    updateStreamState: sinon.stub(),
    forEachDataSubscriber: sinon.stub(),
    addDataSubscriber: sinon.stub(),
    removeDataSubscriber: sinon.stub(),
    getAvSubscriber: sinon.stub(),
    hasAvSubscriber: sinon.stub(),
    addAvSubscriber: sinon.stub(),
    removeAvSubscriber: sinon.stub(),
    updateAvSubscriberState: sinon.stub(),
    updateExternalOutputSubscriberState: sinon.stub(),
    addExternalOutputSubscriber: sinon.stub(),
    getExternalOutputSubscriber: sinon.stub(),
    hasExternalOutputSubscriber: sinon.stub(),
    removeExternalOutputSubscriber: sinon.stub(),
    getPublicStream: sinon.stub(),
    meetAnySelector: sinon.stub(),
  };

  module.exports.Stream = createMock('../erizoController/models/Stream', {
    PublishedStream: sinon.stub().returns(module.exports.PublishedStream),
    // eslint-disable-next-line global-require
    StreamStates: require('../erizoController/models/Stream').StreamStates,
  });

  module.exports.ecchInstance = {
    getErizoJS: sinon.stub(),
    deleteErizoJS: sinon.stub(),
  };

  module.exports.ecch = createMock('../erizoController/ecCloudHandler', {
    EcCloudHandler: sinon.stub().returns(module.exports.ecchInstance),
  });
};

const reset = module.exports.reset;

reset();
