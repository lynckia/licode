/* global require, describe, it, beforeEach, afterEach */


const mocks = require('../utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;

const kArbitraryMessage = {
  type: 'arbitraryType',
  method: 'arbitraryMethod',
  args: ['arbitraryArg'],
  replyTo: '1',
  corrID: '1',
};

describe('RPC', () => {
  let amqpMock;
  let connectionMock;
  let exchangeMock;
  let queueMock;
  let rpcPublicMock;
  let rpc;
  beforeEach(() => {
    mocks.start(mocks.licodeConfig);
    amqpMock = mocks.start(mocks.amqp);
    rpcPublicMock = mocks.start(mocks.rpcPublic);
    connectionMock = mocks.amqpConnection;
    exchangeMock = mocks.amqpExchange;
    queueMock = mocks.amqpQueue;
    // eslint-disable-next-line global-require
    rpc = require('../../rpc/rpc');

    connectionMock.on.withArgs('ready').callsArg(1);
    connectionMock.exchange.callsArgWithAsync(2, exchangeMock);
    connectionMock.exchange.returns(exchangeMock);
    connectionMock.queue.returns(queueMock);
    connectionMock.queue.callsArgWithAsync(1, queueMock);
  });

  afterEach(() => {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(rpcPublicMock);
    mocks.stop(amqpMock);
    mocks.deleteRequireCache();
    mocks.reset();
  });

  it('should connect and create queues and exchanges', (done) => {
    rpc.connect(() => {
      expect(connectionMock.on.callCount).to.equal(2);
      expect(connectionMock.exchange.callCount).to.equal(1);
      expect(connectionMock.queue.callCount).to.equal(2);
      expect(queueMock.bind.callCount).to.equal(2);
      expect(queueMock.subscribe.callCount).to.equal(2);

      done();
    });
  });

  it('should not throw an exception if callback is not passed', (done) => {
    sinon.spy(rpc.connect);

    queueMock.subscribe.onCall(1).callsArgWith(0, kArbitraryMessage);
    rpcPublicMock.arbitraryMethod = sinon.stub().callsArgWith(1, 'type', 'result');
    rpc.connect();

    setTimeout(done, 0);
  });

  it('should call public rpc when receiving messages', (done) => {
    queueMock.subscribe.onCall(1).callsArgWith(0, kArbitraryMessage);
    rpcPublicMock.arbitraryMethod = sinon.stub().callsArgWith(1, 'type', 'result');
    rpc.connect(() => {
      expect(rpcPublicMock.arbitraryMethod.callCount).to.equal(1);
      expect(exchangeMock.publish.withArgs(kArbitraryMessage.replyTo,
        { data: 'result',
          corrID: kArbitraryMessage.corrID,
          type: 'type' }).callCount).to.equal(1);
      done();
    });
  });

  it('should call public rpc on callRpc', (done) => {
    rpc.connect(() => {
      rpc.callRpc('to', 'method', ['args'], undefined);
      expect(exchangeMock.publish.callCount).to.equal(1);
      done();
    });
  });
});
