var databaseUrl = "mydb1";
var collections = ["rooms", "tokens", "services"];
exports.db = require("mongojs").connect(databaseUrl, collections);

exports.superService = '5000144598eb6bc7c198c2ae';
exports.nuveKey = 'claveNuve';
exports.erizoControllerHost = 'rosendo.dit.upm.es:8080';


/*
room {name: '', _id: ObjectId}

service {name: '', key: '', rooms: Array, _id: ObjectId}

token {host: '', userName: '', room: '', role: '', service: '', creationDate: Date(), _id: ObjectId}
*/