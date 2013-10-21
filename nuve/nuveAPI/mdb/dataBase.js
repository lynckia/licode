/*global require, exports*/
var config = require('./../../../licode_config');

var databaseUrl = config.nuve.dataBaseURL;

/*
 * Data base collections and its fields are:
 * 
 * room {name: '', [p2p: bool], _id: ObjectId}
 *
 * service {name: '', key: '', rooms: Array[room], testRoom: room, testToken: token, _id: ObjectId}
 *
 * token {host: '', userName: '', room: '', role: '', service: '', creationDate: Date(), [use: int], [p2p: bool], _id: ObjectId}
 *
 */
var collections = ["rooms", "tokens", "services"];
exports.db = require("mongojs").connect(databaseUrl, collections);

// Superservice ID
exports.superService = config.nuve.superserviceID;

// Superservice key
exports.nuveKey = config.nuve.superserviceKey;

exports.testErizoController = config.nuve.testErizoController;
