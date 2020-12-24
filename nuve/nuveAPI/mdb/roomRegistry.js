/* global require, exports */


const dataBase = require('./dataBase');

const logger = require('./../logger').logger;

// Logger
const log = logger.getLogger('RoomRegistry');

exports.getRooms = (callback) => {
  dataBase.db.collection('rooms').find({}).toArray((err, rooms) => {
    if (err || !rooms) {
      log.info('message: rooms list empty');
    } else {
      callback(rooms);
    }
  });
};

const getRoom = (id, callback) => {
  dataBase.db.collection('rooms').findOne({ _id: dataBase.ObjectId(id) }, (err, room) => {
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
  dataBase.db.collection('rooms').insertOne(room, (error, saved) => {
    if (error) log.warn(`message: addRoom error, ${logger.objectToLog(error)}`);
    callback(saved.ops[0]);
  });
};

/* eslint-disable */
exports.assignErizoControllerToRoom = function(inputRoom, erizoControllerId, callback) {
  const session = dataBase.client.startSession();
  let erizoController;

  // Step 2: Optional. Define options to use for the transaction
  const transactionOptions = {
    readPreference: 'primary',
    readConcern: { level: 'local' },
    writeConcern: { w: 'majority' }
  };

  // Step 3: Use withTransaction to start a transaction, execute the callback, and commit (or abort on error)
  // Note: The callback for withTransaction MUST be async and/or return a Promise.
  try {
    session.withTransaction(async () => {
      const room = await dataBase.db.collection('rooms').findOne({_id: dataBase.ObjectId(inputRoom._id)});
      if (!room) {
        return;
      }

      if (room.erizoControllerId) {
        erizoController = await dataBase.db.collection('erizoControllers').findOne({_id: room.erizoControllerId});
        if (erizoController) {
          return;
        }
      }

      erizoController = await dataBase.db.collection('erizoControllers').findOne({_id: dataBase.ObjectId(erizoControllerId)});

      if (erizoController) {
        room.erizoControllerId = dataBase.ObjectId(erizoControllerId);

        await dataBase.db.collection('rooms').updateOne( { _id: room._id }, { $set: room } );
      }
    }, transactionOptions).then(() => {
      callback(erizoController);
    }).catch((err) => {
      log.error('message: assignErizoControllerToRoom error, ' + logger.objectToLog(err));
    });
  } finally {
    session.endSession();
  }
};

/* eslint-enable */

/*
 * Updates a determined room
 */
exports.updateRoom = (id, room, callback) => {
  dataBase.db.collection('rooms').replaceOne({ _id: dataBase.ObjectId(id) }, room, (error) => {
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
      dataBase.db.collection('rooms').deleteOne({ _id: dataBase.ObjectId(id) }, (error) => {
        if (error) {
          log.warn(`message: removeRoom error, ${logger.objectToLog(error)}`);
        }
      });
    }
  });
};
