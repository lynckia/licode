/* global require, describe, it, beforeEach, afterEach */


const mocks = require('../utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;

const kArbitraryToken = { _id: '1', name: 'arbitraryToken', creationDate: new Date(0) };
const kArbitraryTokenId = '1';

describe('Token Registry', () => {
  let tokenRegistry;
  let dataBase;
  beforeEach(() => {
    mocks.start(mocks.licodeConfig);
    dataBase = mocks.start(mocks.dataBase);
    // eslint-disable-next-line global-require
    tokenRegistry = require('../../mdb/tokenRegistry.js');
  });

  afterEach(() => {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(dataBase);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  it('should return a list of tokens when getList is called', () => {
    dataBase.db.tokens.find.returns({
      toArray(cb) {
        cb(null, [kArbitraryToken]);
      },
    });
    const callback = sinon.stub();
    tokenRegistry.getList(callback);

    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith([kArbitraryToken])).to.be.true;
  });

  it('should return undefined if not found in the database when getToken is called', () => {
    const callback = sinon.stub();
    dataBase.db.tokens.findOne.callsArgWith(1, null, undefined);
    tokenRegistry.getToken(kArbitraryTokenId, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith(undefined)).to.be.true;
  });

  it('should return a token from the database when getToken is called', () => {
    const callback = sinon.stub();
    dataBase.db.tokens.findOne.callsArgWith(1, null, kArbitraryToken);
    tokenRegistry.getToken(kArbitraryTokenId, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith(kArbitraryToken)).to.be.true;
  });

  it('should return false if the token is not found when hasToken is called', () => {
    const callback = sinon.stub();
    dataBase.db.tokens.findOne.callsArgWith(1, null, undefined);
    tokenRegistry.hasToken(kArbitraryTokenId, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith(false)).to.be.true;
  });

  it('should return true if the token is found in database when hasToken is called', () => {
    const callback = sinon.stub();
    dataBase.db.tokens.findOne.callsArgWith(1, null, kArbitraryToken);
    tokenRegistry.hasToken(kArbitraryTokenId, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith(true)).to.be.true;
  });

  it('should call save on Database when calling addToken', () => {
    const callback = sinon.stub();
    dataBase.db.tokens.save.callsArgWith(1, null, { _id: kArbitraryTokenId });
    tokenRegistry.addToken(kArbitraryToken, callback);

    // eslint-disable-next-line no-unused-expressions
    expect(dataBase.db.tokens.save.calledOnce).to.be.true;
    // eslint-disable-next-line no-unused-expressions
    expect(callback.calledWith(kArbitraryTokenId)).to.be.true;
  });

  it('should call update on Database when calling updateToken', () => {
    tokenRegistry.updateToken(kArbitraryToken);

    // eslint-disable-next-line no-unused-expressions
    expect(dataBase.db.tokens.save.calledOnce).to.be.true;
  });

  it('should not call remove on Database when removeToken is called ' +
     'and it does not exist', () => {
    dataBase.db.tokens.findOne.callsArgWith(1, null, undefined);
    tokenRegistry.removeToken(kArbitraryTokenId);

    // eslint-disable-next-line no-unused-expressions
    expect(dataBase.db.tokens.remove.called).to.be.false;
  });

  it('should return true if the token is found when removeToken is called', () => {
    dataBase.db.tokens.findOne.callsArgWith(1, null, kArbitraryToken);
    tokenRegistry.removeToken(kArbitraryTokenId);

    // eslint-disable-next-line no-unused-expressions
    expect(dataBase.db.tokens.remove.called).to.be.true;
  });

  it('should remove a list of tokens when removeOldTokens is called', () => {
    dataBase.db.tokens.findOne.callsArgWith(1, null, kArbitraryToken);
    dataBase.db.tokens.find.returns({
      toArray(cb) {
        cb(null, [kArbitraryToken]);
      },
    });
    tokenRegistry.removeOldTokens();

    // eslint-disable-next-line no-unused-expressions
    expect(dataBase.db.tokens.remove.calledOnce).to.be.true;
  });
});
