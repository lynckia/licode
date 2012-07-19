var roomRegistry = require('./../mdb/roomRegistry');
var rpc = require('./../rpc/rpc');

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
exports.getList = function(req, res) {
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

		rpc.callRpc('getUsersInRoom', room._id, function(users) {
			res.send(users);
		});

	});


}