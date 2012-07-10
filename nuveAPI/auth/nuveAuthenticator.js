var db = require('./../mdb/dataBase').db;
var serviceRegistry = require('./../mdb/serviceRegistry');
var mauthParser = require('./mauthParser');

exports.service;
exports.user;
exports.role;

var cache = {};


exports.authenticate = function(req, res, next) {
	
	console.log('autenticando');
	
	var authHeader = req.header('Authorization');

	var challengeReq = 'MAuth realm="http://marte3.dit.upm.es"';

	if(authHeader != undefined) {

		var params = mauthParser.parseHeader(authHeader);

		//console.log('Params:', params);
		//var string = mauthParser.makeHeader(params);
		//console.log('Header:', string);

		serviceRegistry.getService(params.serviceid, function(serv) {
			if (serv == undefined) {
				console.log('Unknow service:', params.serviceid);		
				res.send(401, {'WWW-Authenticate': challengeReq});
				return;
			}

			var key = serv.key;

			if(!checkTimestamp(serv, params)) {
				res.send(401, {'WWW-Authenticate': challengeReq});
				return;
			}

			if(checkSignature(params, key)) {

				if(params.username != undefined && params.role != undefined) {
					exports.user = params.username;
					exports.role = params.role;

				}

				cache[serv.name] =  params;
				exports.service = serv;
				next();

			} else {
				console.log('Wrong credentials');
				res.send(401, {'WWW-Authenticate': challengeReq});
				return;
			}	
			
		});

	} else {
		console.log('MAuth header not presented');
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

	if(newTS < lastTS || (lastTS == newTS && lastParams.cnonce == params.cnonce)) {
		console.log('Invalid timestamp or cnonce');
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
		console.log('Auth fail. Invalid signature.');
		return false;
	} else {
		return true;
	}
}
