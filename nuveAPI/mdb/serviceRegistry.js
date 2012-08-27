var db = require('./dataBase').db;
var BSON = require('mongodb').BSONPure;


/*
 * Gets a list of the services in the data base.
 */
exports.getList = function(callback) {
	db.services.find({}).toArray(function(err, services) {
		if( err || !services) console.log('Empty list'); 
    	else callback(services);
	});
}

var getService = exports.getService = function(id, callback) {
	db.services.findOne({_id: new BSON.ObjectID(id)}, function(err, service) {
		if(service == undefined) console.log("Service not found");
    	if (callback != undefined) {
    		callback(service);
    	}
  	});

}

var hasService = exports.hasService = function(id, callback) {
	
	getService(id, function(service) {
		if (service == undefined) callback(false);
		else callback(true);
	})

}

/*
 * Adds a new service to the data base.
 */
exports.addService = function(service, callback) {
	service.rooms = new Array();
 	db.services.save(service, function(error, saved) {
 		callback(saved._id);
 	});
}

/*
 * Updates a determined service in the data base.
 */
exports.updateService = function(service) {
	db.services.save(service);
}

/*
 * Removes a determined service from the data base.
 */
exports.removeService = function(id) {
	hasService(id, function(hasS) {
		if (hasS) {
			db.services.remove({_id: new BSON.ObjectID(id)});
		}
	});
}

/*
 * Gets a determined room in a determined service. Returns undefined if room does not exists. 
 */
exports.getRoomForService = function(roomId, service, callback) {

	for(var room in service.rooms) {
		if(service.rooms[room]._id == roomId) {
			callback(service.rooms[room]);
			return;
		}
	}
	callback(undefined);
}
