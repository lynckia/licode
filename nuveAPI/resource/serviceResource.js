var serviceRegistry = require('./../mdb/serviceRegistry');
var BSON = require('mongodb').BSONPure;

var doInit = function (serv, callback) {
	var service = require('./../auth/nuveAuthenticator').service;
	var superService = require('./../mdb/dataBase').superService;
	if(service._id != superService) {
		callback('error');
	} else {
		serviceRegistry.getService(serv, function(ser) {
			callback(ser);
		});
	}
};

//Get
exports.represent = function(req, res) {
	doInit(req.params.service, function(serv) {
		if(serv == 'error'){
			console.log('Service not authorized for this action');
			res.send('Service not authorized for this action', 401);
			return;
		}
		if (serv == undefined) {
			res.send('Service not found', 404);
			return;
		}
		res.send(serv);
	});
	
};

//Delete
exports.deleteService = function(req, res) {
	doInit(req.params.service, function(serv) {
		if(serv == 'error'){
			console.log('Service not authorized for this action');
			res.send('Service not authorized for this action', 401);
			return;
		}
		if (serv == undefined) {
			res.send('Service not found', 404);
			return;
		}
		var id = '';
		id += serv._id;
		serviceRegistry.removeService(id);
		res.send('Service deleted');
	});
};