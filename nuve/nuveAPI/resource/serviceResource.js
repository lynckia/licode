/*global exports, require*/
'use strict';
var serviceRegistry = require('./../mdb/serviceRegistry');
var logger = require('./../logger').logger;

// Logger
var log = logger.getLogger('ServiceResource');

/*
 * Gets the service and checks if it is superservice.
 * Only superservice can do actions about services.
 */
var doInit = function (req, callback) {
    var service = req.service,
        superService = require('./../mdb/dataBase').superService;

    service._id = service._id + '';
    if (service._id !== superService) {
        callback('error');
    } else {
        serviceRegistry.getService(req.params.service, function (ser) {
            callback(ser);
        });
    }
};

/*
 * Get Service. Represents a determined service.
 */
exports.represent = function (req, res) {
    doInit(req, function (serv) {
        if (serv === 'error') {
            log.info('message: represent service - not authorized, serviceId: ' +
                req.params.service);
            res.status(401).send('Service not authorized for this action');
            return;
        }
        if (serv === undefined) {
            res.status(404).send('Service not found');
            return;
        }
        log.info('Representing service ', serv._id);
        res.send(serv);
    });
};

/*
 * Delete Service. Removes a determined service from the data base.
 */
exports.deleteService = function (req, res) {
    doInit(req, function (serv) {
        if (serv === 'error') {
            log.info('message: deleteService - not authorized, serviceId: ' + req.params.service);
            res.status(401).send('Service not authorized for this action');
            return;
        }
        if (serv === undefined) {
            res.status(404).send('Service not found');
            return;
        }
        var id = '';
        id += serv._id;
        serviceRegistry.removeService(id);
        log.info('message: deleteService success, serviceId: ' + id);
        res.send('Service deleted');
    });
};
