/*global require, describe, it, beforeEach, afterEach*/
'use strict';
var mocks = require('../utils');
var request = require('supertest');
var express = require('express');
var sinon = require('sinon');
var bodyParser = require('body-parser');
var expect  = require('chai').expect;

var kArbitraryService = {'_id': '1', rooms: []};

describe('Services Resource', function() {
  var app,
      servicesResource,
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
    servicesResource = require('../../resource/servicesResource');

    app = express();
    app.use(bodyParser.json());
    app.all('*', function(req, res, next) {
      req.service = setServiceStub();
      next();
    });
    app.post('/services', servicesResource.create);
    app.get('/services', servicesResource.represent);
  });

  afterEach(function() {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(dataBaseMock);
    mocks.stop(serviceRegistryMock);
    mocks.stop(nuveAuthenticatorMock);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  describe('Get List of Services', function() {
    it('should fail if service is not super service', function(done) {
      setServiceStub.returns(kArbitraryService);
      dataBaseMock.superService = undefined;
      request(app)
        .get('/services')
        .expect(401, 'Service not authorized for this action')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });

    it('should fail if service does not exist', function(done) {
      setServiceStub.returns(kArbitraryService);
      dataBaseMock.superService = kArbitraryService._id;
      serviceRegistryMock.getList.callsArgWith(0, [kArbitraryService]);
      request(app)
        .get('/services')
        .expect(200, JSON.stringify([kArbitraryService]))
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });
  });

  describe('Create Service', function() {
    it('should fail if service is not super service', function(done) {
      setServiceStub.returns(kArbitraryService);
      dataBaseMock.superService = undefined;
      request(app)
        .post('/services')
        .expect(401, 'Service not authorized for this action')
        .end(function(err) {
          if (err) throw err;
          done();
        });
    });

    it('should succeed if service exists', function(done) {
      setServiceStub.returns(kArbitraryService);
      dataBaseMock.superService = kArbitraryService._id;
      serviceRegistryMock.addService.callsArgWith(1, kArbitraryService);
      request(app)
        .post('/services')
        .expect(200, JSON.stringify(kArbitraryService))
        .end(function(err) {
          if (err) throw err;
          expect(serviceRegistryMock.addService.called).to.be.true;  // jshint ignore:line
          done();
        });
    });
  });
});
