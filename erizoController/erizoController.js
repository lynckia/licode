var rpc = require('./rpc/rpcClient');
var crypto = require('crypto');

var nuveKey = 'grdp1l0';
var tokenS = 'eyJ0b2tlbklkIjoiNGZlNDU1MDc4ZGVkOGRkNjA2MDAwMDAyIiwiaG9zdCI6Imh0dHA6Ly9sb2NhbGhvc3Q6ODA4MCIsInNpZ25hdHVyZSI6IlltRmhPVEppTlRBNE9UVTBNek15T1dVelpUTm1NakkwT1Rrd1pqVTBNREU0WmpRMFl6QmlZdz09In0=';

rpc.connect(function() {

	var tokenJ = JSON.parse(new Buffer(tokenS, 'base64').toString('ascii'));
	var token;

	if(checkSignature(tokenJ, nuveKey)) {

		rpc.callRpc('deleteToken', tokenJ.tokenId, function(resp) {

			if (resp == 'error') {
				console.log('Token does not exist');
			} else if (tokenJ.host == resp.host) {
				console.log('OK');
				token = resp;
				console.log(token);
			}
		});

	}

});



var checkSignature = function(token, key) {


	var calculatedSignature = calculateSignature(token, key);

	if(calculatedSignature != token.signature) {
		console.log('Auth fail. Invalid signature.');
		return false;
	} else {
		return true;
	}
}

var calculateSignature = function(token, key) {

	var toSign = token.tokenId + ',' + token.host;
	
	var signed = crypto.createHmac('sha1', key).update(toSign).digest('hex');

	return (new Buffer(signed)).toString('base64');
}

