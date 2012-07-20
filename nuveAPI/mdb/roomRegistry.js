var db = require('./dataBase').db;
var BSON = require('mongodb').BSONPure;


var getRoom = exports.getRoom = function(id, service, callback) {
	
	db.rooms.findOne({_id: new BSON.ObjectID(id)}, function(err, room) {
		if(room == undefined) console.log("Room not found");
    	if (callback != undefined) {
    		callback(room);
    	}
	});
}

var hasRoom = exports.hasRoom = function(id, service, callback) {
	
	getRoom(id, function(room) {
		if (room == undefined) callback(false);
		else callback(true);
	})

}

exports.addRoom = function(room, service, callback) {

	db.rooms.save(room, function(error, saved) {
		service.rooms.push(saved);
		db.services.save(serv);
		callback(saved._id);
	});
}

exports.removeRoom = function(id, service) {
	hasRoom(id, function(hasR) {
		if (hasR) {
			db.rooms.remove({_id: new BSON.ObjectID(id)});
		}
	});
}
