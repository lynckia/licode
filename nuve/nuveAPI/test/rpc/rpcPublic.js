/* global require, describe, it, beforeEach, afterEach */


// eslint-disable-next-line import/no-extraneous-dependencies
const mocks = require('../utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;

const kArbitraryRoom = { _id: '1', name: '', options: { p2p: true, data: '' } };
const kArbitraryService = { _id: '1', rooms: [kArbitraryRoom] };

describe('RpcPublic API', () => {
  let tokenRegistryMock;
  let serviceRegistryMock;
  let cloudHandlerMock;
  let rpcPublic;

  beforeEach(() => {
    mocks.start(mocks.licodeConfig);
    tokenRegistryMock = mocks.start(mocks.tokenRegistry);
    serviceRegistryMock = mocks.start(mocks.serviceRegistry);
    cloudHandlerMock = mocks.start(mocks.cloudHandler);
    // eslint-disable-next-line global-require
    rpcPublic = require('../../rpc/rpcPublic');
  });

  afterEach(() => {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(tokenRegistryMock);
    mocks.stop(serviceRegistryMock);
    mocks.stop(cloudHandlerMock);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  describe('delete token', () => {
    it('should fail if token does not exist', (done) => {
      tokenRegistryMock.getToken.callsArgWithAsync(1, undefined);
      rpcPublic.deleteToken('1', (type, message) => {
        expect(type).to.equal('callback');
        expect(message).to.equal('error');
        done();
      });
    });

    it('should fail when removing an expired test token', (done) => {
      const arbitraryToken = { use: 500, service: kArbitraryService };
      tokenRegistryMock.getToken.callsArgWithAsync(1, arbitraryToken);
      serviceRegistryMock.getService.callsArgWithAsync(1, kArbitraryService);
      tokenRegistryMock.removeToken.callsArgAsync(1);
      rpcPublic.deleteToken('1', (type, message) => {
        expect(type).to.equal('callback');
        expect(message).to.equal('error');
        done();
      });
    });

    it('should succeed when removing a test token', (done) => {
      const arbitraryToken = { use: 1, service: kArbitraryService };
      tokenRegistryMock.getToken.callsArgWithAsync(1, arbitraryToken);
      serviceRegistryMock.getService.callsArgWithAsync(1, kArbitraryService);
      tokenRegistryMock.removeToken.callsArgAsync(1);
      rpcPublic.deleteToken('1', (type, message) => {
        expect(type).to.equal('callback');
        expect(message).to.deep.equal(arbitraryToken);
        expect(tokenRegistryMock.updateToken.callCount).to.equal(1);
        done();
      });
    });

    it('should succeed when removing a token', (done) => {
      const arbitraryToken = { service: kArbitraryService };
      tokenRegistryMock.getToken.callsArgWithAsync(1, arbitraryToken);
      tokenRegistryMock.removeToken.callsArgAsync(1);
      rpcPublic.deleteToken('1', (type, message) => {
        expect(type).to.equal('callback');
        expect(message).to.deep.equal(arbitraryToken);
        expect(tokenRegistryMock.removeToken.callCount).to.equal(1);
        done();
      });
    });
  });

  it('should add new erizo controllers', () => {
    const callback = sinon.stub();
    cloudHandlerMock.addNewErizoController.callsArgWith(1, 'id1');
    rpcPublic.addNewErizoController('message', callback);
    expect(callback.withArgs('callback', 'id1').callCount).to.equal(1);
    expect(cloudHandlerMock.addNewErizoController.callCount).to.equal(1);
  });

  it('should send keepAlives', () => {
    const callback = sinon.stub();
    cloudHandlerMock.keepAlive.callsArgWith(1, 'result1');
    rpcPublic.keepAlive('message', callback);
    expect(callback.withArgs('callback', 'result1').callCount).to.equal(1);
    expect(cloudHandlerMock.keepAlive.callCount).to.equal(1);
  });

  it('should set info', () => {
    const callback = sinon.stub();
    rpcPublic.setInfo('params1', callback);
    expect(callback.withArgs('callback').callCount).to.equal(1);
    expect(cloudHandlerMock.setInfo.withArgs('params1').callCount).to.equal(1);
  });

  it('should kill cloud handler', () => {
    const callback = sinon.stub();
    rpcPublic.killMe('ip1', callback);
    expect(callback.withArgs('callback').callCount).to.equal(1);
    expect(cloudHandlerMock.killMe.withArgs('ip1').callCount).to.equal(1);
  });
});
