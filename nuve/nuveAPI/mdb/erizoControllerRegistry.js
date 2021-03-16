/* global require, exports */

const dataBase = require('./dataBase');

const logger = require('./../logger').logger;

// Logger
const log = logger.getLogger('ErizoControllerRegistry');

exports.getErizoControllers = (callback) => {
  dataBase.db.collection('erizoControllers').find({}).toArray((err, erizoControllers) => {
    if (err || !erizoControllers) {
      log.info('message: service getList empty');
    } else {
      callback(erizoControllers);
    }
  });
};

const getErizoController = (id, callback) => {
  dataBase.db.collection('erizoControllers').findOne({ _id: dataBase.ObjectId(id) }, (err, erizoController) => {
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
  dataBase.db.collection('erizoControllers').insertOne(erizoController, (error, saved) => {
    if (error) log.warn(`message: addErizoController error, ${logger.objectToLog(error)}`);
    callback(saved.ops[0]);
  });
};


/*
 * Updates a determined ErizoController
 */
exports.updateErizoController = (id, erizoController) => {
  dataBase.db.collection('erizoControllers').updateOne({ _id: dataBase.ObjectId(id) }, { $set: erizoController }, (error) => {
    if (error) log.warn(`message: updateErizoController error, ${logger.objectToLog(error)}`);
  });
};

exports.incrementKeepAlive = (id) => {
  dataBase.db.collection('erizoControllers').updateOne({ _id: dataBase.ObjectId(id) }, { $inc: { keepAlive: 1 } }, (error) => {
    if (error) log.warn(`message: updateErizoController error, ${logger.objectToLog(error)}`);
  });
};

/*
 * Removes a determined ErizoController from the data base.
 */
exports.removeErizoController = (id) => {
  hasErizoController(id, (hasEC) => {
    if (hasEC) {
      dataBase.db.collection('erizoControllers').deleteOne({ _id: dataBase.ObjectId(id) }, (error) => {
        if (error) {
          log.warn('message: removeErizoController error, ', logger.objectToLog(error));
        }
      });
    }
  });
};
