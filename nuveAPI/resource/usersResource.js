var roomRegistry = require('./../mdb/roomRegistry');
var serviceRegistry = require('./../mdb/serviceRegistry');
var rpc = require('./../rpc/rpc');

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
 * Get Users. Represent a list of users of a determined room. This is consulted to cloudHandler using RabbitMQ RPC call.
 */
exports.getList = function(req, res) {
	doInit(req.params.room, function() {

		if (this.service == undefined) {
			res.send('Service not found', 404);
			return;
		} else if (this.room == undefined) {
			console.log('Room ', req.params.room, ' does not exist');
			res.send('Room does not exist', 404);
			return;
		}
		
		console.log('Representing users for room ', this.room._id, 'and service', this.service._id);
		rpc.callRpc('cloudHandler', 'getUsersInRoom', this.room._id, function(users) {
			if (users == 'error') {
				res.send('CloudHandler does not respond', 401);
				return;
			}
			res.send(users);
		});

	});

}
