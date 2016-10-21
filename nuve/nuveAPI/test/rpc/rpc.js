/*global require, describe, it, beforeEach, afterEach*/
'use strict';
var mocks = require('../utils');
var sinon = require('sinon');
var expect  = require('chai').expect;

var kArbitraryMessage = {
  type: 'arbitraryType',
  method: 'arbitraryMethod',
  args: ['arbitraryArg'],
  replyTo: '1',
  corrID: '1'
};

describe('RPC', function() {
  var amqpMock,
      connectionMock,
      exchangeMock,
      queueMock,
      rpcPublicMock,
      rpc;
  beforeEach(function() {
    mocks.start(mocks.licodeConfig);
    amqpMock = mocks.start(mocks.amqp);
    rpcPublicMock = mocks.start(mocks.rpcPublic);
    connectionMock = mocks.amqpConnection;
    exchangeMock = mocks.amqpExchange;
    queueMock = mocks.amqpQueue;
    rpc = require('../../rpc/rpc');

    connectionMock.on.withArgs('ready').callsArg(1);
    connectionMock.exchange.callsArgWithAsync(2, exchangeMock);
    connectionMock.exchange.returns(exchangeMock);
    connectionMock.queue.returns(queueMock);
    connectionMock.queue.callsArgWithAsync(1, queueMock);
  });

  afterEach(function() {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(rpcPublicMock);
    mocks.stop(amqpMock);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  it('should connect and create queues and exchanges', function(done) {
    rpc.connect(function() {
      expect(connectionMock.on.callCount).to.equal(2);
      expect(connectionMock.exchange.callCount).to.equal(1);
      expect(connectionMock.queue.callCount).to.equal(2);
      expect(queueMock.bind.callCount).to.equal(2);
      expect(queueMock.subscribe.callCount).to.equal(2);

      done();
    });
  });

  it('should not throw an exception if callback is not passed', function(done) {
    sinon.spy(rpc.connect);

    queueMock.subscribe.onCall(1).callsArgWith(0, kArbitraryMessage);
    rpcPublicMock.arbitraryMethod = sinon.stub().callsArgWith(1, 'type', 'result');
    rpc.connect();

    setTimeout(done, 0);
  });

  it('should call public rpc when receiving messages', function(done) {
    queueMock.subscribe.onCall(1).callsArgWith(0, kArbitraryMessage);
    rpcPublicMock.arbitraryMethod = sinon.stub().callsArgWith(1, 'type', 'result');
    rpc.connect(function() {
      expect(rpcPublicMock.arbitraryMethod.callCount).to.equal(1);
      expect(exchangeMock.publish.withArgs(kArbitraryMessage.replyTo,
                                           {data: 'result',
                                            corrID: kArbitraryMessage.corrID,
                                            type: 'type'}).callCount).to.equal(1);
      done();
    });
  });

  it('should call public rpc on callRpc', function(done) {
    rpc.connect(function() {
      rpc.callRpc('to', 'method', ['args'], undefined);
      expect(exchangeMock.publish.callCount).to.equal(1);
      done();
    });
  });
});
