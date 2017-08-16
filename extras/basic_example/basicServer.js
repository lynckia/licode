/*global require, __dirname, console*/
'use strict';

var express = require('express');
var bodyParser = require('body-parser');
var errorhandler = require('errorhandler');
var morgan = require('morgan');
var fs = require('fs');
var https = require('https');
var config = require('config');
var stringHash = require('string-hash');

var N = require('./nuve');

var app = express();

// app.configure ya no existe
app.use(errorhandler({
    dumpExceptions: true,
    showStack: true
}));
app.use(morgan('dev'));
app.use(express.static(__dirname + '/public'));

app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));
app.use(function(req, res, next) {
  res.header('Access-Control-Allow-Origin', '*');
  res.header('Access-Control-Allow-Headers', 'Origin, X-Requested-With, Content-Type, Accept');
  next();
});

var NUVE_SUPERSERVICE_ID = config.get('nuve.superserviceID');
var NUVE_SUPERSERVICE_KEY = config.get('nuve.superserviceKey');
var NUVE_ENDPOINT = config.get('nuve.nuveEndpoint');
var LICODE_USERS_PERCENTAGE = config.get('licodeUsersPercentage');
var MAX_STRING_HASH = 4294967295;

N.API.init(NUVE_SUPERSERVICE_ID, NUVE_SUPERSERVICE_KEY, NUVE_ENDPOINT);

var defaultRoomId;
const defaultRoomName = 'basicExampleRoom';

var shouldUseTokbox = function (roomName) {
    if (LICODE_USERS_PERCENTAGE === -1) {
        return false;
    }
    var random = stringHash(roomName) / MAX_STRING_HASH;
    return !(random < LICODE_USERS_PERCENTAGE);
};

var getOrCreateRoom = function (name, callback) {
    if (name === defaultRoomName && defaultRoomId) {
        return callback(null, defaultRoomId);
    }

    N.API.findRoomByName(name, function(room) {
        if (room) {
            return callback(null, room._id);
        }

        N.API.createRoom(name, room => callback(null, room._id), callback);
    }, callback);
};

app.post('/createToken/', function(req, res) {
    let username = req.body.username;
    let role = req.body.role;

    let room = defaultRoomName, type, roomId, alwaysUseLicode;

    if (req.body.room) room = req.body.room;
    if (req.body.roomId) roomId = req.body.roomId;
    if (req.body.alwaysUseLicode) alwaysUseLicode = !!req.body.alwaysUseLicode;

    let createToken = function (roomId) {
      N.API.createToken(roomId, username, role, function(token) {
          console.log('Token created', token);
          res.send(token);
      }, function(error) {
          console.log('Error creating token', error);
          res.status(401).send('No Erizo Controller found');
      });
    };

    if (roomId) {
      createToken(roomId);
    } else {
      if (!alwaysUseLicode && shouldUseTokbox(room)) {
        return res.json({ action: 'use_tokbox' });
      }

      getOrCreateRoom(room, (err, fetchedRoomId) => {
        if (err) {
            console.error('Failed to get or create room: %s, error:', room, err);
            res.status(500).send('Failed to create token.');
            return;
        }
        createToken(fetchedRoomId);
      });
    }
});

app.use(function(req, res, next) {
    res.header('Access-Control-Allow-Origin', '*');
    res.header('Access-Control-Allow-Methods', 'POST, GET, OPTIONS, DELETE');
    res.header('Access-Control-Allow-Headers', 'origin, content-type');
    if (req.method === 'OPTIONS') {
        res.send(200);
    } else {
        next();
    }
});

getOrCreateRoom(defaultRoomName, function (err, roomId) {
    if (err) {
        console.error('Failed to get or create default room', err);
        return process.exit(1);
    }

    defaultRoomId = roomId;
    app.listen(3001);
    console.log('BasicExample started');
});
