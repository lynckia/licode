/* global require, describe, it, beforeEach, afterEach */


const mocks = require('./utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;

describe('Cloud Handler', () => {
  let awssdkMock;
  let erizoControllerRegistryMock;
  let ec2MetadataServiceMock;
  let cloudHandler;

  beforeEach(() => {
    mocks.start(mocks.licodeConfig);
    erizoControllerRegistryMock = mocks.start(mocks.erizoControllerRegistry);
    awssdkMock = mocks.start(mocks.awssdk);
    ec2MetadataServiceMock = mocks.ec2MetadataService;
    // eslint-disable-next-line global-require
    cloudHandler = require('../cloudHandler');
  });

  afterEach(() => {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(erizoControllerRegistryMock);
    mocks.stop(awssdkMock);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  const createPrivateErizoController = (callback) => {
    const arbitraryMessage = {
      _id: '1',
      cloudProvider: '',
      ip: '127.0.0.1',
      hostname: 'hostname',
      port: 1000,
      ssl: true,
    };
    const expectedResult = {
      id: '1',
      publicIP: arbitraryMessage.ip,
      hostname: arbitraryMessage.hostname,
      port: arbitraryMessage.port,
      ssl: arbitraryMessage.ssl,
    };
    erizoControllerRegistryMock.addErizoController.callsArgWith(1, arbitraryMessage);
    cloudHandler.addNewErizoController(arbitraryMessage, callback);
    expect(callback.withArgs(expectedResult).callCount).to.equal(1);
  };

  it('should succeed creating new private erizo controller', () => {
    const callback = sinon.stub();
    createPrivateErizoController(callback);
    expect(callback.callCount).to.equal(1);
  });

  it('should fail creating new amazon erizo controller if aws-sdk fails', () => {
    const arbitraryMessage = {
      cloudProvider: 'amazon',
      ip: '127.0.0.1',
      hostname: 'hostname',
      port: 1000,
      ssl: true,
    };
    const callback = sinon.stub();
    ec2MetadataServiceMock.request.callsArgWith(1, 'error');
    cloudHandler.addNewErizoController(arbitraryMessage, callback);
    expect(callback.withArgs('error').callCount).to.equal(1);
  });

  it('should succeed creating new amazon erizo controller if aws-sdk success', () => {
    const arbitraryMessage = {
      _id: '1',
      cloudProvider: 'amazon',
      ip: '127.0.0.1',
      hostname: 'hostname',
      port: 1000,
      ssl: true,
    };
    const callback = sinon.stub();
    ec2MetadataServiceMock.request.callsArgWith(1, undefined, '127.0.0.2');
    erizoControllerRegistryMock.addErizoController.callsArgWith(1, arbitraryMessage);
    cloudHandler.addNewErizoController(arbitraryMessage, callback);
    const expectedResult = {
      id: '1',
      publicIP: '127.0.0.2',
      hostname: arbitraryMessage.hostname,
      port: arbitraryMessage.port,
      ssl: arbitraryMessage.ssl,
    };
    expect(callback.withArgs(expectedResult).callCount).to.equal(1);
  });

  it('should return "whoareyou" when receiving keepAlives ' +
                    'from unknown erizo controller', () => {
    const callback = sinon.stub();
    erizoControllerRegistryMock.getErizoController.callsArgWith(1, undefined);
    cloudHandler.keepAlive('1', callback);
    expect(callback.withArgs('whoareyou').callCount).to.equal(1);
  });

  it('should return "ok" when receiving keepAlives ' +
                    'from known erizo controller', () => {
    const arbitraryMessage = {
      _id: '1',
      cloudProvider: '',
      ip: '127.0.0.1',
      hostname: 'hostname',
      port: 1000,
      ssl: true,
    };
    const callback = sinon.stub();
    createPrivateErizoController(sinon.stub());
    erizoControllerRegistryMock.getErizoController.callsArgWith(1, arbitraryMessage);
    cloudHandler.keepAlive('1', callback);
    expect(callback.withArgs('ok').callCount).to.equal(1);
  });
});
