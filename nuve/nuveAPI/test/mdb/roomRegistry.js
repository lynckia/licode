/*global require, describe, it, beforeEach, afterEach*/
'use strict';
var mocks = require('../utils');
var sinon = require('sinon');
var expect  = require('chai').expect;

var kArbitraryRoom = {name: 'arbitraryRoom'};
var kArbitraryRoomId = '1';

describe('Room Registry', function() {
  var roomRegistry,
      dataBase;
  beforeEach(function() {
    mocks.start(mocks.licodeConfig);
    dataBase = mocks.start(mocks.dataBase);
    roomRegistry = require('../../mdb/roomRegistry.js');
  });

  afterEach(function() {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(dataBase);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  it('should return a list of rooms when getRooms is called', function() {
    dataBase.db.rooms.find.returns({
      toArray: function(cb) {
        cb(null, [kArbitraryRoom]);
      }
    });
    var callback = sinon.stub();
    roomRegistry.getRooms(callback);

    expect(callback.calledWith([kArbitraryRoom])).to.be.true;  // jshint ignore:line
  });

  it('should return undefined if not found in the database when getRoom is called', function() {
    var callback = sinon.stub();
    dataBase.db.rooms.findOne.callsArgWith(1, null, undefined);
    roomRegistry.getRoom(kArbitraryRoomId, callback);

    expect(callback.calledWith(undefined)).to.be.true;  // jshint ignore:line
  });

  it('should return a room from the database when getRoom is called', function() {
    var callback = sinon.stub();
    dataBase.db.rooms.findOne.callsArgWith(1, null, kArbitraryRoom);
    roomRegistry.getRoom(kArbitraryRoomId, callback);

    expect(callback.calledWith(kArbitraryRoom)).to.be.true;  // jshint ignore:line
  });

  it('should return false if the room is not found in database when hasRoom is called', function() {
    var callback = sinon.stub();
    dataBase.db.rooms.findOne.callsArgWith(1, null, undefined);
    roomRegistry.hasRoom(kArbitraryRoomId, callback);

    expect(callback.calledWith(false)).to.be.true;  // jshint ignore:line
  });

  it('should return true if the room is found in database when hasRoom is called', function() {
    var callback = sinon.stub();
    dataBase.db.rooms.findOne.callsArgWith(1, null, kArbitraryRoom);
    roomRegistry.hasRoom(kArbitraryRoomId, callback);

    expect(callback.calledWith(true)).to.be.true;  // jshint ignore:line
  });

  it('should call save on Database when calling addRoom', function() {
    var callback = sinon.stub();
    dataBase.db.rooms.save.callsArgWith(1, null, kArbitraryRoom);
    roomRegistry.addRoom(kArbitraryRoom, callback);

    expect(dataBase.db.rooms.save.calledOnce).to.be.true;  // jshint ignore:line
    expect(callback.calledWith(kArbitraryRoom)).to.be.true;  // jshint ignore:line
  });

  it('should call update on Database when calling updateRoom', function() {
    roomRegistry.updateRoom(kArbitraryRoomId, kArbitraryRoom);

    expect(dataBase.db.rooms.update.calledOnce).to.be.true;  // jshint ignore:line
  });

  it('should call remove on Database when removeRoom is called and it exists', function() {
    dataBase.db.rooms.findOne.callsArgWith(1, null, undefined);
    roomRegistry.removeRoom(kArbitraryRoomId);

    expect(dataBase.db.rooms.remove.called).to.be.false;  // jshint ignore:line
  });

  it('should return true if the room is found in the database when hasRoom is called', function() {
    dataBase.db.rooms.findOne.callsArgWith(1, null, kArbitraryRoom);
    roomRegistry.removeRoom(kArbitraryRoomId);

    expect(dataBase.db.rooms.remove.called).to.be.true;  // jshint ignore:line
  });
});
