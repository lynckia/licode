/* global require, describe, it, beforeEach, afterEach, before, after */

/* eslint-disable no-unused-expressions, global-require */

const mocks = require('../utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;

describe('Erizo Controller / Token Authenticator', () => {
  let amqperMock;
  let cryptoMock;
  let socketIoMock;
  let signatureMock;
  let processArgsBackup;
  let roomControllerMock;
  let authenticator;

  const errorWithMessage = message => sinon.match.instanceOf(Error).and(sinon.match.has('message', `token: ${message}`));

  // Allows passing arguments to mocha
  before(() => {
    processArgsBackup = [...process.argv];
    process.argv = [];
  });

  after(() => {
    process.argv = [...processArgsBackup];
  });

  beforeEach(() => {
    global.config = { logger: { configFile: true } };
    global.config.erizoController = {};
    global.config.nuve = { superserviceKey: 'arbitraryNuveKey' };
    signatureMock = mocks.signature;
    amqperMock = mocks.start(mocks.amqper);
    cryptoMock = mocks.start(mocks.crypto);
    roomControllerMock = mocks.start(mocks.roomController);
    socketIoMock = mocks.start(mocks.socketIo);
    authenticator = require('../../erizoController/tokenAuthenticator');
  });

  afterEach(() => {
    mocks.stop(roomControllerMock);
    mocks.stop(socketIoMock);
    mocks.stop(cryptoMock);
    mocks.stop(amqperMock);
    mocks.deleteRequireCache();
    mocks.reset();
    global.config = {};
  });

  describe('Authenticate', () => {
    const arbitrarySignature = 'c2lnbmF0dXJl'; // signature
    const arbitraryGoodToken = {
      tokenId: 'tokenId',
      host: 'host',
      signature: arbitrarySignature,
    };
    const arbitraryBadToken = {
      tokenId: 'tokenId',
      host: 'host',
      signature: 'badSignature',
    };

    beforeEach(() => {
      signatureMock.update.returns(signatureMock);
      signatureMock.digest.returns('signature');
    });

    it('should fail if signature is wrong', () => {
      const callback = sinon.stub();
      mocks.socketInstance.handshake = { query: arbitraryBadToken };
      authenticator(roomControllerMock, mocks.socketInstance, callback);

      expect(callback.withArgs(errorWithMessage('Authentication error')).callCount).to.equal(1);
    });

    it('should call Nuve if signature is ok', () => {
      const callback = sinon.stub();

      mocks.socketInstance.handshake = { query: arbitraryGoodToken };
      authenticator(roomControllerMock, mocks.socketInstance, callback);

      expect(amqperMock.callRpc
        .withArgs('nuve', 'deleteToken', arbitraryGoodToken.tokenId).callCount).to.equal(1);
    });

    describe('when deleted', () => {
      let onTokenCallback;
      let callback;

      beforeEach(() => {
        onTokenCallback = sinon.stub();
        mocks.socketInstance.handshake = { query: arbitraryGoodToken };
        authenticator(roomControllerMock, mocks.socketInstance, onTokenCallback);

        callback = amqperMock.callRpc
          .withArgs('nuve', 'deleteToken', arbitraryGoodToken.tokenId)
          .args[0][3].callback;
      });

      it('should disconnect socket on Nuve error', (done) => {
        callback('error');
        setTimeout(() => {
          expect(onTokenCallback.withArgs(errorWithMessage('Token does not exist')).callCount).to.equal(1);
          expect(mocks.socketInstance.disconnect.callCount).to.equal(1);
          done();
        }, 0);
      });

      it('should disconnect socket on Nuve timeout', (done) => {
        callback('timeout');
        setTimeout(() => {
          expect(mocks.socketInstance.disconnect.callCount).to.equal(1);
          expect(onTokenCallback.withArgs(errorWithMessage('Nuve does not respond')).callCount).to.equal(1);
          done();
        }, 0);
      });

      it('should disconnect if Token has an invalid host', (done) => {
        callback({ host: 'invalidHost', room: 'roomId' });
        setTimeout(() => {
          expect(onTokenCallback.withArgs(errorWithMessage('Invalid host')).callCount).to.equal(1);
          done();
        }, 0);
      });
    });
  });
});

