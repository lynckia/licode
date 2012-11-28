var config = require('./../../../lynckia_config');

var databaseUrl = config.nuve.dataBaseURL;

/*
 * Data base collections and its fields are:
 * 
 * room {name: '', _id: ObjectId}
 *
 * service {name: '', key: '', rooms: Array[room], testRoom: room, testToken: token, _id: ObjectId}
 *
 * token {host: '', userName: '', room: '', role: '', service: '', creationDate: Date(), [use: int], _id: ObjectId}
 *
 */
var collections = ["rooms", "tokens", "services"];
exports.db = require("mongojs").connect(databaseUrl, collections);

// Superservice ID
exports.superService = config.nuve.superserviceID;

// Superservice key
exports.nuveKey = config.nuve.superserviceKey;

exports.testErizoController = config.nuve.testErizoController;
