var serviceRegistry = require('./../mdb/serviceRegistry');
var BSON = require('mongodb').BSONPure;

var doInit = function (serv) {
	var service = require('./../auth/nuveAuthenticator').service;
	var superService = require('./../mdb/dataBase').superService;
	return (service._id == superService);
};

//Post
exports.create = function(req, res) {
	if(!doInit()){
		console.log('Service not authorized for this action');
		res.send('Service not authorized for this action', 401);
		return;
	}

	serviceRegistry.addService(req.body, function(result) {
		console.log('Service created:', req.body.name);
		res.send(result);
	});
	

	
};

//Get
exports.represent = function(req, res) {
	if(!doInit()){
		console.log('Service not authorized for this action');
		res.send('Service not authorized for this action', 401);
		return;
	}
	console.log('Represent services');
	serviceRegistry.getList(function(list) {
		res.send(list);
	});	

};