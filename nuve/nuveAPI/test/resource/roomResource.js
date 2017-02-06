/*global require, describe, it, beforeEach, afterEach*/
'use strict';
var mocks = require('../utils');
var sinon = require('sinon');
var request = require('supertest');
var express = require('express');
var bodyParser = require('body-parser');
var expect  = require('chai').expect;

var kArbitraryRoom = {'_id': '1', name: '', options: {p2p: true, data: ''}};
var kArbitraryService = {'_id': '1', rooms: [kArbitraryRoom]};

describe('Room Resource', function() {
  var app,
      roomResource,
      setServiceStub,
      serviceRegistryMock,
      nuveAuthenticatorMock,
      roomRegistryMock,
      erizoControllerRegistryMock;

  beforeEach(function() {
    mocks.start(mocks.licodeConfig);
    serviceRegistryMock = mocks.start(mocks.serviceRegistry);
    roomRegistryMock = mocks.start(mocks.roomRegistry);
    nuveAuthenticatorMock = mocks.start(mocks.nuveAuthenticator);
    erizoControllerRegistryMock = mocks.start(mocks.erizoControllerRegistry);
    setServiceStub = sinon.stub();
    roomResource = require('../../resource/roomResource');
    app = express();
    app.use(bodyParser.json());
    app.all('*', function(req, res, next) {
      req.service = setServiceStub();
      next();
    });
    app.get('/rooms/:room', roomResource.represent);
    app.put('/rooms/:room', roomResource.updateRoom);
    app.patch('/rooms/:room', roomResource.patchRoom);
    app.delete('/rooms/:room', roomResource.deleteRoom);
  });

  afterEach(function() {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(roomRegistryMock);
    mocks.stop(serviceRegistryMock);
    mocks.stop(nuveAuthenticatorMock);
    mocks.stop(erizoControllerRegistryMock);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  describe('Get Room', function() {
    it('should fail if service is not present', function(done) {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryService);
      request(app)
        .get('/rooms/1')
        .expect(401, 'Client unathorized')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });

    it('should fail if room does not exist', function(done) {
      serviceRegistryMock.getRoomForService.callsArgWith(2, undefined);
      setServiceStub.returns(kArbitraryService);
      request(app)
        .get('/rooms/1')
        .expect(404, 'Room does not exist')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });

    it('should return room if it exists', function(done) {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryRoom);
      setServiceStub.returns(kArbitraryService);
      request(app)
        .get('/rooms/1')
        .expect(200, JSON.stringify(kArbitraryRoom))
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });
  });

  describe('Update Room', function() {
    it('should fail if service is not present', function(done) {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryService);
      request(app)
        .put('/rooms/1')
        .send(kArbitraryRoom)
        .expect(401, 'Client unathorized')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });

    it('should fail if room does not exist', function(done) {
      serviceRegistryMock.getRoomForService.callsArgWith(2, undefined);
      setServiceStub.returns(kArbitraryService);
      request(app)
        .put('/rooms/1')
        .send(kArbitraryRoom)
        .expect(404, 'Room does not exist')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });

    it('should update room if it exists', function(done) {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryRoom);
      setServiceStub.returns(kArbitraryService);
      request(app)
        .put('/rooms/1')
        .send(kArbitraryRoom)
        .expect(200, 'Room Updated')
        .end(function(err) {
          if (err) throw err;
          expect(roomRegistryMock.updateRoom.calledOnce).to.be.true;  // jshint ignore:line
          done();
        });
    });
  });

  describe('Patch Room', function() {
    it('should fail if service is not present', function(done) {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryService);
      request(app)
        .patch('/rooms/1')
        .send(kArbitraryRoom)
        .expect(401, 'Client unathorized')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });

    it('should fail if room does not exist', function(done) {
      serviceRegistryMock.getRoomForService.callsArgWith(2, undefined);
      setServiceStub.returns(kArbitraryService);
      request(app)
        .patch('/rooms/1')
        .send(kArbitraryRoom)
        .expect(404, 'Room does not exist')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });

    it('should patch room if it exists', function(done) {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryRoom);
      setServiceStub.returns(kArbitraryService);
      request(app)
        .patch('/rooms/1')
        .send(kArbitraryRoom)
        .expect(200, 'Room Updated')
        .end(function(err) {
          if (err) throw err;
          expect(roomRegistryMock.updateRoom.calledOnce).to.be.true;  // jshint ignore:line
          done();
        });
    });
  });

  describe('Delete Room', function() {
    it('should fail if service is not present', function(done) {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryService);
      request(app)
        .delete('/rooms/1')
        .send()
        .expect(401, 'Client unathorized')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });

    it('should fail if room does not exist', function(done) {
      serviceRegistryMock.getRoomForService.callsArgWith(2, undefined);
      setServiceStub.returns(kArbitraryService);
      request(app)
        .delete('/rooms/1')
        .expect(404, 'Room does not exist')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });

    it('should delete room if it exists', function(done) {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryRoom);
      setServiceStub.returns(kArbitraryService);
      request(app)
        .delete('/rooms/1')
        .expect(200, 'Room deleted')
        .end(function(err) {
          if (err) throw err;
          expect(roomRegistryMock.removeRoom.calledOnce).to.be.true;  // jshint ignore:line
          done();
        });
    });
  });
});
