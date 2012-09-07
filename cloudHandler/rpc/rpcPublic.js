var cloudHandler = require('./../cloudHandler');

exports.addNewErizoController = function(ip, callback) {
	cloudHandler.addNewErizoController(ip, function (id) {
		callback(id);	
	});
}

exports.keepAlive = function(id, callback) {
	cloudHandler.keepAlive(id, function(result) {
		callback(result);
	});
}

exports.setInfo = function(params, callback) {
	cloudHandler.setInfo(params);
	callback();
}

exports.killMe = function(ip, callback) {
	cloudHandler.killMe(ip);
	callback();
}

exports.getErizoControllerForRoom = function(roomId, callback) {
	cloudHandler.getErizoControllerForRoom(roomId, function(ec) {
		callback(ec);
	});
}

exports.getUsersInRoom = function(id, callback) {
    cloudHandler.getUsersInRoom(id, function(users) {
    	callback(users);
    });
}

exports.deleteRoom = function(roomId, callback) {
	cloudHandler.deleteRoom(roomId, function(result) {
	   	callback(result);
    });
}
