/* global require, describe, it, beforeEach, afterEach */


const mocks = require('../utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const request = require('supertest');
// eslint-disable-next-line import/no-extraneous-dependencies
const express = require('express');
// eslint-disable-next-line import/no-extraneous-dependencies
const bodyParser = require('body-parser');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');

const kArbitraryRoom = { _id: '1', name: '', options: { p2p: true, data: '' } };
const kArbitraryService = { _id: '1', rooms: [kArbitraryRoom] };
const kArbitraryErizoController = { hostname: 'hostname', ssl: true, ip: '127.0.0.1', port: 3000 };

describe('Tokens Resource', () => {
  let app;
  let tokensResource;
  let serviceRegistryMock;
  let tokenRegistryMock;
  let nuveAuthenticatorMock;
  let setServiceStub;
  let setUserStub;
  let dataBaseMock;
  let cloudHandlerMock;

  beforeEach(() => {
    mocks.start(mocks.licodeConfig);
    cloudHandlerMock = mocks.start(mocks.cloudHandler);
    serviceRegistryMock = mocks.start(mocks.serviceRegistry);
    tokenRegistryMock = mocks.start(mocks.tokenRegistry);
    dataBaseMock = mocks.start(mocks.dataBase);
    nuveAuthenticatorMock = mocks.start(mocks.nuveAuthenticator);
    setServiceStub = sinon.stub();
    setUserStub = sinon.stub();
    // eslint-disable-next-line global-require
    tokensResource = require('../../resource/tokensResource');

    app = express();
    app.use(bodyParser.json());
    app.all('*', (req, res, next) => {
      req.service = setServiceStub();
      req.user = setUserStub();
      next();
    });
    app.post('/rooms/:room/tokens', tokensResource.create);
  });

  afterEach(() => {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(cloudHandlerMock);
    mocks.stop(dataBaseMock);
    mocks.stop(tokenRegistryMock);
    mocks.stop(serviceRegistryMock);
    mocks.stop(nuveAuthenticatorMock);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  describe('Create Token', () => {
    it('should fail if service is not found', (done) => {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryRoom);
      request(app)
        .post('/rooms/1/tokens')
        .expect(404, 'Service not found')
        .end((err) => {
          if (err) throw err;
          done();
        });
    });

    it('should fail if room is not found', (done) => {
      setServiceStub.returns(kArbitraryService);
      serviceRegistryMock.getRoomForService.callsArgWith(2, undefined);
      request(app)
        .post('/rooms/1/tokens')
        .expect(404, 'Room does not exist')
        .end((err) => {
          if (err) throw err;
          done();
        });
    });

    it('should fail if token does not contain user', (done) => {
      setServiceStub.returns(kArbitraryService);
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryRoom);
      request(app)
        .post('/rooms/1/tokens')
        .expect(401, 'Name and role?')
        .end((err) => {
          if (err) throw err;
          done();
        });
    });

    it('should fail if Erizo Controller is not working', (done) => {
      setServiceStub.returns(kArbitraryService);
      setUserStub.returns('username');
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryRoom);
      cloudHandlerMock.getErizoControllerForRoom.callsArgWith(1, 'timeout');
      request(app)
        .post('/rooms/1/tokens')
        .expect(404, 'No Erizo Controller found')
        .end((err) => {
          if (err) throw err;
          done();
        });
    });


    it('should succeed if service and room exists', (done) => {
      setServiceStub.returns(kArbitraryService);
      setUserStub.returns('username');
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryRoom);
      cloudHandlerMock.getErizoControllerForRoom.callsArgWith(1, kArbitraryErizoController);
      const token = 'eyJ0b2tlbklkIjoic3RyaW5nIiwiaG9zdCI6Imhvc3RuYW1lOjMwMDAiLCJzZWN1cmUiOnRyd' +
                  'WUsInNpZ25hdHVyZSI6Ik5UYzBORGRpTWpneE5ERXhZek5oTkRVMlkyRXpZVFF3TVRGbE1HVT' +
                  'FZelJoWVRoaE1UQXlaUT09In0=';
      tokenRegistryMock.addToken.callsArgWith(1, 'string');
      request(app)
        .post('/rooms/1/tokens')
        .expect(200, token)
        .end((err) => {
          if (err) throw err;
          done();
        });
    });
  });
});
