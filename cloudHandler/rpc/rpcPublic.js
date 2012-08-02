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

exports.getTheBestEC = function(callback) {
	cloudHandler.getTheBestEC(function(host) {
		callback(host);
	});
}