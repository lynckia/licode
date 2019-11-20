/* global require, describe, it, beforeEach, afterEach */


const mocks = require('../utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;

const kArbitraryErizoController = { name: 'arbitraryErizoController' };
const kArbitraryErizoControllerId = '1';

describe('ErizoController Registry', () => {
  let erizoControllerRegistry;
  let dataBase;
  beforeEach(() => {
    mocks.start(mocks.licodeConfig);
    dataBase = mocks.start(mocks.dataBase);
    // eslint-disable-next-line global-require
    erizoControllerRegistry = require('../../mdb/erizoControllerRegistry.js');
  });

  afterEach(() => {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(dataBase);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  it('should return a list of erizoControllers when getErizoControllers is called', () => {
    dataBase.db.erizoControllers.find.returns({
      toArray(cb) {
        cb(null, [kArbitraryErizoController]);
      },
    });
    const callback = sinon.stub();
    erizoControllerRegistry.getErizoControllers(callback);
    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith([kArbitraryErizoController])).to.be.true;
  });

  it('should return undefined if not found in the db when getEC is called', () => {
    const callback = sinon.stub();
    dataBase.db.erizoControllers.findOne.callsArgWith(1, null, undefined);
    erizoControllerRegistry.getErizoController(kArbitraryErizoControllerId, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith(undefined)).to.be.true;
  });

  it('should return a erizoController from the db when getEC is called', () => {
    const callback = sinon.stub();
    dataBase.db.erizoControllers.findOne.callsArgWith(1, null, kArbitraryErizoController);
    erizoControllerRegistry.getErizoController(kArbitraryErizoControllerId, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith(kArbitraryErizoController)).to.be.true;
  });

  it('should return false if the EC is not found in db when hasEC is called', () => {
    const callback = sinon.stub();
    dataBase.db.erizoControllers.findOne.callsArgWith(1, null, undefined);
    erizoControllerRegistry.hasErizoController(kArbitraryErizoControllerId, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith(false)).to.be.true;
  });

  it('should return true if the erizoController is found in db when hasEC is called', () => {
    const callback = sinon.stub();
    dataBase.db.erizoControllers.findOne.callsArgWith(1, null, kArbitraryErizoController);
    erizoControllerRegistry.hasErizoController(kArbitraryErizoControllerId, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith(true)).to.be.true;
  });

  it('should call save on Database when calling addErizoController', () => {
    const callback = sinon.stub();
    dataBase.db.erizoControllers.save.callsArgWith(1, null, kArbitraryErizoController);
    erizoControllerRegistry.addErizoController(kArbitraryErizoController, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(dataBase.db.erizoControllers.save.calledOnce).to.be.true;
    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith(kArbitraryErizoController)).to.be.true;
  });

  it('should call update on Database when calling updateEC', () => {
    erizoControllerRegistry.updateErizoController(kArbitraryErizoControllerId,
      kArbitraryErizoController);

    // eslint-disable-next-line no-unused-expressions
    expect(dataBase.db.erizoControllers.update.calledOnce).to.be.true;
  });

  it('should call remove on Database when removeEC is called and it exists', () => {
    dataBase.db.erizoControllers.findOne.callsArgWith(1, null, undefined);
    erizoControllerRegistry.removeErizoController(kArbitraryErizoControllerId);

    // eslint-disable-next-line no-unused-expressions
    expect(dataBase.db.erizoControllers.remove.called).to.be.false;
  });

  it('should return true if the EC is found in the db when hasEC is called', () => {
    dataBase.db.erizoControllers.findOne.callsArgWith(1, null, kArbitraryErizoController);
    erizoControllerRegistry.removeErizoController(kArbitraryErizoControllerId);

    // eslint-disable-next-line no-unused-expressions
    expect(dataBase.db.erizoControllers.remove.called).to.be.true;
  });
});
