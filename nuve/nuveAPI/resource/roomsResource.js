/* global exports, require */


const roomRegistry = require('./../mdb/roomRegistry');
const serviceRegistry = require('./../mdb/serviceRegistry');

const logger = require('./../logger').logger;

// Logger
const log = logger.getLogger('RoomsResource');

/*
 * Post Room. Creates a new room for a determined service.
 */
exports.createRoom = (req, res) => {
  let room;

  const currentService = req.service;

  if (currentService === undefined) {
    res.status(404).send('Service not found');
    return;
  }
  if (req.body.name === undefined) {
    log.info('message: createRoom - invalid room name');
    res.status(400).send('Invalid room');
    return;
  }

  req.body.options = req.body.options || {};

  if (req.body.options.test) {
    if (currentService.testRoom !== undefined) {
      log.info(`message: testRoom already exists, serviceId: ${currentService.name}`);
      res.send(currentService.testRoom);
    } else {
      room = { name: 'testRoom' };
      roomRegistry.addRoom(room, (result) => {
        currentService.testRoom = result;
        currentService.rooms.push(result);
        serviceRegistry.addRoomToService(currentService, result);
        log.info(`message: testRoom created, serviceId: ${currentService.name}`);
        res.send(result);
      });
    }
  } else {
    room = { name: req.body.name };

    if (req.body.options.p2p) {
      room.p2p = true;
    }
    if (req.body.options.data) {
      room.data = req.body.options.data;
    }
    if (typeof req.body.options.mediaConfiguration === 'string') {
      room.mediaConfiguration = req.body.options.mediaConfiguration;
    }
    roomRegistry.addRoom(room, (result) => {
      currentService.rooms.push(result);
      serviceRegistry.addRoomToService(currentService, result);
      log.info(`message: createRoom success, roomName:${req.body.name}, ` +
        `serviceId: ${currentService.name}, p2p: ${room.p2p}`);
      res.send(result);
    });
  }
};

/*
 * Get Rooms. Represent a list of rooms for a determined service.
 */
exports.represent = (req, res) => {
  const currentService = req.service;
  if (currentService === undefined) {
    res.status(404).send('Service not found');
    return;
  }
  log.info(`message: representRooms, serviceId: ${currentService._id}`);

  res.send(currentService.rooms);
};
