/*global exports, require, console*/
var serviceRegistry = require('./../mdb/serviceRegistry');
var logger = require('./../logger').logger;

// Logger
var log = logger.getLogger("ServiceResource");

/*
 * Gets the service and checks if it is superservice. Only superservice can do actions about services.
 */
var doInit = function (serv, callback) {
    "use strict";

    var service = require('./../auth/nuveAuthenticator').service,
        superService = require('./../mdb/dataBase').superService;

    service._id = service._id + '';    
    if (service._id !== superService) {
        callback('error');
    } else {
        serviceRegistry.getService(serv, function (ser) {
            callback(ser);
        });
    }
};

/*
 * Get Service. Represents a determined service.
 */
exports.represent = function (req, res) {
    "use strict";

    doInit(req.params.service, function (serv) {
        if (serv === 'error') {
            log.info('Service ', req.params.service, ' not authorized for this action');
            res.send('Service not authorized for this action', 401);
            return;
        }
        if (serv === undefined) {
            res.send('Service not found', 404);
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
    "use strict";

    doInit(req.params.service, function (serv) {
        if (serv === 'error') {
            log.info('Service ', req.params.service, ' not authorized for this action');
            res.send('Service not authorized for this action', 401);
            return;
        }
        if (serv === undefined) {
            res.send('Service not found', 404);
            return;
        }
        var id = '';
        id += serv._id;
        serviceRegistry.removeService(id);
        log.info('Serveice ', id, ' deleted');
        res.send('Service deleted');
    });
};