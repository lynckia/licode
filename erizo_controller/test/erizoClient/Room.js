/* global describe, beforeEach, afterEach, context, it, expect, Erizo, sinon */

/* eslint-disable no-unused-expressions */

function promisify(func) {
  return new Promise((resolve) => {
    func((...args) => {
      resolve(...args);
    });
  });
}

describe('Room', () => {
  let room;
  let io;
  let connectionHelpers;
  let connectionManager;
  let socket;

  beforeEach(() => {
    Erizo.Logger.setLogLevel(Erizo.Logger.NONE);
    socket = {
      io: { engine: { transport: { ws: { onclose: sinon.stub() } } } },
      on: sinon.stub(),
      removeListener: sinon.stub(),
      emit: sinon.stub(),
      disconnect: sinon.stub(),
    };
    io = {
      connect: sinon.stub().returns(socket),
    };
    connectionHelpers = {
      getBrowser: sinon.stub(),
    };
    connectionManager = { ErizoConnectionManager: Erizo._.ErizoConnectionManager };
  });

  afterEach(() => {
  });


  it('should connect to ErizoController', async () => {
    const data = {
      tokenId: 'arbitraryId',
    };
    const spec = { token: Erizo._.Base64.encodeBase64(JSON.stringify(data)) };
    room = Erizo._.Room(io, connectionHelpers, connectionManager, spec);
    room.connect({});
    const promise = promisify(room.on.bind(null, 'room-connected'));
    const response = {
      id: 'arbitraryRoomId',
      clientId: 'arbitraryClientId',
      streams: [],
    };
    socket.on.withArgs('connected').callArgWith(1, response);

    const roomEvent = await promise;
    expect(roomEvent.type).to.equals('room-connected');
    expect(roomEvent.streams.length).to.equals(0);
  });

  context('Room connected', () => {
    const arbitraryStream = {
      id: 'arbitraryStreamId',
      audio: true,
      video: true,
      data: true,
      label: 'arbitraryStreamId',
      screen: false,
      attributes: [],
    };

    beforeEach(async () => {
      const data = {
        tokenId: 'arbitraryId',
      };
      const spec = { token: Erizo._.Base64.encodeBase64(JSON.stringify(data)) };
      room = Erizo._.Room(io, connectionHelpers, connectionManager, spec);
      room.connect({});
      const promise = promisify(room.on.bind(null, 'room-connected'));
      const response = {
        id: 'arbitraryRoomId',
        clientId: 'arbitraryClientId',
        streams: [],
      };
      socket.on.withArgs('connected').callArgWith(1, response);

      await promise;
    });

    it('should trigger new streams created', async () => {
      const promise = promisify(room.on.bind(null, 'stream-added'));
      socket.on.withArgs('onAddStream').callArgWith(1, arbitraryStream);
      const streamEvent = await promise;
      const stream = streamEvent.stream;
      expect(stream.getID()).to.equal(arbitraryStream.id);
    });

    it('should trigger new streams deleted', async () => {
      const promise = promisify(room.on.bind(null, 'stream-removed'));
      socket.on.withArgs('onAddStream').callArgWith(1, arbitraryStream);
      socket.on.withArgs('onRemoveStream').callArgWith(1, arbitraryStream);
      const streamEvent = await promise;
      const stream = streamEvent.stream;
      expect(stream.getID()).to.equal(arbitraryStream.id);
    });

    it('should subscribe to new streams', async () => {
      const promise = promisify(room.on.bind(null, 'stream-added'));
      socket.on.withArgs('onAddStream').callArgWith(1, arbitraryStream);
      const streamEvent = await promise;
      const stream = streamEvent.stream;

      room.subscribe(stream);

      const result = 'arbitraryResult';
      const erizoId = 'arbitraryErizoId';
      const connectionId = 'arbitraryConnectionId';
      const error = null;

      const data = socket.emit.withArgs('subscribe').args[0][1].msg;
      expect(data.options.streamId).to.equal(arbitraryStream.id);
      expect(stream.state).to.equal('subscribing');

      socket.emit.withArgs('subscribe').args[0][2](result, erizoId, connectionId, error);
      stream.dispatchEvent(Erizo.StreamEvent({ type: 'added', stream }));
      expect(stream.state).to.equal('subscribed');
    });

    it.skip('should resubscribe to new streams', async () => {
      const promise = promisify(room.on.bind(null, 'stream-added'));
      socket.on.withArgs('onAddStream').callArgWith(1, arbitraryStream);
      const streamEvent = await promise;
      const stream = streamEvent.stream;

      room.subscribe(stream);

      const result = 'arbitraryResult';
      const erizoId = 'arbitraryErizoId';
      const connectionId = 'arbitraryConnectionId';
      const error = null;

      const data = socket.emit.withArgs('subscribe').args[0][1].msg;
      expect(data.options.streamId).to.equal(arbitraryStream.id);
      expect(stream.state).to.equal('subscribing');

      socket.emit.withArgs('subscribe').args[0][2](result, erizoId, connectionId, error);

      room.unsubscribe(stream);

      expect(stream.state).to.equal('unsubscribing');
      stream.dispatchEvent(Erizo.StreamEvent({ type: 'added', stream }));
      expect(stream.state).to.equal('subscribed');
      const connection = room.erizoConnectionManager.getErizoConnection('arbitraryConnectionId');
      connection.streamRemovedListener(stream.getID());
      socket.emit.withArgs('unsubscribe').args[0][2](result, error);
      expect(stream.state).to.equal('unsubscribed');
    });
  });
});
