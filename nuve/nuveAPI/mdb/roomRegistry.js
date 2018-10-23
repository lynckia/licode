/* global require, exports, ObjectId */


const db = require('./dataBase').db;

const logger = require('./../logger').logger;

// Logger
const log = logger.getLogger('RoomRegistry');

exports.getRooms = (callback) => {
  db.rooms.find({}).toArray((err, rooms) => {
    if (err || !rooms) {
      log.info('message: rooms list empty');
    } else {
      callback(rooms);
    }
  });
};

const getRoom = (id, callback) => {
  db.rooms.findOne({ _id: db.ObjectId(id) }, (err, room) => {
    if (room === undefined) {
      log.warn(`message: getRoom - Room not found, roomId: ${id}`);
    }
    if (callback !== undefined) {
      callback(room);
    }
  });
};

exports.getRoom = getRoom;

const hasRoom = (id, callback) => {
  getRoom(id, (room) => {
    if (room === undefined) {
      callback(false);
    } else {
      callback(true);
    }
  });
};

exports.hasRoom = hasRoom;

/*
 * Adds a new room to the data base.
 */
exports.addRoom = (room, callback) => {
  db.rooms.save(room, (error, saved) => {
    if (error) log.warn(`message: addRoom error, ${logger.objectToLog(error)}`);
    callback(saved);
  });
};

exports.assignErizoControllerToRoom = (room, erizoControllerId, callback) =>
  // eslint-disable-next-line
  db.eval(function (id, erizoControllerIdToFind) {
    let erizoController;
    const roomToUpdate = db.rooms.findOne({ _id: new ObjectId(id) });
    if (!roomToUpdate) {
      return erizoController;
    }

    if (roomToUpdate.erizoControllerId) {
      erizoController = db.erizoControllers.findOne({ _id: roomToUpdate.erizoControllerId });
      if (erizoController) {
        return erizoController;
      }
    }

    erizoController = db.erizoControllers.findOne({ _id: new ObjectId(erizoControllerIdToFind) });

    if (erizoController) {
      roomToUpdate.erizoControllerId = new ObjectId(erizoControllerIdToFind);

      db.rooms.save(roomToUpdate);
    }
    return erizoController;
  }, `${room._id}`, `${erizoControllerId}`, (error, erizoController) => {
    if (error) {
      log.warn('message: assignErizoControllerToRoom error:', logger.objectToLog(error));
    }
    if (callback) {
      callback(erizoController);
    }
  });

/*
 * Updates a determined room
 */
exports.updateRoom = (id, room, callback) => {
  db.rooms.update({ _id: db.ObjectId(id) }, room, (error) => {
    if (error) log.warn(`message: updateRoom error, ${logger.objectToLog(error)}`);
    if (callback) callback(error);
  });
};

/*
 * Removes a determined room from the data base.
 */
exports.removeRoom = (id) => {
  hasRoom(id, (hasR) => {
    if (hasR) {
      db.rooms.remove({ _id: db.ObjectId(id) }, (error) => {
        if (error) {
          log.warn(`message: removeRoom error, ${logger.objectToLog(error)}`);
        }
      });
    }
  });
};
