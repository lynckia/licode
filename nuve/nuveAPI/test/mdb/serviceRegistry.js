/* global require, describe, it, beforeEach, afterEach */


const mocks = require('../utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;

const kArbitraryService = { name: 'arbitraryService' };
const kArbitraryServiceId = '1';
const kArbitraryRoom = { name: '', options: { test: true } };

describe('Service Registry', () => {
  let serviceRegistry;
  let dataBase;
  beforeEach(() => {
    mocks.start(mocks.licodeConfig);
    dataBase = mocks.start(mocks.dataBase);
    // eslint-disable-next-line global-require
    serviceRegistry = require('../../mdb/serviceRegistry.js');
  });

  afterEach(() => {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(dataBase);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  it('should return a list of services when getList is called', () => {
    dataBase.db.collection('services').find.returns({
      toArray(cb) {
        cb(null, [kArbitraryService]);
      },
    });
    const callback = sinon.stub();
    serviceRegistry.getList(callback);

    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith([kArbitraryService])).to.be.true;
  });

  it('should return undefined if not found in the database when getService is called', () => {
    const callback = sinon.stub();
    dataBase.db.collection('services').findOne.callsArgWith(1, null, undefined);
    serviceRegistry.getService(kArbitraryServiceId, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith(undefined)).to.be.true;
  });

  it('should return a service from the database when getService is called', () => {
    const callback = sinon.stub();
    dataBase.db.collection('services').findOne.callsArgWith(1, null, kArbitraryService);
    serviceRegistry.getService(kArbitraryServiceId, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith(kArbitraryService)).to.be.true;
  });

  it('should return false if the service is not found when hasService is called', () => {
    const callback = sinon.stub();
    dataBase.db.collection('services').findOne.callsArgWith(1, null, undefined);
    serviceRegistry.hasService(kArbitraryServiceId, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith(false)).to.be.true;
  });

  it('should return true if the service is found when hasService is called', () => {
    const callback = sinon.stub();
    dataBase.db.collection('services').findOne.callsArgWith(1, null, kArbitraryService);
    serviceRegistry.hasService(kArbitraryServiceId, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith(true)).to.be.true;
  });

  it('should call insertOne on Database when calling addService', () => {
    const callback = sinon.stub();
    dataBase.db.collection('services').insertOne.callsArgWith(1, null, { insertedId: kArbitraryServiceId });
    serviceRegistry.addService(kArbitraryService, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(dataBase.db.collection('services').insertOne.calledOnce).to.be.true;
    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith(kArbitraryServiceId)).to.be.true;
  });

  it('should call replaceOne on Database when calling updateService', () => {
    serviceRegistry.updateService(kArbitraryService);

    // eslint-disable-next-line no-unused-expressions
    expect(dataBase.db.collection('services').replaceOne.calledOnce).to.be.true;
  });

  it('should call updateOne on Database when calling addRoomToService', () => {
    serviceRegistry.addRoomToService(kArbitraryService, kArbitraryRoom);

    // eslint-disable-next-line no-unused-expressions
    expect(dataBase.db.collection('services').updateOne.calledOnce).to.be.true;
  });

  it('should call deleteOne on Database when removeService is called and it exists', () => {
    dataBase.db.collection('services').findOne.callsArgWith(1, null, undefined);
    serviceRegistry.removeService(kArbitraryServiceId);

    // eslint-disable-next-line no-unused-expressions
    expect(dataBase.db.collection('services').deleteOne.called).to.be.false;
  });

  it('should return true if the service is found when hasService is called', () => {
    dataBase.db.collection('services').findOne.callsArgWith(1, null, kArbitraryService);
    serviceRegistry.removeService(kArbitraryServiceId);

    // eslint-disable-next-line no-unused-expressions
    expect(dataBase.db.collection('services').deleteOne.called).to.be.true;
  });

  const service = {
    rooms: {
      1: { _id: 'roomId1' },
      2: { _id: 'roomId2' },
    },
  };

  it('should return the room given its id when getRoomForService is called', () => {
    const callback = sinon.stub();
    serviceRegistry.getRoomForService('roomId1', service, callback);

    expect(callback.args[0][0]).to.deep.equal({ _id: 'roomId1' });
  });

  it('should return the room given its id when getRoomForService is called', () => {
    const callback = sinon.stub();
    serviceRegistry.getRoomForService('roomId3', service, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(callback.args[0][0]).to.be.undefined;
  });
});
