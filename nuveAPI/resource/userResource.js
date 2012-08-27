var roomRegistry = require('./../mdb/roomRegistry');
var serviceRegistry = require('./../mdb/serviceRegistry');

var service;
var room;

/*
 * Gets the service and the room for the proccess of the request.
 */
var doInit = function (roomId, callback) {
	this.service = require('./../auth/nuveAuthenticator').service;

	serviceRegistry.getRoomForService(roomId, this.service, function(room) {
		this.room = room;
		callback();
	});

};

/*
 * Get User. Represent a determined user of a room. This is consulted to erizoController using RabbitMQ RPC call.
 */
exports.getUser = function(req, res) {

	doInit(req.params.room, function() {

		if (this.service == undefined) {
			res.send('Service not found', 404);
			return;
		} else if (this.room == undefined) {
			console.log('Room ', req.params.room, ' does not exist');
			res.send('Room does not exist', 404);
			return;
		}

		var user = req.params.user;
		
		//Consultar RabbitMQ



	});

}

/*
 * Delete User. Removes a determined user from a room. This order is sent to erizoController using RabbitMQ RPC call.
 */
exports.deleteUser = function(req, res) {

	doInit(req.params.room, function() {

		if (this.service == undefined) {
			res.send('Service not found', 404);
			return;
		} else if (this.room == undefined) {
			console.log('Room ', req.params.room, ' does not exist');
			res.send('Room does not exist', 404);
			return;
		}

		var user = req.params.user;
		
		//Consultar RabbitMQ



	});
}