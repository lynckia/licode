/* global require, exports */

// eslint-disable-next-line import/no-extraneous-dependencies
const mongojs = require('mongojs');

// eslint-disable-next-line import/no-unresolved
const config = require('./../../../licode_config');

config.nuve = config.nuve || {};
config.nuve.dataBaseURL = config.nuve.dataBaseURL || 'localhost/nuvedb';
config.nuve.superserviceID = config.nuve.superserviceID || '';
config.nuve.superserviceKey = config.nuve.superserviceKey || '';
config.nuve.testErizoController = config.nuve.testErizoController || 'localhost:8080';

const databaseUrl = config.nuve.dataBaseURL;

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
 *    ip: ip,
 *    state: 2,
 *    keepAlive: 0,
 *    hostname: hostname,
 *    port: port,
 *    ssl: ssl,
 *    _id: ObjectId
 *  };
 *
 */
const collections = ['rooms', 'tokens', 'services', 'erizoControllers'];

exports.db = mongojs(databaseUrl, collections);

// Superservice ID
exports.superService = config.nuve.superserviceID;

// Superservice key
exports.nuveKey = config.nuve.superserviceKey;

exports.testErizoController = config.nuve.testErizoController;
