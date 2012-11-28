var db = require('./dataBase').db;
var BSON = require('mongodb').BSONPure;


var getRoom = exports.getRoom = function(id, callback) {
	
	db.rooms.findOne({_id: new BSON.ObjectID(id)}, function(err, room) {
		if(room == undefined) console.log('Room ', id, ' not found');
    	if (callback != undefined) {
    		callback(room);
    	}
	});
}

var hasRoom = exports.hasRoom = function(id, callback) {
	
	getRoom(id, function(room) {
		if (room == undefined) callback(false);
		else callback(true);
	});

}

/*
 * Adds a new room to the data base.
 */
exports.addRoom = function(room, callback) {

	db.rooms.save(room, function(error, saved) {
		callback(saved);
	});
}

/*
 * Removes a determined room from the data base.
 */
exports.removeRoom = function(id) {
	hasRoom(id, function(hasR) {
		if (hasR) {
			db.rooms.remove({_id: new BSON.ObjectID(id)});
		}
	});
}
