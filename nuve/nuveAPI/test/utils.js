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

var dbEntry = function() {
  return {
    find: sinon.stub(),
    findOne: sinon.stub(),
    save: sinon.stub(),
    update: sinon.stub(),
    remove: sinon.stub()
  };
};

// Mocks
var reset = module.exports.reset = function() {
  module.exports.mauthParser = createMock('../auth/mauthParser', {
    calculateClientSignature: sinon.stub(),
    calculateServerSignature: sinon.stub(),
    parseHeader: sinon.stub(),
    makeHeader: sinon.stub()
  });

  module.exports.nuveAuthenticator = createMock('../auth/nuveAuthenticator', {
    authenticate: sinon.stub()
  });

  module.exports.serviceRegistry = createMock('../mdb/serviceRegistry', {
    getService: sinon.stub(),
    addService: sinon.stub(),
    updateService: sinon.stub(),
    removeService: sinon.stub(),
    getRoomForService: sinon.stub(),
    getList: sinon.stub().returns()
  });

  module.exports.roomRegistry = createMock('../mdb/roomRegistry', {
    getRoom: sinon.stub(),
    hasRoom: sinon.stub(),
    addRoom: sinon.stub(),
    updateRoom: sinon.stub(),
    removeRoom: sinon.stub()
  });

  module.exports.tokenRegistry = createMock('../mdb/tokenRegistry', {
    getList: sinon.stub(),
    getToken: sinon.stub(),
    hasToken: sinon.stub(),
    addToken: sinon.stub(),
    updateToken: sinon.stub(),
    removeOldTokens: sinon.stub(),
    removeToken: sinon.stub()
  });

  module.exports.erizoControllerRegistry = createMock('../mdb/erizoControllerRegistry', {
    getErizoControllers: sinon.stub(),
    getErizoController: sinon.stub(),
    hasErizoController: sinon.stub(),
    addErizoController: sinon.stub(),
    updateErizoController: sinon.stub(),
    removeErizoController: sinon.stub()
  });

  module.exports.dataBase = createMock('../mdb/dataBase', {
    superService: 'superService',
    nuveKey: 'nuveKey',
    testErizoController: 'testErizoController',
    db: {
      ObjectId: sinon.stub(),
      rooms: dbEntry(),
      services: dbEntry(),
      tokens: dbEntry(),
      erizoControllers: dbEntry()
    }
  });

  module.exports.amqpExchange = {
    publish: sinon.stub()
  };

  module.exports.amqpQueue = {
    name: 'testQueue',
    bind: sinon.stub(),
    subscribe: sinon.stub()
  };

  module.exports.amqpConnection = {
    on: sinon.stub(),
    exchange: sinon.stub().callsArgWith(2, module.exports.amqpExchange),
    queue: sinon.stub().callsArgWith(1, module.exports.amqpQueue)
  };

  module.exports.amqp = createMock('amqp', {
    createConnection: sinon.stub().returns(module.exports.amqpConnection)
  });

  module.exports.ec2MetadataService = {
    request: sinon.stub()
  };

  module.exports.awssdk = createMock('aws-sdk', {
    MetadataService: sinon.stub().returns(module.exports.ec2MetadataService)
  });

  module.exports.rpcPublic = createMock('../rpc/rpcPublic', {
  });

  module.exports.cloudHandler = createMock('../cloudHandler', {
    addNewErizoController: sinon.stub(),
    keepAlive: sinon.stub(),
    setInfo: sinon.stub(),
    killMe: sinon.stub(),
    getErizoControllerForRoom: sinon.stub(),
    getUsersInRoom: sinon.stub(),
    deleteRoom: sinon.stub(),
    deleteUser: sinon.stub()
  });

  module.exports.licodeConfig = createMock('licode_config', {
    logger: {configFile: true},
    cloudProvider: {host: ''}
  });
};

reset();
