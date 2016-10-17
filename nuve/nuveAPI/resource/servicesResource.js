/*global exports, require*/
'use strict';
var serviceRegistry = require('./../mdb/serviceRegistry');
var logger = require('./../logger').logger;

// Logger
var log = logger.getLogger('ServicesResource');

var currentService;

/*
 * Gets the service and checks if it is superservice.
 * Only superservice can do actions about services.
 */
var doInit = function () {
    currentService = require('./../auth/nuveAuthenticator').service;
    var superService = require('./../mdb/dataBase').superService;
    currentService._id = currentService._id + '';
    return (currentService._id === superService);
};

/*
 * Post Service. Creates a new service.
 */
exports.create = function (req, res) {
    if (!doInit()) {
        log.info('message: createService - unauthorized, serviceId: ' + currentService._id);
        res.send('Service not authorized for this action', 401);
        return;
    }

    serviceRegistry.addService(req.body, function (result) {
        log.info('message: createService success, serviceName: ' + req.body.name);
        res.send(result);
    });
};

/*
 * Get Service. Represents a determined service.
 */
exports.represent = function (req, res) {
    if (!doInit()) {
        log.info('message: representService - not authorised, serviceId: ' + currentService._id);
        res.send('Service not authorized for this action', 401);
        return;
    }

    serviceRegistry.getList(function (list) {
        log.info('message: representServices');
        res.send(list);
    });
};
