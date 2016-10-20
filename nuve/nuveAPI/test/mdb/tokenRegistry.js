/*global require, describe, it, beforeEach, afterEach*/
'use strict';
var mocks = require('../utils');
var sinon = require('sinon');
var expect  = require('chai').expect;

var kArbitraryToken = {_id: '1', name: 'arbitraryToken', creationDate: new Date(0)};
var kArbitraryTokenId = '1';

describe('Token Registry', function() {
  var tokenRegistry,
      dataBase;
  beforeEach(function() {
    mocks.start(mocks.licodeConfig);
    dataBase = mocks.start(mocks.dataBase);
    tokenRegistry = require('../../mdb/tokenRegistry.js');
  });

  afterEach(function() {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(dataBase);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  it('should return a list of tokens when getList is called', function() {
    dataBase.db.tokens.find.returns({
      toArray: function(cb) {
        cb(null, [kArbitraryToken]);
      }
    });
    var callback = sinon.stub();
    tokenRegistry.getList(callback);

    expect(callback.calledWith([kArbitraryToken])).to.be.true;  // jshint ignore:line
  });

  it('should return undefined if not found in the database when getToken is called', function() {
    var callback = sinon.stub();
    dataBase.db.tokens.findOne.callsArgWith(1, null, undefined);
    tokenRegistry.getToken(kArbitraryTokenId, callback);

    expect(callback.calledWith(undefined)).to.be.true;  // jshint ignore:line
  });

  it('should return a token from the database when getToken is called', function() {
    var callback = sinon.stub();
    dataBase.db.tokens.findOne.callsArgWith(1, null, kArbitraryToken);
    tokenRegistry.getToken(kArbitraryTokenId, callback);

    expect(callback.calledWith(kArbitraryToken)).to.be.true;  // jshint ignore:line
  });

  it('should return false if the token is not found when hasToken is called', function() {
    var callback = sinon.stub();
    dataBase.db.tokens.findOne.callsArgWith(1, null, undefined);
    tokenRegistry.hasToken(kArbitraryTokenId, callback);

    expect(callback.calledWith(false)).to.be.true;  // jshint ignore:line
  });

  it('should return true if the token is found in database when hasToken is called', function() {
    var callback = sinon.stub();
    dataBase.db.tokens.findOne.callsArgWith(1, null, kArbitraryToken);
    tokenRegistry.hasToken(kArbitraryTokenId, callback);

    expect(callback.calledWith(true)).to.be.true;  // jshint ignore:line
  });

  it('should call save on Database when calling addToken', function() {
    var callback = sinon.stub();
    dataBase.db.tokens.save.callsArgWith(1, null, {_id: kArbitraryTokenId});
    tokenRegistry.addToken(kArbitraryToken, callback);

    expect(dataBase.db.tokens.save.calledOnce).to.be.true;  // jshint ignore:line
    expect(callback.calledWith(kArbitraryTokenId)).to.be.true;  // jshint ignore:line
  });

  it('should call update on Database when calling updateToken', function() {
    tokenRegistry.updateToken(kArbitraryToken);

    expect(dataBase.db.tokens.save.calledOnce).to.be.true;  // jshint ignore:line
  });

  it('should not call remove on Database when removeToken is called ' +
     'and it does not exist', function() {
    dataBase.db.tokens.findOne.callsArgWith(1, null, undefined);
    tokenRegistry.removeToken(kArbitraryTokenId);

    expect(dataBase.db.tokens.remove.called).to.be.false;  // jshint ignore:line
  });

  it('should return true if the token is found when removeToken is called', function() {
    dataBase.db.tokens.findOne.callsArgWith(1, null, kArbitraryToken);
    tokenRegistry.removeToken(kArbitraryTokenId);

    expect(dataBase.db.tokens.remove.called).to.be.true;  // jshint ignore:line
  });

  it('should remove a list of tokens when removeOldTokens is called', function() {
    dataBase.db.tokens.findOne.callsArgWith(1, null, kArbitraryToken);
    dataBase.db.tokens.find.returns({
      toArray: function(cb) {
        cb(null, [kArbitraryToken]);
      }
    });
    tokenRegistry.removeOldTokens();

    expect(dataBase.db.tokens.remove.calledOnce).to.be.true;  // jshint ignore:line
  });
});
