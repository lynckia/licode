var roomRegistry = require('./../mdb/roomRegistry');

var service;

var doInit = function () {
	this.service = require('./../auth/nuveAuthenticator').service;
};

//Post
exports.createRoom = function(req, res) {
	doInit();

	if (this.service == undefined) {
		res.send('Service not found', 404);
		return;
	}
	if(req.body.name == undefined) {
		console.log('Invalid room');
		res.send('Invalid room', 404);
		return;
	}
	
	roomRegistry.addRoom(req.body, this.service, function(result) {
		console.log('Room created:', req.body.name, 'for service', this.service.name);
		res.send(result);
	});
	

};

//Get
exports.represent = function(req, res) {
	doInit();
	if (this.service == undefined) {
		res.send('Service not found', 404);
		return;
	}
	console.log('Represent rooms');
	roomRegistry.getList(function(list) {
		res.send(list);
	});	

};
