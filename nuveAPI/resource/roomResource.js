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
 * Get Room. Represents a determined room.
 */
exports.represent = function(req, res) {
	doInit(req.params.room, function() {
		if (this.service == undefined) {
			res.send('Client unathorized', 401);
		}
		else if (this.room == undefined) {
			console.log('Room ', req.params.room, ' does not exist');	
			res.send('Room does not exist', 404);	
		} else {
			console.log('Representing room ', this.room._id, 'of service ', this.service._id);
			res.send(this.room);
		}
		
	});
	
};

/*
 * Delete Room. Removes a determined room from the data base and asks cloudHandler to remove ir from erizoController.
 */
exports.deleteRoom = function(req, res) {
	doInit(req.params.room, function() {
		if (this.service == undefined) {
			res.send('Client unathorized', 401);
		}
		else if (this.room == undefined) {
			console.log('Room ', req.params.room, ' does not exist');
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
				console.log('Room ', id, ' deleted for service ', this.service._id);
				rpc.callRpc('cloudHandler', 'deleteRoom', id, function() {});
				res.send('Room deleted');
			}
		}
	});

};