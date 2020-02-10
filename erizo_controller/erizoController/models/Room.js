
const events = require('events');
const controller = require('../roomController');
const Client = require('./Client').Client;
const logger = require('./../../common/logger').logger;
const StreamManager = require('../StreamManager').StreamManager;

const log = logger.getLogger('ErizoController - Room');

class Room extends events.EventEmitter {
  constructor(erizoControllerId, amqper, ecch, id, p2p) {
    super();
    this.streamManager = new StreamManager();
    this.clients = new Map();
    this.id = id;
    this.erizoControllerId = erizoControllerId;
    this.p2p = p2p;
    this.amqper = amqper;
    this.ecch = ecch;
    if (!p2p) {
      this.setupRoomController();
    }
  }

  hasClientWithId(id) {
    return this.clients.has(id);
  }

  getClientById(id) {
    return this.clients.get(id);
  }

  createClient(channel, token, options) {
    const client = new Client(channel, token, options, this);
    client.on('disconnect', this.onClientDisconnected.bind(this, client));
    this.clients.set(client.id, client);
    return client;
  }

  forEachClient(doSomething) {
    this.clients.forEach((client) => {
      doSomething(client);
    });
  }

  removeClient(id) {
    if (this.controller) {
      this.controller.removeClient(id);
    }
    return this.clients.delete(id);
  }

  onClientDisconnected() {
    if (this.clients.size === 0) {
      log.debug(`message: deleting empty room, roomId: ${this.id}`);
      this.emit('room-empty');
    }
  }

  setupRoomController() {
    this.controller = controller.RoomController({
      amqper: this.amqper,
      ecch: this.ecch,
      erizoControllerId: this.erizoControllerId,
      streamManager: this.streamManager,
    });
    this.controller.addEventListener(this.onRoomControllerEvent.bind(this));
  }

  onRoomControllerEvent(type, evt) {
    if (type === 'unpublish') {
      // It's supposed to be an integer.
      const streamId = parseInt(evt, 10);
      log.warn('message: Triggering removal of stream ' +
                 'because of ErizoJS timeout, ' +
                 `streamId: ${streamId}`);
      this.sendMessage('onRemoveStream', { id: streamId });
      // remove clients and streams?
    }
  }

  sendConnectionMessageToClient(clientId, connectionId, info, evt) {
    const client = this.getClientById(clientId);
    if (client) {
      client.sendMessage('connection_message_erizo', { connectionId, info, evt });
    }
  }

  sendMessage(method, args) {
    this.forEachClient((client) => {
      log.debug('message: sendMsgToRoom,',
        'clientId:', client.id, ',',
        'roomId:', this.id, ', ',
        logger.objectToLog(method));
      client.sendMessage(method, args);
    });
  }
}

class Rooms extends events.EventEmitter {
  constructor(amqper, ecch) {
    super();
    this.amqper = amqper;
    this.ecch = ecch;
    this.rooms = new Map();
  }

  size() {
    return this.rooms.size;
  }

  getOrCreateRoom(erizoControllerId, id, p2p) {
    let room = this.rooms.get(id);
    if (room === undefined) {
      room = new Room(erizoControllerId, this.amqper, this.ecch, id, p2p);
      this.rooms.set(room.id, room);
      room.on('room-empty', this.deleteRoom.bind(this, id));
      this.emit('updated');
    }
    return room;
  }

  forEachRoom(doSomething) {
    this.rooms.forEach((room) => {
      doSomething(room);
    });
  }

  getRoomWithClientId(id) {
    // eslint-disable-next-line no-restricted-syntax
    for (const room of this.rooms.values()) {
      if (room.hasClientWithId(id)) {
        return room;
      }
    }
    return undefined;
  }

  getRoomById(id) {
    return this.rooms.get(id);
  }

  deleteRoom(id) {
    if (this.rooms.delete(id)) {
      this.emit('updated');
    }
  }
}

exports.Room = Room;
exports.Rooms = Rooms;
