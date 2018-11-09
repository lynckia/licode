/* global require, describe, it, beforeEach, afterEach */


const mocks = require('../utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const request = require('supertest');
// eslint-disable-next-line import/no-extraneous-dependencies
const express = require('express');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');
// eslint-disable-next-line import/no-extraneous-dependencies
const bodyParser = require('body-parser');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;

const kArbitraryService = { _id: '1', rooms: [] };

describe('Services Resource', () => {
  let app;
  let servicesResource;
  let serviceRegistryMock;
  let nuveAuthenticatorMock;
  let setServiceStub;
  let dataBaseMock;

  beforeEach(() => {
    mocks.start(mocks.licodeConfig);
    serviceRegistryMock = mocks.start(mocks.serviceRegistry);
    dataBaseMock = mocks.start(mocks.dataBase);
    nuveAuthenticatorMock = mocks.start(mocks.nuveAuthenticator);
    setServiceStub = sinon.stub();
    // eslint-disable-next-line global-require
    servicesResource = require('../../resource/servicesResource');

    app = express();
    app.use(bodyParser.json());
    app.all('*', (req, res, next) => {
      req.service = setServiceStub();
      next();
    });
    app.post('/services', servicesResource.create);
    app.get('/services', servicesResource.represent);
  });

  afterEach(() => {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(dataBaseMock);
    mocks.stop(serviceRegistryMock);
    mocks.stop(nuveAuthenticatorMock);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  describe('Get List of Services', () => {
    it('should fail if service is not super service', (done) => {
      setServiceStub.returns(kArbitraryService);
      dataBaseMock.superService = undefined;
      request(app)
        .get('/services')
        .expect(401, 'Service not authorized for this action')
        .end((err) => {
          if (err) throw err;
          done();
        });
    });

    it('should fail if service does not exist', (done) => {
      setServiceStub.returns(kArbitraryService);
      dataBaseMock.superService = kArbitraryService._id;
      serviceRegistryMock.getList.callsArgWith(0, [kArbitraryService]);
      request(app)
        .get('/services')
        .expect(200, JSON.stringify([kArbitraryService]))
        .end((err) => {
          if (err) throw err;
          done();
        });
    });
  });

  describe('Create Service', () => {
    it('should fail if service is not super service', (done) => {
      setServiceStub.returns(kArbitraryService);
      dataBaseMock.superService = undefined;
      request(app)
        .post('/services')
        .expect(401, 'Service not authorized for this action')
        .end((err) => {
          if (err) throw err;
          done();
        });
    });

    it('should succeed if service exists', (done) => {
      setServiceStub.returns(kArbitraryService);
      dataBaseMock.superService = kArbitraryService._id;
      serviceRegistryMock.addService.callsArgWith(1, kArbitraryService);
      request(app)
        .post('/services')
        .expect(200, JSON.stringify(kArbitraryService))
        .end((err) => {
          if (err) throw err;
          // eslint-disable-next-line no-unused-expressions
          expect(serviceRegistryMock.addService.called).to.be.true;
          done();
        });
    });
  });
});
