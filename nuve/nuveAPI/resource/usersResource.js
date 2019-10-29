/* global exports, require */


const serviceRegistry = require('./../mdb/serviceRegistry');
const cloudHandler = require('../cloudHandler');

const logger = require('./../logger').logger;

// Logger
const log = logger.getLogger('UsersResource');

/*
 * Gets the service and the room for the proccess of the request.
 */
const doInit = (req, callback) => {
  const currentService = req.service;

  serviceRegistry.getRoomForService(req.params.room, currentService, (room) => {
    callback(currentService, room);
  });
};

/*
 * Get Users. Represent a list of users of a determined room. This is consulted to cloudHandler.
 */
exports.getList = (req, res) => {
  doInit(req, (currentService, currentRoom) => {
    if (currentService === undefined) {
      res.status(404).send('Service not found');
      return;
    } else if (currentRoom === undefined) {
      log.info(`message: getUserList - room not found, roomId: ${req.params.room}`);
      res.status(404).send('Room does not exist');
      return;
    }

    log.info(`message: getUsersList success, roomId: ${currentRoom._id}, ` +
      `serviceId: ${currentService._id}`);
    cloudHandler.getUsersInRoom(currentRoom._id, (users) => {
      if (users === 'timeout') {
        res.status(503).send('Erizo Controller managing this room does not respond');
        return;
      }
      res.send(users);
    });
  });
};
