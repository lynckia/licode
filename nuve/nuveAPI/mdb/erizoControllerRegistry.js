/*global require, exports*/
'use strict';
var db = require('./dataBase').db;

var logger = require('./../logger').logger;

// Logger
var log = logger.getLogger('ErizoControllerRegistry');

exports.getErizoControllers = function (callback) {
    db.erizoControllers.find({}).toArray(function (err, erizoControllers) {
        if (err || !erizoControllers) {
            log.info('message: service getList empty');
        } else {
            callback(erizoControllers);
        }
    });
};

var getErizoController = exports.getErizoController = function (id, callback) {
    db.erizoControllers.findOne({_id: db.ObjectId(id)}, function (err, erizoController) {
        if (erizoController === undefined) {
            log.warn('message: getErizoController - ErizoController not found, Id: ' + id);
        }
        if (callback !== undefined) {
            callback(erizoController);
        }
    });
};

var hasErizoController = exports.hasErizoController = function (id, callback) {
    getErizoController(id, function (erizoController) {
        if (erizoController === undefined) {
            callback(false);
        } else {
            callback(true);
        }
    });
};

/*
 * Adds a new ErizoController to the data base.
 */
exports.addErizoController = function (erizoController, callback) {
    db.erizoControllers.save(erizoController, function (error, saved) {
        if (error) log.warn('message: addErizoController error, ' + logger.objectToLog(error));
        callback(saved);
    });
};


/*
 * Updates a determined ErizoController
 */
exports.updateErizoController = function (id, erizoController) {
    db.erizoControllers.update({_id: db.ObjectId(id)}, erizoController, function (error) {
        if (error) log.warn('message: updateErizoController error, ' + logger.objectToLog(error));
    });
};

/*
 * Removes a determined ErizoController from the data base.
 */
exports.removeErizoController = function (id) {
    hasErizoController(id, function (hasEC) {
        if (hasEC) {
            db.erizoControllers.remove({_id: db.ObjectId(id)}, function (error) {
                if (error) log.warn('message: removeErizoController error, ' +
                   logger.objectToLog(error));
            });
        }
    });
};
