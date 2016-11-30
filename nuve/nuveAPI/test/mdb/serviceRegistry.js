/*global require, describe, it, beforeEach, afterEach*/
'use strict';
var mocks = require('../utils');
var sinon = require('sinon');
var expect  = require('chai').expect;

var kArbitraryService = {name: 'arbitraryService'};
var kArbitraryServiceId = '1';

describe('Service Registry', function() {
  var serviceRegistry,
      dataBase;
  beforeEach(function() {
    mocks.start(mocks.licodeConfig);
    dataBase = mocks.start(mocks.dataBase);
    serviceRegistry = require('../../mdb/serviceRegistry.js');
  });

  afterEach(function() {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(dataBase);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  it('should return a list of services when getList is called', function() {
    dataBase.db.services.find.returns({
      toArray: function(cb) {
        cb(null, [kArbitraryService]);
      }
    });
    var callback = sinon.stub();
    serviceRegistry.getList(callback);

    expect(callback.calledWith([kArbitraryService])).to.be.true;  // jshint ignore:line
  });

  it('should return undefined if not found in the database when getService is called', function() {
    var callback = sinon.stub();
    dataBase.db.services.findOne.callsArgWith(1, null, undefined);
    serviceRegistry.getService(kArbitraryServiceId, callback);

    expect(callback.calledWith(undefined)).to.be.true;  // jshint ignore:line
  });

  it('should return a service from the database when getService is called', function() {
    var callback = sinon.stub();
    dataBase.db.services.findOne.callsArgWith(1, null, kArbitraryService);
    serviceRegistry.getService(kArbitraryServiceId, callback);

    expect(callback.calledWith(kArbitraryService)).to.be.true;  // jshint ignore:line
  });

  it('should return false if the service is not found when hasService is called', function() {
    var callback = sinon.stub();
    dataBase.db.services.findOne.callsArgWith(1, null, undefined);
    serviceRegistry.hasService(kArbitraryServiceId, callback);

    expect(callback.calledWith(false)).to.be.true;  // jshint ignore:line
  });

  it('should return true if the service is found when hasService is called', function() {
    var callback = sinon.stub();
    dataBase.db.services.findOne.callsArgWith(1, null, kArbitraryService);
    serviceRegistry.hasService(kArbitraryServiceId, callback);

    expect(callback.calledWith(true)).to.be.true;  // jshint ignore:line
  });

  it('should call save on Database when calling addService', function() {
    var callback = sinon.stub();
    dataBase.db.services.save.callsArgWith(1, null, {_id: kArbitraryServiceId});
    serviceRegistry.addService(kArbitraryService, callback);

    expect(dataBase.db.services.save.calledOnce).to.be.true;  // jshint ignore:line
    expect(callback.calledWith(kArbitraryServiceId)).to.be.true;  // jshint ignore:line
  });

  it('should call update on Database when calling updateService', function() {
    serviceRegistry.updateService(kArbitraryService);

    expect(dataBase.db.services.save.calledOnce).to.be.true;  // jshint ignore:line
  });

  it('should call remove on Database when removeService is called and it exists', function() {
    dataBase.db.services.findOne.callsArgWith(1, null, undefined);
    serviceRegistry.removeService(kArbitraryServiceId);

    expect(dataBase.db.services.remove.called).to.be.false;  // jshint ignore:line
  });

  it('should return true if the service is found when hasService is called', function() {
    dataBase.db.services.findOne.callsArgWith(1, null, kArbitraryService);
    serviceRegistry.removeService(kArbitraryServiceId);

    expect(dataBase.db.services.remove.called).to.be.true;  // jshint ignore:line
  });

  var service = {
    rooms: {
      '1': {'_id': 'roomId1'},
      '2': {'_id': 'roomId2'},
    }
  };

  it('should return the room given its id when getRoomForService is called', function() {
    var callback = sinon.stub();
    serviceRegistry.getRoomForService('roomId1', service, callback);

    expect(callback.args[0][0]).to.deep.equal({'_id': 'roomId1'});
  });

  it('should return the room given its id when getRoomForService is called', function() {
    var callback = sinon.stub();
    serviceRegistry.getRoomForService('roomId3', service, callback);

    expect(callback.args[0][0]).to.be.undefined;  // jshint ignore:line
  });
});
