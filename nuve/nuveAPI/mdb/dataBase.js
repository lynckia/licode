/*global require, exports*/
'use strict';
var config = require('./../../../licode_config');

config.nuve = config.nuve || {};
config.nuve.dataBaseURL = config.nuve.dataBaseURL || 'localhost/nuvedb';
config.nuve.superserviceID = config.nuve.superserviceID || '';
config.nuve.superserviceKey = config.nuve.superserviceKey || '';
config.nuve.testErizoController = config.nuve.testErizoController || 'localhost:8080';

var databaseUrl = config.nuve.dataBaseURL;

/*
 * Data base collections and its fields are:
 *
 * room {name: '', erizoControllerId: '', [p2p: bool], [data: {}], _id: ObjectId}
 *
 * service {name: '', key: '', rooms: Array[room], testRoom: room, testToken: token, _id: ObjectId}
 *
 * token {host: '', userName: '', room: '', role: '', service: '',
 *        creationDate: Date(), [use: int], [p2p: bool], _id: ObjectId}
 *
 * erizoController {
 *		ip: ip,
 *		state: 2,
 *		keepAlive: 0,
 *		hostname: hostname,
 *		port: port,
 *		ssl: ssl,
 *		_id: ObjectId
 *	};
 *
 */
var collections = ['rooms', 'tokens', 'services'];
var mongojs = require('mongojs');
exports.db = mongojs(databaseUrl, collections);

// Superservice ID
exports.superService = config.nuve.superserviceID;

// Superservice key
exports.nuveKey = config.nuve.superserviceKey;

exports.testErizoController = config.nuve.testErizoController;
