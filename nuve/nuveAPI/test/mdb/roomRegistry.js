/* global require, describe, it, beforeEach, afterEach */


const mocks = require('../utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;

const kArbitraryRoom = { name: 'arbitraryRoom' };
const kArbitraryRoomId = '1';

describe('Room Registry', () => {
  let roomRegistry;
  let dataBase;
  beforeEach(() => {
    mocks.start(mocks.licodeConfig);
    dataBase = mocks.start(mocks.dataBase);
    // eslint-disable-next-line global-require
    roomRegistry = require('../../mdb/roomRegistry.js');
  });

  afterEach(() => {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(dataBase);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  it('should return a list of rooms when getRooms is called', () => {
    dataBase.db.rooms.find.returns({
      toArray(cb) {
        cb(null, [kArbitraryRoom]);
      },
    });
    const callback = sinon.stub();
    roomRegistry.getRooms(callback);

    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith([kArbitraryRoom])).to.be.true;
  });

  it('should return undefined if not found in the database when getRoom is called', () => {
    const callback = sinon.stub();
    dataBase.db.rooms.findOne.callsArgWith(1, null, undefined);
    roomRegistry.getRoom(kArbitraryRoomId, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith(undefined)).to.be.true;
  });

  it('should return a room from the database when getRoom is called', () => {
    const callback = sinon.stub();
    dataBase.db.rooms.findOne.callsArgWith(1, null, kArbitraryRoom);
    roomRegistry.getRoom(kArbitraryRoomId, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith(kArbitraryRoom)).to.be.true;
  });

  it('should return false if the room is not found in database when hasRoom is called', () => {
    const callback = sinon.stub();
    dataBase.db.rooms.findOne.callsArgWith(1, null, undefined);
    roomRegistry.hasRoom(kArbitraryRoomId, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith(false)).to.be.true;
  });

  it('should return true if the room is found in database when hasRoom is called', () => {
    const callback = sinon.stub();
    dataBase.db.rooms.findOne.callsArgWith(1, null, kArbitraryRoom);
    roomRegistry.hasRoom(kArbitraryRoomId, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith(true)).to.be.true;
  });

  it('should call save on Database when calling addRoom', () => {
    const callback = sinon.stub();
    dataBase.db.rooms.save.callsArgWith(1, null, kArbitraryRoom);
    roomRegistry.addRoom(kArbitraryRoom, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(dataBase.db.rooms.save.calledOnce).to.be.true;
    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith(kArbitraryRoom)).to.be.true;
  });

  it('should call update on Database when calling updateRoom', () => {
    roomRegistry.updateRoom(kArbitraryRoomId, kArbitraryRoom);

    // eslint-disable-next-line no-unused-expressions
    expect(dataBase.db.rooms.update.calledOnce).to.be.true;
  });

  it('should call remove on Database when removeRoom is called and it exists', () => {
    dataBase.db.rooms.findOne.callsArgWith(1, null, undefined);
    roomRegistry.removeRoom(kArbitraryRoomId);

    // eslint-disable-next-line no-unused-expressions
    expect(dataBase.db.rooms.remove.called).to.be.false;
  });

  it('should return true if the room is found in the database when hasRoom is called', () => {
    dataBase.db.rooms.findOne.callsArgWith(1, null, kArbitraryRoom);
    roomRegistry.removeRoom(kArbitraryRoomId);

    // eslint-disable-next-line no-unused-expressions
    expect(dataBase.db.rooms.remove.called).to.be.true;
  });
});
