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

let kArbitraryRoom;
let kArbitraryService;
const kTestRoom = { name: '', options: { test: true } };

describe('Rooms Resource', () => {
  let app;
  let roomsResource;
  let setServiceStub;
  let serviceRegistryMock;
  let nuveAuthenticatorMock;
  let roomRegistryMock;

  beforeEach(() => {
    mocks.start(mocks.licodeConfig);
    serviceRegistryMock = mocks.start(mocks.serviceRegistry);
    roomRegistryMock = mocks.start(mocks.roomRegistry);
    nuveAuthenticatorMock = mocks.start(mocks.nuveAuthenticator);
    setServiceStub = sinon.stub();
    // eslint-disable-next-line global-require
    roomsResource = require('../../resource/roomsResource');

    kArbitraryRoom = { _id: '1', name: '', options: { p2p: true, data: '' } };
    kArbitraryService = { _id: '1', rooms: [kArbitraryRoom] };

    app = express();
    app.use(bodyParser.json());
    app.all('*', (req, res, next) => {
      req.service = setServiceStub();
      next();
    });
    app.get('/rooms/', roomsResource.represent);
    app.post('/rooms/', roomsResource.createRoom);
  });

  afterEach(() => {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(roomRegistryMock);
    mocks.stop(serviceRegistryMock);
    mocks.stop(nuveAuthenticatorMock);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  describe('Get Room List', () => {
    it('should fail if service is not present', (done) => {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryService);
      request(app)
        .get('/rooms')
        .expect(404, 'Service not found')
        .end((err) => {
          if (err) throw err;
          done();
        });
    });

    it('should return room if it exists', (done) => {
      setServiceStub.returns(kArbitraryService);
      request(app)
        .get('/rooms')
        .expect(200, JSON.stringify(kArbitraryService.rooms))
        .end((err) => {
          if (err) throw err;
          done();
        });
    });
  });

  describe('Create Room', () => {
    it('should fail if service is not present', (done) => {
      serviceRegistryMock.getRoomForService.callsArgWith(2, kArbitraryService);
      request(app)
        .post('/rooms')
        .expect(404, 'Service not found')
        .end((err) => {
          if (err) throw err;
          done();
        });
    });

    it('should fail if room name is not present', (done) => {
      setServiceStub.returns(kArbitraryService);
      request(app)
        .post('/rooms')
        .send({})
        .expect(400, 'Invalid room')
        .end((err) => {
          if (err) throw err;
          done();
        });
    });

    it('should create test rooms', (done) => {
      setServiceStub.returns(kArbitraryService);
      roomRegistryMock.addRoom.callsArgWith(1, kTestRoom);
      request(app)
        .post('/rooms')
        .send(kTestRoom)
        .expect(200, JSON.stringify(kTestRoom))
        .end((err) => {
          if (err) throw err;
          // eslint-disable-next-line no-unused-expressions
          expect(roomRegistryMock.addRoom.called).to.be.true;
          // eslint-disable-next-line no-unused-expressions
          expect(serviceRegistryMock.addRoomToService.called).to.be.true;
          done();
        });
    });

    it('should reuse test rooms', (done) => {
      kArbitraryService.testRoom = kTestRoom;
      setServiceStub.returns(kArbitraryService);
      request(app)
        .post('/rooms')
        .send(kTestRoom)
        .expect(200, JSON.stringify(kTestRoom))
        .end((err) => {
          if (err) throw err;
          // eslint-disable-next-line no-unused-expressions
          expect(roomRegistryMock.addRoom.called).to.be.false;
          done();
        });
    });

    it('should create normal rooms', (done) => {
      setServiceStub.returns(kArbitraryService);
      roomRegistryMock.addRoom.callsArgWith(1, kTestRoom);
      request(app)
        .post('/rooms')
        .send(kArbitraryRoom)
        .expect(200, JSON.stringify(kTestRoom))
        .end((err) => {
          if (err) throw err;
          // eslint-disable-next-line no-unused-expressions
          expect(roomRegistryMock.addRoom.called).to.be.true;
          // eslint-disable-next-line no-unused-expressions
          expect(serviceRegistryMock.addRoomToService.called).to.be.true;
          done();
        });
    });
  });
});
