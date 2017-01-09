/*global require, describe, it, beforeEach, afterEach*/
'use strict';
var mocks = require('../utils');
var sinon = require('sinon');
var expect  = require('chai').expect;

var kArbitraryErizoController = {name: 'arbitraryErizoController'};
var kArbitraryErizoControllerId = '1';

describe('ErizoController Registry', function() {
  var erizoControllerRegistry,
      dataBase;
  beforeEach(function() {
    mocks.start(mocks.licodeConfig);
    dataBase = mocks.start(mocks.dataBase);
    erizoControllerRegistry = require('../../mdb/erizoControllerRegistry.js');
  });

  afterEach(function() {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(dataBase);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  it('should return a list of erizoControllers when getErizoControllers is called', function() {
    dataBase.db.erizoControllers.find.returns({
      toArray: function(cb) {
        cb(null, [kArbitraryErizoController]);
      }
    });
    var callback = sinon.stub();
    erizoControllerRegistry.getErizoControllers(callback);

    expect(callback.calledWith([kArbitraryErizoController])).to.be.true;  // jshint ignore:line
  });

  it('should return undefined if not found in the db when getEC is called', function() {
    var callback = sinon.stub();
    dataBase.db.erizoControllers.findOne.callsArgWith(1, null, undefined);
    erizoControllerRegistry.getErizoController(kArbitraryErizoControllerId, callback);

    expect(callback.calledWith(undefined)).to.be.true;  // jshint ignore:line
  });

  it('should return a erizoController from the db when getEC is called', function() {
    var callback = sinon.stub();
    dataBase.db.erizoControllers.findOne.callsArgWith(1, null, kArbitraryErizoController);
    erizoControllerRegistry.getErizoController(kArbitraryErizoControllerId, callback);

    expect(callback.calledWith(kArbitraryErizoController)).to.be.true;  // jshint ignore:line
  });

  it('should return false if the EC is not found in db when hasEC is called', function() {
    var callback = sinon.stub();
    dataBase.db.erizoControllers.findOne.callsArgWith(1, null, undefined);
    erizoControllerRegistry.hasErizoController(kArbitraryErizoControllerId, callback);

    expect(callback.calledWith(false)).to.be.true;  // jshint ignore:line
  });

  it('should return true if the erizoController is found in db when hasEC is called', function() {
    var callback = sinon.stub();
    dataBase.db.erizoControllers.findOne.callsArgWith(1, null, kArbitraryErizoController);
    erizoControllerRegistry.hasErizoController(kArbitraryErizoControllerId, callback);

    expect(callback.calledWith(true)).to.be.true;  // jshint ignore:line
  });

  it('should call save on Database when calling addErizoController', function() {
    var callback = sinon.stub();
    dataBase.db.erizoControllers.save.callsArgWith(1, null, kArbitraryErizoController);
    erizoControllerRegistry.addErizoController(kArbitraryErizoController, callback);

    expect(dataBase.db.erizoControllers.save.calledOnce).to.be.true;  // jshint ignore:line
    expect(callback.calledWith(kArbitraryErizoController)).to.be.true;  // jshint ignore:line
  });

  it('should call update on Database when calling updateEC', function() {
    erizoControllerRegistry.updateErizoController(kArbitraryErizoControllerId, 
                                                  kArbitraryErizoController);

    expect(dataBase.db.erizoControllers.update.calledOnce).to.be.true;  // jshint ignore:line
  });

  it('should call remove on Database when removeEC is called and it exists', function() {
    dataBase.db.erizoControllers.findOne.callsArgWith(1, null, undefined);
    erizoControllerRegistry.removeErizoController(kArbitraryErizoControllerId);

    expect(dataBase.db.erizoControllers.remove.called).to.be.false;  // jshint ignore:line
  });

  it('should return true if the EC is found in the db when hasEC is called', function() {
    dataBase.db.erizoControllers.findOne.callsArgWith(1, null, kArbitraryErizoController);
    erizoControllerRegistry.removeErizoController(kArbitraryErizoControllerId);

    expect(dataBase.db.erizoControllers.remove.called).to.be.true;  // jshint ignore:line
  });
});
