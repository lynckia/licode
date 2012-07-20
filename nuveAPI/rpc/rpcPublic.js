var tokenRegistry = require('./../mdb/tokenRegistry');

/*
 * This function is used to consume a token. Removes it from the data base and returns to erizoController.
 */
exports.deleteToken = function(id, callback) {

	tokenRegistry.getToken(id, function(token) {

		if(token == undefined) {
			callback('error');
		} else {

			tokenRegistry.removeToken(id, function() {
                console.log('Consumed token ', token._id, 'from room ', token.room ' of service ', token.service);
				callback(token);
			});
		}
	});

}
