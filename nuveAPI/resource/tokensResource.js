var roomRegistry = require('./../mdb/roomRegistry');
var tokenRegistry = require('./../mdb/tokenRegistry');
var dataBase= require('./../mdb/dataBase');
var crypto = require('crypto');

var service;
var room;

var doInit = function (roomId, callback) {
	this.service = require('./../auth/nuveAuthenticator').service;

	serviceRegistry.getRoomForService(roomId, this.service, function(room) {
		this.room = room;
		callback();
	});

};

//Post
exports.create = function(req, res) {
	doInit(req.params.room, function() {

		if (this.service == undefined) {
			console.log('Without service');
			res.send('Without service', 401);
			return;
		} else if (this.room == undefined) {
			console.log('Room not found');
			res.send('Room not found', 404);
			return;
		}

		generateToken(function(tokenS) {
			
			if (tokenS == undefined) {
				res.send('Name and role?', 401);
				return;
			}
			console.log('Sending token');
			res.send(tokenS);
			
		});
	});
	
};


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
	
	token.host = dataBase.erizoControllerHost;


	//***********************************
	//TODO: Comprobaciones de salas y tal
	//***********************************


	tokenRegistry.addToken(token, function(id) {

		var toSign = id + ',' + token.host;
		var hex = crypto.createHmac('sha1', dataBase.nuveKey).update(toSign).digest('hex');
		var signed = (new Buffer(hex)).toString('base64');

		var tokenJ = {tokenId: id, host: token.host, signature: signed};
		var tokenS = (new Buffer(JSON.stringify(tokenJ))).toString('base64');

		callback(tokenS);
	});
	

}



