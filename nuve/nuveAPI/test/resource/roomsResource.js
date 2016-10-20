/*global require, describe, it, beforeEach, afterEach*/
'use strict';
var mocks = require('../utils');
var request = require('supertest');
var express = require('express');
var bodyParser = require('body-parser');
var expect  = require('chai').expect;

var kArbitraryRoom;
var kArbitraryService;
var kTestRoom = {name:'', options: {test: true}};

describe('Rooms Resource', function() {
  var app,
      roomsResource,
      serviceRegistryMock,
      nuveAuthenticatorMock,
      roomRegistryMock;

  beforeEach(function() {
    mocks.start(mocks.licodeConfig);
    serviceRegistryMock = mocks.start(mocks.serviceRegistry);
    roomRegistryMock = mocks.start(mocks.roomRegistry);
    nuveAuthenticatorMock = mocks.start(mocks.nuveAuthenticator);

    roomsResource = require('../../resource/roomsResource');

    kArbitraryRoom = {'_id': '1', name: '', options: {p2p: true, data: ''}};
    kArbitraryService = {'_id': '1', rooms: [kArbitraryRoom]};

    app = express();
    app.use(bodyParser.json());
    app.get('/rooms/', roomsResource.represent);
    app.post('/rooms/', roomsResource.createRoom);
  });

  afterEach(function() {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(roomRegistryMock);
    mocks.stop(serviceRegistryMock);
    mocks.stop(nuveAuthenticatorMock);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  describe('Get Room List', function() {
    it('should fail if service is not present', function(done) {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryService);
      nuveAuthenticatorMock.service = undefined;
      request(app)
        .get('/rooms')
        .expect(404, 'Service not found')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });

    it('should return room if it exists', function(done) {
      nuveAuthenticatorMock.service = kArbitraryService;
      request(app)
        .get('/rooms')
        .expect(200, JSON.stringify(kArbitraryService.rooms))
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });
  });

  describe('Create Room', function() {
    it('should fail if service is not present', function(done) {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryService);
      nuveAuthenticatorMock.service = undefined;
      request(app)
        .post('/rooms')
        .expect(404, 'Service not found')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });

    it('should fail if room name is not present', function(done) {
      nuveAuthenticatorMock.service = kArbitraryService;
      request(app)
        .post('/rooms')
        .send({})
        .expect(400, 'Invalid room')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });

    it('should create test rooms', function(done) {
      nuveAuthenticatorMock.service = kArbitraryService;
      roomRegistryMock.addRoom.callsArgWith(1, kTestRoom);
      request(app)
        .post('/rooms')
        .send(kTestRoom)
        .expect(200, JSON.stringify(kTestRoom))
        .end(function(err) {
          if (err) throw err;
          expect(roomRegistryMock.addRoom.called).to.be.true;  // jshint ignore:line
          expect(serviceRegistryMock.updateService.called).to.be.true;  // jshint ignore:line
          done();
        });
    });

    it('should reuse test rooms', function(done) {
      nuveAuthenticatorMock.service = kArbitraryService;
      nuveAuthenticatorMock.service.testRoom = kTestRoom;
      request(app)
        .post('/rooms')
        .send(kTestRoom)
        .expect(200, JSON.stringify(kTestRoom))
        .end(function(err) {
          if (err) throw err;
          expect(roomRegistryMock.addRoom.called).to.be.false;  // jshint ignore:line
          done();
        });
    });

    it('should create normal rooms', function(done) {
      nuveAuthenticatorMock.service = kArbitraryService;
      roomRegistryMock.addRoom.callsArgWith(1, kTestRoom);
      request(app)
        .post('/rooms')
        .send(kArbitraryRoom)
        .expect(200, JSON.stringify(kTestRoom))
        .end(function(err) {
          if (err) throw err;
          expect(roomRegistryMock.addRoom.called).to.be.true;  // jshint ignore:line
          expect(serviceRegistryMock.updateService.called).to.be.true;  // jshint ignore:line
          done();
        });
    });
  });
});
