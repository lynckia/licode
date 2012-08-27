var databaseUrl = "chotis2.dit.upm.es/mydb1";

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
exports.superService = '50001a6651336eccad9dcfdd';

// Superservice key
exports.nuveKey = 'claveNuve';

exports.testErizoController = 'chotis2.dit.upm.es:8080';
