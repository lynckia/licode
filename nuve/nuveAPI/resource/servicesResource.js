/*global exports, require, console*/
var serviceRegistry = require('./../mdb/serviceRegistry');

var currentService;

/*
 * Gets the service and checks if it is superservice. Only superservice can do actions about services.
 */
var doInit = function (serv) {
    "use strict";

    currentService = require('./../auth/nuveAuthenticator').service;
    var superService = require('./../mdb/dataBase').superService;
    currentService._id = currentService._id + '';
    return (currentService._id === superService);
};

/*
 * Post Service. Creates a new service.
 */
exports.create = function (req, res) {
    "use strict";

    if (!doInit()) {
        console.log('Service ', currentService._id, ' not authorized for this action');
        res.send('Service not authorized for this action', 401);
        return;
    }

    serviceRegistry.addService(req.body, function (result) {
        console.log('Service created: ', req.body.name);
        res.send(result);
    });
};

/*
 * Get Service. Represents a determined service.
 */
exports.represent = function (req, res) {
    "use strict";

    if (!doInit()) {
        console.log('Service ', currentService, ' not authorized for this action');
        res.send('Service not authorized for this action', 401);
        return;
    }

    serviceRegistry.getList(function (list) {
        console.log('Representing services');
        res.send(list);
    });
};