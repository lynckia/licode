/* global require, exports */


const db = require('./dataBase').db;

const logger = require('./../logger').logger;

// Logger
const log = logger.getLogger('ErizoControllerRegistry');

exports.getErizoControllers = (callback) => {
  db.erizoControllers.find({}).toArray((err, erizoControllers) => {
    if (err || !erizoControllers) {
      log.info('message: service getList empty');
    } else {
      callback(erizoControllers);
    }
  });
};

const getErizoController = (id, callback) => {
  db.erizoControllers.findOne({ _id: db.ObjectId(id) }, (err, erizoController) => {
    if (erizoController === undefined) {
      log.warn(`message: getErizoController - ErizoController not found, Id: ${id}`);
    }
    if (callback !== undefined) {
      callback(erizoController);
    }
  });
};
exports.getErizoController = getErizoController;

const hasErizoController = (id, callback) => {
  getErizoController(id, (erizoController) => {
    if (erizoController === undefined) {
      callback(false);
    } else {
      callback(true);
    }
  });
};

exports.hasErizoController = hasErizoController;

/*
 * Adds a new ErizoController to the data base.
 */
exports.addErizoController = (erizoController, callback) => {
  db.erizoControllers.save(erizoController, (error, saved) => {
    if (error) log.warn(`message: addErizoController error, ${logger.objectToLog(error)}`);
    callback(saved);
  });
};


/*
 * Updates a determined ErizoController
 */
exports.updateErizoController = (id, erizoController) => {
  db.erizoControllers.update({ _id: db.ObjectId(id) }, { $set: erizoController }, (error) => {
    if (error) log.warn(`message: updateErizoController error, ${logger.objectToLog(error)}`);
  });
};

exports.incrementKeepAlive = (id) => {
  db.erizoControllers.update({ _id: db.ObjectId(id) }, { $inc: { keepAlive: 1 } }, (error) => {
    if (error) log.warn(`message: updateErizoController error, ${logger.objectToLog(error)}`);
  });
};

/*
 * Removes a determined ErizoController from the data base.
 */
exports.removeErizoController = (id) => {
  hasErizoController(id, (hasEC) => {
    if (hasEC) {
      db.erizoControllers.remove({ _id: db.ObjectId(id) }, (error) => {
        if (error) {
          log.warn('message: removeErizoController error, ',
            `${logger.objectToLog(error)}`);
        }
      });
    }
  });
};
