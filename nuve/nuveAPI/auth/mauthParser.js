var crypto = require('crypto');

/*
 * Parses a string header to a json with the fields of the authentication header.
 */
exports.parseHeader = function(header) {

	var params = {};

	var array = new Array();
	var p = header.split(',');

	for(var i = 0; i < p.length; i++) {

		array = p[i].split('=');
		var val= '';
		
		for(var j = 1; j < array.length; j++) {
			if (array[j] == '') {
				val += '=';
			} else {
				val += array[j];
			}
		}

		params[array[0].slice(6)] = val;
	
	}
	return params;
}

/*
 * Makes a string header from a json with the fields of an authentication header.
 */
exports.makeHeader = function(params) {

	if(params.realm == undefined) {
		return undefined;
	}

	var header = 'MAuth realm=\"' + params.realm + '\"';
	
	for(var key in params) {
		if(key == 'realm') {
			continue;
		}
		header += ',mauth_' + key + '=\"' + params[key] + '\"';
	}
	return header;
}

/*
 * Given a json with the header params and the key calculates the signature.
 */
exports.calculateClientSignature = function(params, key) {

	var toSign = params.timestamp + ',' + params.cnonce;
	if(params.username != undefined && params.role != undefined) {
		toSign += ',' + params.username + ',' + params.role;
	}

	var signed = crypto.createHmac('sha1', key).update(toSign).digest('hex');

	return (new Buffer(signed)).toString('base64');
}

/*
 * Given a json with the header params and the key calculates the signature.
 */
exports.calculateServerSignature = function(params, key) {

	var toSign = params.timestamp;
	var signed = crypto.createHmac('sha1', key).update(toSign).digest('hex');

	return (new Buffer(signed)).toString('base64');
}
