/* global require, describe, it, beforeEach, afterEach */

const mocks = require('../utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');
// eslint-disable-next-line import/no-extraneous-dependencies
const request = require('supertest');
// eslint-disable-next-line import/no-extraneous-dependencies
const express = require('express');
// eslint-disable-next-line import/no-extraneous-dependencies
const bodyParser = require('body-parser');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;

const kArbitraryRoom = { _id: '1', name: '', options: { p2p: true, data: '' } };
const kArbitraryService = { _id: '1', rooms: [kArbitraryRoom] };

describe('Room Resource', () => {
  let app;
  let roomResource;
  let setServiceStub;
  let serviceRegistryMock;
  let nuveAuthenticatorMock;
  let roomRegistryMock;
  let erizoControllerRegistryMock;

  beforeEach(() => {
    mocks.start(mocks.licodeConfig);
    serviceRegistryMock = mocks.start(mocks.serviceRegistry);
    roomRegistryMock = mocks.start(mocks.roomRegistry);
    nuveAuthenticatorMock = mocks.start(mocks.nuveAuthenticator);
    erizoControllerRegistryMock = mocks.start(mocks.erizoControllerRegistry);
    setServiceStub = sinon.stub();
    // eslint-disable-next-line global-require
    roomResource = require('../../resource/roomResource');
    app = express();
    app.use(bodyParser.json());
    app.all('*', (req, res, next) => {
      req.service = setServiceStub();
      next();
    });
    app.get('/rooms/:room', roomResource.represent);
    app.put('/rooms/:room', roomResource.updateRoom);
    app.patch('/rooms/:room', roomResource.patchRoom);
    app.delete('/rooms/:room', roomResource.deleteRoom);
  });

  afterEach(() => {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(roomRegistryMock);
    mocks.stop(serviceRegistryMock);
    mocks.stop(nuveAuthenticatorMock);
    mocks.stop(erizoControllerRegistryMock);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  describe('Get Room', () => {
    it('should fail if service is not present', (done) => {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryService);
      request(app)
        .get('/rooms/1')
        .expect(401, 'Client unathorized')
        .end((err) => {
          if (err) throw err;
          done();
        });
    });

    it('should fail if room does not exist', (done) => {
      serviceRegistryMock.getRoomForService.callsArgWith(2, undefined);
      setServiceStub.returns(kArbitraryService);
      request(app)
        .get('/rooms/1')
        .expect(404, 'Room does not exist')
        .end((err) => {
          if (err) throw err;
          done();
        });
    });

    it('should return room if it exists', (done) => {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryRoom);
      setServiceStub.returns(kArbitraryService);
      request(app)
        .get('/rooms/1')
        .expect(200, JSON.stringify(kArbitraryRoom))
        .end((err) => {
          if (err) throw err;
          done();
        });
    });
  });

  describe('Update Room', () => {
    it('should fail if service is not present', (done) => {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryService);
      request(app)
        .put('/rooms/1')
        .send(kArbitraryRoom)
        .expect(401, 'Client unathorized')
        .end((err) => {
          if (err) throw err;
          done();
        });
    });

    it('should fail if room does not exist', (done) => {
      serviceRegistryMock.getRoomForService.callsArgWith(2, undefined);
      setServiceStub.returns(kArbitraryService);
      request(app)
        .put('/rooms/1')
        .send(kArbitraryRoom)
        .expect(404, 'Room does not exist')
        .end((err) => {
          if (err) throw err;
          done();
        });
    });

    it('should update room if it exists', (done) => {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryRoom);
      setServiceStub.returns(kArbitraryService);
      request(app)
        .put('/rooms/1')
        .send(kArbitraryRoom)
        .expect(200, 'Room Updated')
        .end((err) => {
          if (err) throw err;
          // eslint-disable-next-line no-unused-expressions
          expect(roomRegistryMock.updateRoom.calledOnce).to.be.true;
          done();
        });
    });
  });

  describe('Patch Room', () => {
    it('should fail if service is not present', (done) => {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryService);
      request(app)
        .patch('/rooms/1')
        .send(kArbitraryRoom)
        .expect(401, 'Client unathorized')
        .end((err) => {
          if (err) throw err;
          done();
        });
    });

    it('should fail if room does not exist', (done) => {
      serviceRegistryMock.getRoomForService.callsArgWith(2, undefined);
      setServiceStub.returns(kArbitraryService);
      request(app)
        .patch('/rooms/1')
        .send(kArbitraryRoom)
        .expect(404, 'Room does not exist')
        .end((err) => {
          if (err) throw err;
          done();
        });
    });

    it('should patch room if it exists', (done) => {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryRoom);
      setServiceStub.returns(kArbitraryService);
      request(app)
        .patch('/rooms/1')
        .send(kArbitraryRoom)
        .expect(200, 'Room Updated')
        .end((err) => {
          if (err) throw err;
          // eslint-disable-next-line no-unused-expressions
          expect(roomRegistryMock.updateRoom.calledOnce).to.be.true;
          done();
        });
    });
  });

  describe('Delete Room', () => {
    it('should fail if service is not present', (done) => {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryService);
      request(app)
        .delete('/rooms/1')
        .send()
        .expect(401, 'Client unathorized')
        .end((err) => {
          if (err) throw err;
          done();
        });
    });

    it('should fail if room does not exist', (done) => {
      serviceRegistryMock.getRoomForService.callsArgWith(2, undefined);
      setServiceStub.returns(kArbitraryService);
      request(app)
        .delete('/rooms/1')
        .expect(404, 'Room does not exist')
        .end((err) => {
          if (err) throw err;
          done();
        });
    });

    it('should delete room if it exists', (done) => {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryRoom);
      setServiceStub.returns(kArbitraryService);
      request(app)
        .delete('/rooms/1')
        .expect(200, 'Room deleted')
        .end((err) => {
          if (err) throw err;
          // eslint-disable-next-line no-unused-expressions
          expect(roomRegistryMock.removeRoom.calledOnce).to.be.true;
          done();
        });
    });
  });
});
