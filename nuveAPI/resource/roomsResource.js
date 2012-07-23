var roomRegistry = require('./../mdb/roomRegistry');
var serviceRegistry = require('./../mdb/serviceRegistry');

var service;

/*
 * Gets the service for the proccess of the request.
 */
var doInit = function () {
	this.service = require('./../auth/nuveAuthenticator').service;
};

/*
 * Post Room. Creates a new room for a determined service.
 */
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
	
	roomRegistry.addRoom(req.body, function(result) {
		this.service.rooms.push(result);
		serviceRegistry.updateService(this.service);
		console.log('Room created:', req.body.name, 'for service', this.service.name);
		res.send(result);
	});
	

};

/*
 * Get Rooms. Represent a list of rooms for a determined service.
 */
exports.represent = function(req, res) {
	doInit();
	if (this.service == undefined) {
		res.send('Service not found', 404);
		return;
	}
	console.log('Representing rooms for service ', this.service._id);
	
	res.send(this.service.rooms);
};
