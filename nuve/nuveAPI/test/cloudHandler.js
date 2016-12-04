/*global require, describe, it, beforeEach, afterEach*/
'use strict';
var mocks = require('./utils');
var sinon = require('sinon');
var expect  = require('chai').expect;

describe('Cloud Handler', function() {
  var awssdkMock,
      ec2MetadataServiceMock,
      cloudHandler;

  beforeEach(function() {
    mocks.start(mocks.licodeConfig);
    awssdkMock = mocks.start(mocks.awssdk);
    ec2MetadataServiceMock = mocks.ec2MetadataService;
    cloudHandler = require('../cloudHandler');
  });

  afterEach(function() {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(awssdkMock);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  var createPrivateErizoController = function(callback) {
    var arbitraryMessage = {
      cloudProvider: '',
      ip: '127.0.0.1',
      hostname: 'hostname',
      port: 1000,
      ssl: true
    };
    var expectedResult = {
      id: 1,
      publicIP: arbitraryMessage.ip,
      hostname: arbitraryMessage.hostname,
      port: arbitraryMessage.port,
      ssl: arbitraryMessage.ssl
    };
    cloudHandler.addNewErizoController(arbitraryMessage, callback);
    expect(callback.withArgs(expectedResult).callCount).to.equal(1);
  };

  it('should succeed creating new private erizo controller', function() {
    var callback = sinon.stub();
    createPrivateErizoController(callback);
    expect(callback.callCount).to.equal(1);
  });

  it.only('should fail creating new amazon erizo controller if aws-sdk fails', function() {
    var arbitraryMessage = {
      cloudProvider: 'amazon',
      ip: '127.0.0.1',
      hostname: 'hostname',
      port: 1000,
      ssl: true
    };
    var callback = sinon.stub();
    ec2MetadataServiceMock.request.callsArgWith(1, 'error');
    cloudHandler.addNewErizoController(arbitraryMessage, callback);
    expect(callback.withArgs('error').callCount).to.equal(1);
  });

  it.only('should succeed creating new amazon erizo controller if aws-sdk success', function() {
    var arbitraryMessage = {
      cloudProvider: 'amazon',
      ip: '127.0.0.1',
      hostname: 'hostname',
      port: 1000,
      ssl: true
    };
    var callback = sinon.stub();
    ec2MetadataServiceMock.request.callsArgWith(1, undefined, '127.0.0.2');
    cloudHandler.addNewErizoController(arbitraryMessage, callback);
    var expectedResult = {
      id: 1,
      publicIP: '127.0.0.2',
      hostname: arbitraryMessage.hostname,
      port: arbitraryMessage.port,
      ssl: arbitraryMessage.ssl

    };
    expect(callback.withArgs(expectedResult).callCount).to.equal(1);
  });

  it('should return "whoareyou" when receiving keepAlives ' +
                    'from unknown erizo controller', function() {
    var callback = sinon.stub();
    cloudHandler.keepAlive('1', callback);
    expect(callback.withArgs('whoareyou').callCount).to.equal(1);
  });

  it('should return "ok" when receiving keepAlives ' +
                    'from known erizo controller', function() {
    var callback = sinon.stub();
    createPrivateErizoController(sinon.stub());
    cloudHandler.keepAlive('1', callback);
    expect(callback.withArgs('ok').callCount).to.equal(1);
  });
});
