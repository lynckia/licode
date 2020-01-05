/* global exports, require */


const roomRegistry = require('./../mdb/roomRegistry');
const serviceRegistry = require('./../mdb/serviceRegistry');
const cloudHandler = require('../cloudHandler');

const logger = require('./../logger').logger;

// Logger
const log = logger.getLogger('RoomResource');

/*
 * Gets the service and the room for the proccess of the request.
 */
const doInit = (req, callback) => {
  serviceRegistry.getRoomForService(req.params.room, req.service, (room) => {
    callback(room);
  });
};

/*
 * Get Room. Represents a determined room.
 */
exports.represent = (req, res) => {
  doInit(req, (currentRoom) => {
    if (req.service === undefined) {
      res.status(401).send('Client unathorized');
    } else if (currentRoom === undefined) {
      log.info(`message: representRoom - room does not exits, roomId: ${req.params.room}`);
      res.status(404).send('Room does not exist');
    } else {
      log.info(`message: representRoom success, roomId: ${currentRoom._id
      }, serviceId: ${req.service._id}`);
      res.send(currentRoom);
    }
  });
};

/*
 * Update Room.
 */
exports.updateRoom = (req, res) => {
  doInit(req, (currentRoom) => {
    if (req.service === undefined) {
      res.status(401).send('Client unathorized');
    } else if (currentRoom === undefined) {
      log.info(`message: updateRoom - room does not exist + roomId: ${req.params.room}`);
      res.status(404).send('Room does not exist');
    } else if (req.body.name === undefined) {
      log.info('message: updateRoom - Invalid room');
      res.status(400).send('Invalid room');
    } else {
      let id = '';
      const array = req.service.rooms;
      let index = -1;
      id += currentRoom._id;
      const room = currentRoom;

      room.name = req.body.name;
      const options = req.body.options || {};


      if (options.p2p) {
        room.p2p = true;
      }
      if (options.data) {
        room.data = options.data;
      }
      if (typeof options.mediaConfiguration === 'string') {
        room.mediaConfiguration = options.mediaConfiguration;
      }

      roomRegistry.updateRoom(id, room);

      for (let i = 0; i < array.length; i += 1) {
        if (array[i]._id === currentRoom._id) {
          index = i;
        }
      }
      if (index !== -1) {
        req.service.rooms[index] = room;
        serviceRegistry.updateService(req.service);
        log.info(`message: updateRoom  successful, roomId: ${id}, ` +
          `serviceId: ${req.service._id}`);
        res.send('Room Updated');
      }
    }
  });
};

/*
 * Patch Room.
 */
exports.patchRoom = (req, res) => {
  doInit(req, (currentRoom) => {
    if (req.service === undefined) {
      res.status(401).send('Client unathorized');
    } else if (currentRoom === undefined) {
      log.info(`message: pachRoom - room does not exist, roomId : ${req.params.room}`);
      res.status(404).send('Room does not exist');
    } else {
      let id = '';
      const array = req.service.rooms;
      let index = -1;
      id += currentRoom._id;
      const room = currentRoom;

      if (req.body.name) room.name = req.body.name;
      if (req.body.options) {
        if (req.body.options.p2p) room.p2p = req.body.options.p2p;
        if (req.body.options.data) {
          Object.keys(req.body.options.data).forEach((dataKey) => {
            room.data[dataKey] = req.body.options.data[dataKey];
          });
        }
      }

      roomRegistry.updateRoom(id, room);

      for (let i = 0; i < array.length; i += 1) {
        if (array[i]._id === currentRoom._id) {
          index = i;
        }
      }
      if (index !== -1) {
        req.service.rooms[index] = room;
        serviceRegistry.updateService(req.service);
        log.info('message: patchRoom room successfully updated, ' +
        `roomId: ${id}, serviceId: ${req.service._id}`);

        res.send('Room Updated');
      }
    }
  });
};


/*
 * Delete Room. Removes a determined room from the data base
 * and asks cloudHandler to remove it from erizoController.
 */
exports.deleteRoom = (req, res) => {
  doInit(req, (currentRoom) => {
    if (req.service === undefined) {
      res.status(401).send('Client unathorized');
    } else if (currentRoom === undefined) {
      log.info(`message: deleteRoom - room does not exist, roomId: ${req.params.room}`);
      res.status(404).send('Room does not exist');
    } else {
      let id = '';
      const array = req.service.rooms;
      let index = -1;

      id += currentRoom._id;
      roomRegistry.removeRoom(id);

      for (let i = 0; i < array.length; i += 1) {
        if (array[i]._id === currentRoom._id) {
          index = i;
        }
      }
      if (index !== -1) {
        req.service.rooms.splice(index, 1);
        serviceRegistry.updateService(req.service);
        log.info('message: deleteRoom - room successfully deleted, ' +
        `roomId: ${id}, serviceId: ${req.service._id}`);
        cloudHandler.deleteRoom(id, () => {});
        res.send('Room deleted');
      }
    }
  });
};
