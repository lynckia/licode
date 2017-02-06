/*global exports, require*/
'use strict';
var serviceRegistry = require('./../mdb/serviceRegistry');
var logger = require('./../logger').logger;

// Logger
var log = logger.getLogger('ServicesResource');

/*
 * Gets the service and checks if it is superservice.
 * Only superservice can do actions about services.
 */
var doInit = function (req) {
    var currentService = req.service;
    var superService = require('./../mdb/dataBase').superService;
    currentService._id = currentService._id + '';
    return (currentService._id === superService);
};

/*
 * Post Service. Creates a new service.
 */
exports.create = function (req, res) {
    if (!doInit(req)) {
        log.info('message: createService - unauthorized, serviceId: ' + req.service._id);
        res.status(401).send('Service not authorized for this action');
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
    if (!doInit(req)) {
        log.info('message: representService - not authorised, serviceId: ' + req.service._id);
        res.status(401).send('Service not authorized for this action');
        return;
    }

    serviceRegistry.getList(function (list) {
        log.info('message: representServices');
        res.send(list);
    });
};
