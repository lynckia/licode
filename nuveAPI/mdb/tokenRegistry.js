var db = require('./dataBase').db;
var BSON = require('mongodb').BSONPure;

exports.getList = function(callback) {
	
	db.tokens.find({}).toArray(function(err, tokens) {
		if( err || !tokens) console.log('Empty list'); 
    	else callback(tokens);
	});
}

var getToken = exports.getToken = function(id, callback) {
	
	db.tokens.findOne({_id: new BSON.ObjectID(id)}, function(err, token) {
		if(token == undefined) console.log('Token ', id, ' not found');
    	if (callback != undefined) {
    		callback(token);
    	}
	});
}

var hasToken = exports.hasToken = function(id, callback) {
	
	getToken(id, function(token) {
		if (token == undefined) callback(false);
		else callback(true);
	})

}

exports.addToken = function(token, callback) {
 	db.tokens.save(token, function(error, saved) {
 		callback(saved._id);
 	});
}

exports.removeToken = function(id, callback) {
	hasToken(id, function(hasT) {
		if (hasT) {
			db.tokens.remove({_id: new BSON.ObjectID(id)});
			callback();
		}
	});
}
