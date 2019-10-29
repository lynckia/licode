/* global require, exports */

const db = require('./dataBase').db;
const logger = require('./../logger').logger;

// Logger
const log = logger.getLogger('ServiceRegistry');

/*
 * Gets a list of the services in the data base.
 */
exports.getList = (callback) => {
  db.services.find({}).toArray((err, services) => {
    if (err || !services) {
      log.info('message: service getList empty');
    } else {
      callback(services);
    }
  });
};

const getService = (id, callback) => {
  db.services.findOne({ _id: db.ObjectId(id) }, (err, service) => {
    if (service === undefined) {
      log.info(`message: getService service not found, serviceId ${id}`);
    }
    if (callback !== undefined) {
      callback(service);
    }
  });
};

exports.getService = getService;

const hasService = (id, callback) => {
  getService(id, (service) => {
    if (service === undefined) {
      callback(false);
    } else {
      callback(true);
    }
  });
};

exports.hasService = hasService;

/*
 * Adds a new service to the data base.
 */
exports.addService = (service, callback) => {
  // eslint-disable-next-line no-param-reassign
  service.rooms = [];
  db.services.save(service, (error, saved) => {
    if (error) log.info(`message: addService error, ${logger.objectToLog(error)}`);
    callback(saved._id);
  });
};

/*
 * Updates a service in the data base.
 */
exports.updateService = (service, callback) => {
  db.services.save(service, (error) => {
    if (error) log.info(`message: updateService error, ${logger.objectToLog(error)}`);
    if (callback) callback();
  });
};

/*
 * Updates a service in the data base with a new room.
 */
exports.addRoomToService = (service, room, callback) => {
  db.services.update({ _id: db.ObjectId(service._id) }, { $addToSet: { rooms: room } },
    (error) => {
      if (error) log.info(`message: updateService error, ${logger.objectToLog(error)}`);
      if (callback) callback();
    });
};

/*
 * Removes a service from the data base.
 */
exports.removeService = (id) => {
  hasService(id, (hasS) => {
    if (hasS) {
      db.services.remove({ _id: db.ObjectId(id) }, (error) => {
        if (error) log.info(`message: removeService error, ${logger.objectToLog(error)}`);
      });
    }
  });
};

/*
 * Gets a room in a service. Returns undefined if room does not exists.
 */
exports.getRoomForService = (roomId, service, callback) => {
  let requestedRoom;
  Object.keys(service.rooms).forEach((roomKey) => {
    if (String(service.rooms[roomKey]._id) === String(roomId)) {
      requestedRoom = service.rooms[roomKey];
    }
  });
  callback(requestedRoom);
};
