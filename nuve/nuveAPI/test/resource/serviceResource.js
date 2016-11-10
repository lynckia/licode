/*global require, describe, it, beforeEach, afterEach*/
'use strict';
var mocks = require('../utils');
var request = require('supertest');
var express = require('express');
var sinon = require('sinon');
var bodyParser = require('body-parser');
var expect  = require('chai').expect;

var kArbitraryService = {'_id': '1', rooms: []};

describe('Service Resource', function() {
  var app,
      serviceResource,
      serviceRegistryMock,
      nuveAuthenticatorMock,
      setServiceStub,
      dataBaseMock;

  beforeEach(function() {
    mocks.start(mocks.licodeConfig);
    serviceRegistryMock = mocks.start(mocks.serviceRegistry);
    dataBaseMock = mocks.start(mocks.dataBase);
    nuveAuthenticatorMock = mocks.start(mocks.nuveAuthenticator);
    setServiceStub = sinon.stub();
    serviceResource = require('../../resource/serviceResource');

    app = express();
    app.use(bodyParser.json());
    app.all('*', function(req, res, next) {
      req.service = setServiceStub();
      next();
    });
    app.get('/services/:service', serviceResource.represent);
    app.delete('/services/:service', serviceResource.deleteService);
  });

  afterEach(function() {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(dataBaseMock);
    mocks.stop(serviceRegistryMock);
    mocks.stop(nuveAuthenticatorMock);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  describe('Get Service', function() {
    it('should fail if service is not super service', function(done) {
      setServiceStub.returns(kArbitraryService);
      dataBaseMock.superService = undefined;
      request(app)
        .get('/services/1')
        .expect(401, 'Service not authorized for this action')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });

    it('should fail if service does not exist', function(done) {
      setServiceStub.returns(kArbitraryService);
      dataBaseMock.superService = kArbitraryService._id;
      serviceRegistryMock.getService.callsArgWith(1, undefined);
      request(app)
        .get('/services/1')
        .expect(404, 'Service not found')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });

    it('should succeed if service exists', function(done) {
      setServiceStub.returns(kArbitraryService);
      dataBaseMock.superService = kArbitraryService._id;
      serviceRegistryMock.getService.callsArgWith(1, kArbitraryService);
      request(app)
        .get('/services/1')
        .expect(200, JSON.stringify(kArbitraryService))
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });
  });

  describe('Delete Service', function() {
    it('should fail if service is not super service', function(done) {
      setServiceStub.returns(kArbitraryService);
      dataBaseMock.superService = undefined;
      request(app)
        .delete('/services/1')
        .expect(401, 'Service not authorized for this action')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });

    it('should fail if service does not exist', function(done) {
      setServiceStub.returns(kArbitraryService);
      dataBaseMock.superService = kArbitraryService._id;
      serviceRegistryMock.getService.callsArgWith(1, undefined);
      request(app)
        .delete('/services/1')
        .expect(404, 'Service not found')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });

    it('should fail if service does not exist', function(done) {
      setServiceStub.returns(kArbitraryService);
      dataBaseMock.superService = kArbitraryService._id;
      serviceRegistryMock.getService.callsArgWith(1, kArbitraryService);
      request(app)
        .delete('/services/1')
        .expect(200, 'Service deleted')
        .end(function(err) {
          if (err) throw err;
          expect(serviceRegistryMock.getService.called).to.be.true;  // jshint ignore:line
          done();
        });
    });
  });
});
