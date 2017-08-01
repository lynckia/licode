/*global require, exports*/
'use strict';
var config = require('config');

var nuveConfig = config.get('nuve');

var databaseUrl = nuveConfig.dataBaseURL;

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
 *		keepAliveTs: new Date(),
 *		hostname: hostname,
 *		port: port,
 *		ssl: ssl,
 *		_id: ObjectId
 *	};
 *
 */
var collections = ['rooms', 'tokens', 'services', 'erizoControllers'];
var mongojs = require('mongojs');
exports.db = mongojs(databaseUrl, collections);

// Superservice ID
exports.superService = nuveConfig.superserviceID;

// Superservice key
exports.nuveKey = nuveConfig.superserviceKey;

exports.testErizoController = nuveConfig.testErizoController;
