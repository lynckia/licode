var roomRegistry = require('./../mdb/roomRegistry');
var serviceRegistry = require('./../mdb/serviceRegistry');

var service;
var room;

var doInit = function (roomId, callback) {
	this.service = require('./../auth/nuveAuthenticator').service;

	serviceRegistry.getRoomForService(roomId, this.service, function(room) {
		this.room = room;
		callback();
	});	
};

//Get
exports.represent = function(req, res) {
	doInit(req.params.room, function() {
		if (this.service == undefined) {
			res.send('Client unathorized', 401);
		}
		else if (this.room == undefined) {
			res.send('Room does not exist', 404);	
		} else {
			res.send(this.room);
		}
		
	});
	
};

//Delete
exports.deleteRoom = function(req, res) {
	doInit(req.params.room, function() {
		if (this.service == undefined) {
			res.send('Client unathorized', 401);
		}
		else if (this.room == undefined) {
			res.send('Room does not exist', 404);	
		} else {
			var id = '';
			id += this.room._id;
			roomRegistry.removeRoom(id);

			var array = this.service.rooms;
			var index = -1;
			for (var i = 0; i < array.length; i++) {
	
				if(array[i]._id == id){
					index = i;
				}
			}
			if (index != -1) {
				this.service.rooms.splice(index, 1);
				serviceRegistry.updateService(this.service);
				res.send('Room deleted');
			}
		}
	});

};