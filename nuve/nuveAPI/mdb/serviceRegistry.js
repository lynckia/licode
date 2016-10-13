/*global require, exports*/
'use strict';
var db = require('./dataBase').db;

var logger = require('./../logger').logger;

// Logger
var log = logger.getLogger('ServiceRegistry');

/*
 * Gets a list of the services in the data base.
 */
exports.getList = function (callback) {
    db.services.find({}).toArray(function (err, services) {
        if (err || !services) {
            log.info('Empty list');
        } else {
            callback(services);
        }
    });
};

var getService = exports.getService = function (id, callback) {
    db.services.findOne({_id: db.ObjectId(id)}, function (err, service) {
        if (service === undefined) {
            log.info('Service not found');
        }
        if (callback !== undefined) {
            callback(service);
        }
    });
};

var hasService = exports.hasService = function (id, callback) {
    getService(id, function (service) {
        if (service === undefined) {
            callback(false);
        } else {
            callback(true);
        }
    });
};

/*
 * Adds a new service to the data base.
 */
exports.addService = function (service, callback) {
    service.rooms = [];
    db.services.save(service, function (error, saved) {
        if (error) log.info('MongoDB: Error adding service: ', error);
        callback(saved._id);
    });
};

/*
 * Updates a determined service in the data base.
 */
exports.updateService = function (service) {
    db.services.save(service, function (error) {
        if (error) log.info('MongoDB: Error updating service: ', error);
    });
};

/*
 * Removes a determined service from the data base.
 */
exports.removeService = function (id) {
    hasService(id, function (hasS) {
        if (hasS) {
            db.services.remove({_id: db.ObjectId(id)}, function (error) {
                if (error) log.info('MongoDB: Error removing service: ', error);
            });
        }
    });
};

/*
 * Gets a determined room in a determined service. Returns undefined if room does not exists.
 */
exports.getRoomForService = function (roomId, service, callback) {
    var room;
    for (room in service.rooms) {
        if (service.rooms.hasOwnProperty(room)) {
            if (String(service.rooms[room]._id) === String(roomId)) {
                callback(service.rooms[room]);
                return;
            }
        }
    }
    callback(undefined);
};
