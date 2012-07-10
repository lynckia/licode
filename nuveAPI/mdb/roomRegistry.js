var db = require('./dataBase').db;
var BSON = require('mongodb').BSONPure;

exports.getList = function(callback) {
	
	db.rooms.find({}).toArray(function(err, rooms) {
		if( err || !rooms) console.log('Empty list'); 
    	else callback(rooms);
	});
}

var getRoom = exports.getRoom = function(id, callback) {
	
	db.rooms.findOne({_id: new BSON.ObjectID(id)}, function(err, room) {
		if(room == undefined) console.log("Room not found");
    	if (callback != undefined) {
    		callback(room);
    	}
	});
}

var hasRoom = exports.hasRoom = function(id, callback) {
	
	getRoom(id, function(room) {
		if (room == undefined) callback(false);
		else callback(true);
	})

}

exports.addRoom = function(room, service, callback) {

	db.services.findOne({_id: service._id}, function(err, serv) {
		if( err || !serv) console.log("Error creating room: service not found");
    	else {
 			serv.rooms.push(room);
 			db.rooms.save(room, function(error, saved) {
 				db.services.save(serv);
 				callback(saved._id);
 			});
 			
  		}
	});
	
}

exports.removeRoom = function(id) {
	hasRoom(id, function(hasR) {
		if (hasR) {
			db.rooms.remove({_id: new BSON.ObjectID(id)});
		}
	});
}
