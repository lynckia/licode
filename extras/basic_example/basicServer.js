/*global require, __dirname, console*/
var express = require('express');
var net = require('net');
var N = require('./nuve');
var config = require('./../../lynckia_config');

var app = express();

app.use(express.bodyParser());

app.configure(function () {
    "use strict";
    app.use(express.errorHandler({ dumpExceptions: true, showStack: true }));
    app.use(express.logger());
    app.use(express.static(__dirname + '/public'));
    //app.set('views', __dirname + '/../views/');
    //disable layout
    //app.set("view options", {layout: false});
});

app.use(function (req, res, next) {
    "use strict";
    res.header('Access-Control-Allow-Origin', '*');
    res.header('Access-Control-Allow-Methods', 'POST, GET, OPTIONS, DELETE');
    res.header('Access-Control-Allow-Headers', 'origin, content-type');

    next();
});

N.API.init(config.nuve.superserviceID, config.nuve.superserviceKey, 'http://localhost:3000/');

var myRoom;

N.API.getRooms(function (roomlist) {
    "use strict";
    var rooms = JSON.parse(roomlist);
    console.log(rooms.length);
    if (rooms.length === 0) {
        N.API.createRoom('myRoom', function (roomID) {
            myRoom = roomID._id;
            console.log('Created room ', myRoom);
        });
    } else {
        myRoom = rooms[0]._id;
        console.log('Using room ', myRoom);
    }
});

app.post('/createToken/', function (req, res) {
    "use strict";
    var room = myRoom,
        username = req.body.username,
        role = req.body.role;
    N.API.createToken(room, username, role, function (token) {
        console.log(token);
        res.send(token);
    });
});

app.get('/getRooms/', function (req, res) {
    "use strict";
    N.API.getRooms(function (rooms) {
        res.send(rooms);
    });
});

app.get('/getUsers/:room', function (req, res) {
    "use strict";
    var room = req.params.room;
    N.API.getUsers(room, function (users) {
        res.send(users);
    });
});

app.listen(3004);
