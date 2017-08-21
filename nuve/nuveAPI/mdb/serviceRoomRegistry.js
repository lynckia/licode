/*global require, exports, ObjectId*/

'use strict';
var db = require('./dataBase').db;
var roomRegistry = require('./roomRegistry');

var logger = require('./../logger').logger;

// Logger
var log = logger.getLogger('ServiceRoomRegistry');

function prepareServiceRoom(service, room) {
    return {
        serviceId: service._id,
        roomId: room._id,
    };
};

exports.addServiceRoom = function (service, room, callback) {
    var serviceRoom = prepareServiceRoom(service, room);
    db.servicesRooms.save(serviceRoom, function (error, saved) {
        if (error) log.warn('message: addServiceRoom error, ' + logger.objectToLog(error));
        callback(saved);
    });
};

exports.removeServiceRoom = function (service, room, callback) {
    var serviceRoom = prepareServiceRoom(service, room);
    db.servicesRooms.remove(serviceRoom, function (error, saved){
        if (error) log.warn('message: removeServiceRoom error, ' + logger.objectToLog(error));
        callback(saved);
    });
};

exports.getRoomForService = function (roomId, service, callback) {
    var query = { roomId, serviceId: service._id };
    db.services.find(query, function (error, serviceRoom) {
        if (error) {
            log.error('message: getRoomForService error, ' + logger.objectToLog(error));
            return callback(undefined);
        }

        roomRegistry.getRoom(roomId, callback);
    });
};