/*global require, describe, it, beforeEach, afterEach*/
'use strict';
var mocks = require('../utils');
var sinon = require('sinon');
var expect  = require('chai').expect;

var kArbitraryRoom = {'_id': '1', name: '', options: {p2p: true, data: ''}};
var kArbitraryService = {'_id': '1', rooms: [kArbitraryRoom]};

describe('RpcPublic API', function() {
  var tokenRegistryMock,
      serviceRegistryMock,
      cloudHandlerMock,
      rpcPublic;

  beforeEach(function() {
    mocks.start(mocks.licodeConfig);
    tokenRegistryMock = mocks.start(mocks.tokenRegistry);
    serviceRegistryMock = mocks.start(mocks.serviceRegistry);
    cloudHandlerMock = mocks.start(mocks.cloudHandler);
    rpcPublic = require('../../rpc/rpcPublic');
  });

  afterEach(function() {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(tokenRegistryMock);
    mocks.stop(serviceRegistryMock);
    mocks.stop(cloudHandlerMock);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  describe('delete token', function() {
    it('should fail if token does not exist', function(done) {
      tokenRegistryMock.getToken.callsArgWithAsync(1, undefined);
      rpcPublic.deleteToken('1', function(type, message) {
        expect(type).to.equal('callback');
        expect(message).to.equal('error');
        done();
      });
    });

    it('should fail when removing an expired test token', function(done) {
      var arbitraryToken = {use: 500, service: kArbitraryService};
      tokenRegistryMock.getToken.callsArgWithAsync(1, arbitraryToken);
      serviceRegistryMock.getService.callsArgWithAsync(1, kArbitraryService);
      tokenRegistryMock.removeToken.callsArgAsync(1);
      rpcPublic.deleteToken('1', function(type, message) {
        expect(type).to.equal('callback');
        expect(message).to.equal('error');
        done();
      });
    });

    it('should succeed when removing a test token', function(done) {
      var arbitraryToken = {use: 1, service: kArbitraryService};
      tokenRegistryMock.getToken.callsArgWithAsync(1, arbitraryToken);
      serviceRegistryMock.getService.callsArgWithAsync(1, kArbitraryService);
      tokenRegistryMock.removeToken.callsArgAsync(1);
      rpcPublic.deleteToken('1', function(type, message) {
        expect(type).to.equal('callback');
        expect(message).to.deep.equal(arbitraryToken);
        expect(tokenRegistryMock.updateToken.callCount).to.equal(1);
        done();
      });
    });

    it('should succeed when removing a token', function(done) {
      var arbitraryToken = {service: kArbitraryService};
      tokenRegistryMock.getToken.callsArgWithAsync(1, arbitraryToken);
      tokenRegistryMock.removeToken.callsArgAsync(1);
      rpcPublic.deleteToken('1', function(type, message) {
        expect(type).to.equal('callback');
        expect(message).to.deep.equal(arbitraryToken);
        expect(tokenRegistryMock.removeToken.callCount).to.equal(1);
        done();
      });
    });
  });

  it('should add new erizo controllers', function() {
    var callback = sinon.stub();
    cloudHandlerMock.addNewErizoController.callsArgWith(1, 'id1');
    rpcPublic.addNewErizoController('message', callback);
    expect(callback.withArgs('callback', 'id1').callCount).to.equal(1);
    expect(cloudHandlerMock.addNewErizoController.callCount).to.equal(1);
  });

  it('should send keepAlives', function() {
    var callback = sinon.stub();
    cloudHandlerMock.keepAlive.callsArgWith(1, 'result1');
    rpcPublic.keepAlive('message', callback);
    expect(callback.withArgs('callback', 'result1').callCount).to.equal(1);
    expect(cloudHandlerMock.keepAlive.callCount).to.equal(1);
  });

  it('should set info', function() {
    var callback = sinon.stub();
    rpcPublic.setInfo('params1', callback);
    expect(callback.withArgs('callback').callCount).to.equal(1);
    expect(cloudHandlerMock.setInfo.withArgs('params1').callCount).to.equal(1);
  });

  it('should kill cloud handler', function() {
    var callback = sinon.stub();
    rpcPublic.killMe('ip1', callback);
    expect(callback.withArgs('callback').callCount).to.equal(1);
    expect(cloudHandlerMock.killMe.withArgs('ip1').callCount).to.equal(1);
  });
});
