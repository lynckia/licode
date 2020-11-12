/* global exports, require */


const serviceRegistry = require('./../mdb/serviceRegistry');
const cloudHandler = require('../cloudHandler');

const logger = require('./../logger').logger;

// Logger
const log = logger.getLogger('UserResource');

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
 * Get User. Represent a determined user of a room.
 * This is consulted to erizoController using RabbitMQ RPC call.
 */
exports.getUser = (req, res) => {
  doInit(req, (currentService, currentRoom) => {
    if (currentService === undefined) {
      res.status(404).send('Service not found');
      return;
    } else if (currentRoom === undefined) {
      log.info(`message: getUser - room not found, roomId: ${req.params.room}`);
      res.status(404).send('Room does not exist');
      return;
    }

    const user = req.params.user;

    cloudHandler.getUsersInRoom(currentRoom._id, (users) => {
      if (users === 'error') {
        res.status(503).send('CloudHandler does not respond');
        return;
      }
      let userFound = false;
      users.forEach((userInList) => {
        if (userInList.name === user) {
          userFound = true;
          log.info(`message: getUser success, ${logger.objectToLog(user)}`);
          res.send(userInList);
        }
      });
      if (!userFound) {
        log.error(`message: getUser user not found, userId: ${req.params.user}`);
        res.status(404).send('User does not exist');
      }
    });
  });
};

/*
 * Delete User. Removes a determined user from a room.
 * This order is sent to erizoController using RabbitMQ RPC call.
 */
exports.deleteUser = (req, res) => {
  doInit(req, (currentService, currentRoom) => {
    if (currentService === undefined) {
      res.status(404).send('Service not found');
      return;
    } else if (currentRoom === undefined) {
      log.info(`message: deleteUser - room not found, roomId: ${req.params.room}`);
      res.status(404).send('Room does not exist');
      return;
    }

    const user = req.params.user;

    cloudHandler.deleteUser(user, currentRoom._id, (result) => {
      if (result !== 'Success') {
        res.status(404).send(result);
      } else {
        res.send(result);
      }
    });
    // Consultar RabbitMQ
  });
};
