var serviceRegistry = require('./../mdb/serviceRegistry');
var BSON = require('mongodb').BSONPure;

var service;

/*
 * Gets the service and checks if it is superservice. Only superservice can do actions about services.
 */
var doInit = function (serv) {
	this.service = require('./../auth/nuveAuthenticator').service;
	var superService = require('./../mdb/dataBase').superService;
	return (this.service._id == superService);
};

/*
 * Post Service. Creates a new service.
 */
exports.create = function(req, res) {
	if(!doInit()){
		console.log('Service ', this.service._id, ' not authorized for this action');
		res.send('Service not authorized for this action', 401);
		return;
	}

	serviceRegistry.addService(req.body, function(result) {
		console.log('Service created: ', req.body.name);
		res.send(result);
	});
	

	
};

/*
 * Get Service. Represents a determined service.
 */
exports.represent = function(req, res) {
	if(!doInit()){
		console.log('Service ', this.service, ' not authorized for this action');
		res.send('Service not authorized for this action', 401);
		return;
	}
	
	serviceRegistry.getList(function(list) {
		console.log('Representing services');
		res.send(list);
	});	

};