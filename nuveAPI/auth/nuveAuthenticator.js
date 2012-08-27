var db = require('./../mdb/dataBase').db;
var serviceRegistry = require('./../mdb/serviceRegistry');
var mauthParser = require('./mauthParser');

exports.service;
exports.user;
exports.role;

var cache = {};

/*
 * This function has the logic needed for authenticate a nuve request. 
 * If the authentication success exports the service and the user and role (if needed). Else send back 
 * a response with an authentication request to the client.
 */
exports.authenticate = function(req, res, next) {

	var authHeader = req.header('Authorization');

	var challengeReq = 'MAuth realm="http://marte3.dit.upm.es"';

	if(authHeader != undefined) {

		var params = mauthParser.parseHeader(authHeader);

		// Get the service from the data base.
		serviceRegistry.getService(params.serviceid, function(serv) {
			if (serv == undefined) {
				console.log('[Auth] Unknow service:', params.serviceid);		
				res.send(401, {'WWW-Authenticate': challengeReq});
				return;
			}

			var key = serv.key;

			// Check if timestam and cnonce are valids in order to avoid duplicate requests.
			if(!checkTimestamp(serv, params)) {
				console.log('[Auth] Invalid timestamp or cnonce');
				res.send(401, {'WWW-Authenticate': challengeReq});
				return;
			}

			// Check if the signature is valid. 
			if(checkSignature(params, key)) {

				if(params.username != undefined && params.role != undefined) {
					exports.user = params.username;
					exports.role = params.role;
				}

				cache[serv.name] =  params;
				exports.service = serv;

				// If everything in the authentication is valid continue with the request.
				next();

			} else {
				console.log('[Auth] Wrong credentials');
				res.send(401, {'WWW-Authenticate': challengeReq});
				return;
			}	
			
		});

	} else {
		console.log('[Auth] MAuth header not presented');
		res.send(401, {'WWW-Authenticate': challengeReq});
		return;
	}
	
}

var checkTimestamp = function(ser, params) {

	var lastParams = cache[ser.name];

	if(lastParams == undefined) {
		return true;
	}

	var lastTS = lastParams.timestamp;
	var newTS = params.timestamp;
	var lastC = lastParams.cnonce;
	var newC = params.cnonce;

	if(newTS < lastTS || (lastTS == newTS && lastC == newC)) {
		console.log('Last timestamp: ', lastTS, ' and new: ', newTS);
		console.log('Last cnonce: ', lastC, ' and new: ', newC);
		return false;
	}

	return true;
}

var checkSignature = function(params, key) {

	if(params.signature_method != 'HMAC_SHA1') {
		return false;
	}

	var calculatedSignature = mauthParser.calculateClientSignature(params, key);

	if(calculatedSignature != params.signature) {
		return false;
	} else {
		return true;
	}
}
