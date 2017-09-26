'use strict';
const events = require('events');
const controller = require('../roomController');
const Client = require('./Client').Client;
const logger = require('./../../common/logger').logger;
const log = logger.getLogger('ErizoController - Room');

class Room extends events.EventEmitter {
  constructor(amqper, ecch, id, p2p) {
    super();
    this.streams = new Map();
    this.clients = new Map();
    this.id = id;
    this.p2p = p2p;
    this.amqper = amqper;
    this.ecch = ecch;
    if (!p2p) {
      this.setupRoomController();
    }
  }

  getStreamById(id) {
    return this.streams.get(id);
  }

  forEachStream(doSomething) {
    for (let stream of this.streams.values()) {
      doSomething(stream);
    }
  }

  removeStream(id) {
    return this.streams.delete(id);
  }

  getClientById(id) {
    return this.clients.get(id);
  }

  createClient(channel, token) {
    let client = new Client(channel, token, this);
    client.on('disconnect', this.onClientDisconnected.bind(this, client));
    this.clients.set(client.id, client);
    return client;
  }

  forEachClient(doSomething) {
    for (let client of this.clients.values()) {
      doSomething(client);
    }
  }

  removeClient(id) {
    return this.clients.delete(id);
  }

  onClientDisconnected() {
    if (this.clients.size === 0) {
      log.debug('message: deleting empty room, roomId: ' + this.id);
      this.emit('room-empty');
    }
  }

  setupRoomController() {
    this.controller = controller.RoomController({amqper: this.amqper, ecch: this.ecch});
    this.controller.addEventListener(this.onRoomControllerEvent.bind(this));
  }

  onRoomControllerEvent(type, evt ) {
    if (type === 'unpublish') {
        // It's supposed to be an integer.
        var streamId = parseInt(evt);
        log.warn('message: Triggering removal of stream ' +
                 'because of ErizoJS timeout, ' +
                 'streamId: ' + streamId);
        this.sendMessage('onRemoveStream', {id: streamId});
        // remove clients and streams?
    }
  }

  sendMessage(method, args) {
    var clients = this.clients;
    for (let client of clients.values()) {
      log.debug('message: sendMsgToRoom,',
                'clientId:', client.id, ',',
                'roomId:', this.id, ', ',
                logger.objectToLog(method));
      client.sendMessage(method, args);
    }
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

  getOrCreateRoom(id, p2p) {
    let room = this.rooms.get(id);
    if (room === undefined) {
      room = new Room(this.amqper, this.ecch, id, p2p);
      this.rooms.set(room.id, room);
      room.on('room-empty', this.deleteRoom.bind(this, id));
      this.emit('updated');
    }
    return room;
  }

  forEachRoom(doSomething) {
    for (const room of this.rooms.values()) {
      doSomething(room);
    }
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
