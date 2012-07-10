var db = require('./dataBase').db;
var BSON = require('mongodb').BSONPure;

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

exports.addService = function(service, callback) {
	service.rooms = new Array();
 	db.services.save(service, function(error, saved) {
 		callback(saved._id);
 	});
}

exports.updateService = function(service) {
	db.services.save(service);
}

exports.removeService = function(id) {
	hasService(id, function(hasS) {
		if (hasS) {
			db.services.remove({_id: new BSON.ObjectID(id)});
		}
	});
}
