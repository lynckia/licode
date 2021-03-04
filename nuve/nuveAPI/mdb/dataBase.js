/* global require, exports */

// eslint-disable-next-line import/no-extraneous-dependencies
const { MongoClient, ObjectId } = require('mongodb');

// eslint-disable-next-line import/no-unresolved
const config = require('./../../../licode_config');

config.nuve = config.nuve || {};
config.nuve.dataBaseURL = config.nuve.dataBaseURL || 'mongodb://localhost/nuvedb';
config.nuve.dataBaseName = config.nuve.dataBaseName || 'nuvedb';
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
// Create a new MongoClient
exports.client = new MongoClient(databaseUrl, { useUnifiedTopology: true });

// Use connect method to connect to the Server
exports.client.connect((err) => {
  if (err) {
    // eslint-disable-next-line no-console
    console.log('Error connecting to MongoDB server', databaseUrl, err);
    return;
  }
  exports.db = exports.client.db(config.nuve.dataBaseName);
});

exports.ObjectId = ObjectId;

// Superservice ID
exports.superService = config.nuve.superserviceID;

// Superservice key
exports.nuveKey = config.nuve.superserviceKey;

exports.testErizoController = config.nuve.testErizoController;
