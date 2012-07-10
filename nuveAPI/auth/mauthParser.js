var crypto = require('crypto');

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

exports.calculateClientSignature = function(params, key) {

	var toSign = params.timestamp + ',' + params.cnonce;
	if(params.username != undefined && params.role != undefined) {
		toSign += ',' + params.username + ',' + params.role;
	}

	var signed = crypto.createHmac('sha1', key).update(toSign).digest('hex');

	return (new Buffer(signed)).toString('base64');
}

exports.calculateServerSignature = function(params, key) {

	var toSign = params.timestamp;
	var signed = crypto.createHmac('sha1', key).update(toSign).digest('hex');

	return (new Buffer(signed)).toString('base64');
}

//Authorization: MAuth realm="http://marte3.dit.upm.es",mauth_signature_method="HMAC_SHA1",mauth_serviceid="prueba",mauth_timestamp="1231321321",mauth_cnonce="123123aadf",mauth_version="3.1",mauth_username="user",mauth_role="participant",mauth_signature="asasasa"