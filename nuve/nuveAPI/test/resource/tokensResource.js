/*global require, describe, it, beforeEach, afterEach*/
'use strict';
var mocks = require('../utils');
var request = require('supertest');
var express = require('express');
var bodyParser = require('body-parser');
var sinon = require('sinon');
var kArbitraryRoom = {'_id': '1', name: '', options: {p2p: true, data: ''}};
var kArbitraryService = {'_id': '1', rooms: [kArbitraryRoom]};
var kArbitraryErizoController = {hostname: 'hostname', ssl: true, ip: '127.0.0.1', port: 3000};

describe('Tokens Resource', function() {
  var app,
      tokensResource,
      serviceRegistryMock,
      tokenRegistryMock,
      nuveAuthenticatorMock,
      setServiceStub,
      setUserStub,
      dataBaseMock,
      cloudHandlerMock;

  beforeEach(function() {
    mocks.start(mocks.licodeConfig);
    cloudHandlerMock = mocks.start(mocks.cloudHandler);
    serviceRegistryMock = mocks.start(mocks.serviceRegistry);
    tokenRegistryMock = mocks.start(mocks.tokenRegistry);
    dataBaseMock = mocks.start(mocks.dataBase);
    nuveAuthenticatorMock = mocks.start(mocks.nuveAuthenticator);
    setServiceStub = sinon.stub();
    setUserStub = sinon.stub();
    tokensResource = require('../../resource/tokensResource');

    app = express();
    app.use(bodyParser.json());
    app.all('*', function(req, res, next) {
      req.service = setServiceStub();
      req.user = setUserStub();
      next();
    });
    app.post('/rooms/:room/tokens', tokensResource.create);
  });

  afterEach(function() {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(cloudHandlerMock);
    mocks.stop(dataBaseMock);
    mocks.stop(tokenRegistryMock);
    mocks.stop(serviceRegistryMock);
    mocks.stop(nuveAuthenticatorMock);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  describe('Create Token', function() {
    it('should fail if service is not found', function(done) {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryRoom);
      request(app)
        .post('/rooms/1/tokens')
        .expect(404, 'Service not found')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });

    it('should fail if room is not found', function(done) {
      setServiceStub.returns(kArbitraryService);
      serviceRegistryMock.getRoomForService.callsArgWith(2, undefined);
      request(app)
        .post('/rooms/1/tokens')
        .expect(404, 'Room does not exist')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });

    it('should fail if token does not contain user', function(done) {
      setServiceStub.returns(kArbitraryService);
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryRoom);
      request(app)
        .post('/rooms/1/tokens')
        .expect(401, 'Name and role?')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });

    it('should fail if Erizo Controller is not working', function(done) {
      setServiceStub.returns(kArbitraryService);
      setUserStub.returns('username');
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryRoom);
      cloudHandlerMock.getErizoControllerForRoom.callsArgWith(1, 'timeout');
      request(app)
        .post('/rooms/1/tokens')
        .expect(404, 'No Erizo Controller found')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });


    it('should succeed if service and room exists', function(done) {
      setServiceStub.returns(kArbitraryService);
      setUserStub.returns('username');
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryRoom);
      cloudHandlerMock.getErizoControllerForRoom.callsArgWith(1, kArbitraryErizoController);
      var token = 'eyJ0b2tlbklkIjoic3RyaW5nIiwiaG9zdCI6Imhvc3RuYW1lOjMwMDAiLCJzZWN1cmUiOnRyd' +
                  'WUsInNpZ25hdHVyZSI6Ik5UYzBORGRpTWpneE5ERXhZek5oTkRVMlkyRXpZVFF3TVRGbE1HVT' +
                  'FZelJoWVRoaE1UQXlaUT09In0=';
      tokenRegistryMock.addToken.callsArgWith(1, 'string');
      request(app)
        .post('/rooms/1/tokens')
        .expect(200, token)
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });
  });
});
