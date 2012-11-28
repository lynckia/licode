var tokenRegistry = require('./../mdb/tokenRegistry');
var serviceRegistry = require('./../mdb/serviceRegistry');

/*
 * This function is used to consume a token. Removes it from the data base and returns to erizoController.
 */
exports.deleteToken = function(id, callback) {

	tokenRegistry.getToken(id, function(token) {
		
		if(token == undefined) {
			callback('error');
		} else {

			if (token.use !== undefined) {
				//Is a test token
				var time = ((new Date()).getTime()) - token.creationDate.getTime();
				if (token.use > 490) { // || time*1000 > 3600*24*30){
					var s = '' + token.service;
					serviceRegistry.getService(s, function(service) {
						delete service.testToken;
						serviceRegistry.updateService(service);
						tokenRegistry.removeToken(id, function() {
							console.log('TestToken expiration time. Deleting ', token._id, 'from room ', token.room, ' of service ', token.service);
							callback('error');
						});

					});
					
				} else {
					token.use++;
					tokenRegistry.updateToken(token);
					console.log('Using (', token.use,') testToken ', token._id, 'for testRoom ', token.room, ' of service ', token.service);
					callback(token);
				}

			} else {
				tokenRegistry.removeToken(id, function() {
	                console.log('Consumed token ', token._id, 'from room ', token.room, ' of service ', token.service);
					callback(token);
				});
			}
		}
	});

}
