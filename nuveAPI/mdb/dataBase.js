var databaseUrl = "chotis2.dit.upm.es/mydb1";

/*
 * Data base collections and its fields are:
 * 
 * room {name: '', _id: ObjectId}
 *
 * service {name: '', key: '', rooms: Array, _id: ObjectId}
 *
 * token {host: '', userName: '', room: '', role: '', service: '', creationDate: Date(), _id: ObjectId}
 *
 */
var collections = ["rooms", "tokens", "services"];
exports.db = require("mongojs").connect(databaseUrl, collections);

// Superservice ID
exports.superService = '5000144598eb6bc7c198c2ae';

// Superservice key
exports.nuveKey = 'claveNuve';

