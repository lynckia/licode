/*global require, __dirname, console*/
'use strict';
var express = require('express'),
    bodyParser = require('body-parser'),
    errorhandler = require('errorhandler'),
    morgan = require('morgan'),
    N = require('./nuve'),
    fs = require('fs'),
    https = require('https'),
    config = require('config');

var app = express();

// app.configure ya no existe
app.use(errorhandler({
    dumpExceptions: true,
    showStack: true
}));
app.use(morgan('dev'));
app.use(express.static(__dirname + '/public'));

app.use(bodyParser.json());
app.use(bodyParser.urlencoded({
    extended: true
}));
app.use(function(req, res, next) {
  res.header('Access-Control-Allow-Origin', '*');
  res.header('Access-Control-Allow-Headers', 'Origin, X-Requested-With, Content-Type, Accept');
  next();
});

var NUVE_SUPERSERVICE_ID = config.get('nuve.superserviceID');
var NUVE_SUPERSERVICE_KEY = config.get('nuve.superserviceKey');
var NUVE_ENDPOINT = config.get('nuve.nuveEndpoint');

N.API.init(NUVE_SUPERSERVICE_ID, NUVE_SUPERSERVICE_KEY, NUVE_ENDPOINT);

var defaultRoom;
const defaultRoomName = 'basicExampleRoom';

var getOrCreateRoom = function (name, type = 'erizo', callback = function(){}) {

    if (name === defaultRoomName && defaultRoom) {
        callback(defaultRoom);
        return;
    }

    N.API.getRooms(function (roomlist){
        var theRoom = '';
        var rooms = JSON.parse(roomlist);
        for (var room of rooms) {
            if (room.name === name &&
                room.data &&
                room.data.basicExampleRoom){

                theRoom = room._id;
                callback(theRoom);
                return;
            }
        }

        let extra = {data: {basicExampleRoom: true}};
        if (type === 'p2p') extra.p2p = true;

        N.API.createRoom(name, function (roomID) {
            theRoom = roomID._id;
            callback(theRoom);
        }, function(){}, extra);
    });
};

var deleteRoomsIfEmpty = function (theRooms, callback) {
    if (theRooms.length === 0){
        callback(true);
        return;
    }
    var theRoomId = theRooms.pop()._id;
    N.API.getUsers(theRoomId, function(userlist) {
        var users = JSON.parse(userlist);
        if (Object.keys(users).length === 0){
            N.API.deleteRoom(theRoomId, function(){
                deleteRoomsIfEmpty(theRooms, callback);
            });
        } else {
            deleteRoomsIfEmpty(theRooms, callback);
        }
    }, function (error, status) {
        console.log('Error getting user list for room ', theRoomId, 'reason: ', error);
        switch (status) {
            case 404:
                deleteRoomsIfEmpty(theRooms, callback);
                break;
            case 503:
                N.API.deleteRoom(theRoomId, function(){
                    deleteRoomsIfEmpty(theRooms, callback);
                });
                break;
        }
    });
};

var cleanExampleRooms = function (callback) {
    console.log('Cleaning basic example rooms');
    N.API.getRooms(function (roomlist) {
        var rooms = JSON.parse(roomlist);
        var roomsToCheck = [];
        for (var room of rooms){
            if (room.data &&
                room.data.basicExampleRoom &&
                room.name !== defaultRoomName){

                roomsToCheck.push(room);
            }
        }
        deleteRoomsIfEmpty (roomsToCheck, function () {
            callback('done');
        });
    });

};

app.get('/getRooms/', function(req, res) {
    N.API.getRooms(function(rooms) {
        res.send(rooms);
    });
});

app.get('/getUsers/:room', function(req, res) {
    var room = req.params.room;
    N.API.getUsers(room, function(users) {
        res.send(users);
    });
});


app.post('/createToken/', function(req, res) {
    console.log('Creating token. Request body: ',req.body);

    let username = req.body.username;
    let role = req.body.role;

    let room = defaultRoomName, type, roomId;

    if (req.body.room && !isNaN(req.body.room)) room = req.body.room;
    if (req.body.type) type = req.body.type;
    if (req.body.roomId) roomId = req.body.roomId;

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
      getOrCreateRoom(room, type, createToken);
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

cleanExampleRooms(function() {
    getOrCreateRoom(defaultRoomName, undefined, function (roomId) {
        defaultRoom = roomId;
        app.listen(3001);
        console.log('BasicExample started');
    });
});
