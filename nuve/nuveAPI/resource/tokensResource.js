var roomRegistry = require('./../mdb/roomRegistry');
var tokenRegistry = require('./../mdb/tokenRegistry');
var serviceRegistry = require('./../mdb/serviceRegistry');
var dataBase= require('./../mdb/dataBase');
var rpc = require('./../rpc/rpc');
var crypto = require('crypto');

var service;
var room;

/*
 * Gets the service and the room for the proccess of the request.
 */
var doInit = function (roomId, callback) {
	this.service = require('./../auth/nuveAuthenticator').service;

	serviceRegistry.getRoomForService(roomId, this.service, function(room) {
		this.room = room;
		callback();
	});

};

/*
 * Post Token. Creates a new token for a determined room of a service.
 */
exports.create = function(req, res) {
	doInit(req.params.room, function() {

		if (this.service == undefined) {
			console.log('Service not found');
			res.send('Service not found', 404);
			return;
		} else if (this.room == undefined) {
			console.log('Room ', req.params.room, ' does not exist');
			res.send('Room does not exist', 404);
			return;
		}

		generateToken(function(tokenS) {
			
			if (tokenS === undefined) {
				res.send('Name and role?', 401);
				return;
			}
			if (tokenS == 'error') {
				res.send('CloudHandler does not respond', 401);
				return;
			}
			console.log('Created token for room ', this.room._id, 'and service ', this.service._id);
			res.send(tokenS);
			
		});
	});
	
};

/*
 * Generates new token. 
 * The format of a token is:
 * {tokenId: id, host: erizoController host, signature: signature of the token};
 */
var generateToken = function(callback) {

	var user = require('./../auth/nuveAuthenticator').user;
	var role = require('./../auth/nuveAuthenticator').role;

	if (user == undefined || user == '') {
		callback(undefined);
		return;
	}

	var token = {};
	token.userName = user;
	token.room = this.room._id;
	token.role = role;
	token.service = this.service._id;
	token.creationDate = new Date();

	var r = '' +  this.room._id;
	var tr = undefined; 

	if (this.service.testRoom !== undefined) {
		tr = '' + this.service.testRoom._id;
	}

	if (tr == r) {

		if (this.service.testToken === undefined) {
			token.use = 0;
			token.host = dataBase.testErizoController;

			console.log('Creating testToken');

			tokenRegistry.addToken(token, function(id) {

				token._id = id;
				this.service.testToken = token;
				serviceRegistry.updateService(this.service);

				var tokenS = getTokenString(id, token);
				callback(tokenS);
				return;
			});

		} else {
			
			token = this.service.testToken;

			console.log('TestToken already exists, sending it', token);
		
			var tokenS = getTokenString(token._id, token);
			callback(tokenS);
			return;

		}		
	} else {

		rpc.callRpc('cloudHandler', 'getErizoControllerForRoom', this.room._id, function(ec) {

			if (ec == 'timeout') {
				callback('error');
				return;
			}

			token.host = ec.ip + ':8080';

			tokenRegistry.addToken(token, function(id) {

				var tokenS = getTokenString(id, token);
				callback(tokenS);
			});
		});
	}
}


var getTokenString = function(id, token) {

	var toSign = id + ',' + token.host;
	var hex = crypto.createHmac('sha1', dataBase.nuveKey).update(toSign).digest('hex');
	var signed = (new Buffer(hex)).toString('base64');

	var tokenJ = {tokenId: id, host: token.host, signature: signed};
	var tokenS = (new Buffer(JSON.stringify(tokenJ))).toString('base64');

	return tokenS;
}