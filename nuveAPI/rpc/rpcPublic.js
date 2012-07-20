var tokenRegistry = require('./../mdb/tokenRegistry');

exports.deleteToken = function(id, callback) {

	tokenRegistry.getToken(id, function(token) {

		if(token == undefined) {
			callback('error');
		} else {

			tokenRegistry.removeToken(id, function() {
				callback(token);
			});
		}
	});

}
