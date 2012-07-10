var roomRegistry = require('./../mdb/roomRegistry');

var service;
var room;

var doInit = function (room, callback) {
	this.service = require('./../auth/nuveAuthenticator').service;

	roomRegistry.getRoom(room, function(room) {
		this.room = room;
		callback();
	});

};

//Get
exports.getUser = function(req, res) {

	doInit(req.params.room, function() {

		if (this.service == undefined) {
			console.log('Without service');
			res.send('Without service', 401);
			return;
		} else if (this.room == undefined) {
			console.log('Room not found');
			res.send('Room not found', 404);
			return;
		}

		var user = req.params.user;
		
		//Consultar RabbitMQ



	});

}

//Delete
exports.deleteUser = function(req, res) {

	doInit(req.params.room, function() {

		if (this.service == undefined) {
			console.log('Without service');
			res.send('Without service', 401);
			return;
		} else if (this.room == undefined) {
			console.log('Room not found');
			res.send('Room not found', 404);
			return;
		}

		var user = req.params.user;
		
		//Consultar RabbitMQ



	});
}