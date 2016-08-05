/*global require, __dirname, console*/
var express = require('express'),
    bodyParser = require('body-parser'),
    errorhandler = require('errorhandler'),
    morgan = require('morgan'),
    net = require('net'),
    N = require('./nuve'),
    fs = require("fs"),
    https = require("https"),
        config = require('./../../licode_config');

var options = {
    key: fs.readFileSync('../../cert/key.pem').toString(),
    cert: fs.readFileSync('../../cert/cert.pem').toString()
};

var app = express();

// app.configure ya no existe
"use strict";
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

//app.set('views', __dirname + '/../views/');
//disable layout
//app.set("view options", {layout: false});

N.API.init(config.nuve.superserviceID, config.nuve.superserviceKey, 'http://localhost:3000/');

var defaultRoom;
var defaultRoomName = "basicExampleRoom";

var getOrCreateRoom = function (roomName, callback) {

    if (roomName == defaultRoomName && defaultRoom) {
        console.log("It's the default Room", roomName);
        callback(defaultRoom);
        return;
    }

    N.API.getRooms(function (roomlist){
        "use strict";
        var theRoom = "";
        var rooms = JSON.parse(roomlist);
        for (var room in rooms) {
            if (rooms[room].name === roomName && rooms[room].data && rooms[room].data.basicExampleRoom){
                theRoom = rooms[room]._id;
                console.log("Found Room", roomName);
                callback(theRoom);
                return;
            }
        }
        N.API.createRoom(roomName, function (roomID) {
            theRoom = roomID._id;
            console.log("Created Room", theRoom);
            callback(theRoom);
        }, function(){}, {data: {basicExampleRoom:true}});
    });
};
var deleteRoomsIfEmpty = function (theRooms, callback) {
    if (theRooms.length == 0){
        callback(true);
        return;
    }
    var theRoom = theRooms.pop()
    N.API.getUsers(theRoom._id, function(userlist){
        var users = JSON.parse(userlist);
        console.log("Checking room", theRoom.name, "users", Object.keys(users).length);
        if (Object.keys(users).length === 0){
            N.API.deleteRoom(theRoom._id, function(res){
                console.log("Deleted Room", theRoom.name);
                if (theRooms.length > 0){
                    deleteRoomsIfEmpty(theRooms, callback);
                }
            });
        } else {
            if (theRooms.length > 0){
                deleteRoomsIfEmpty(theRooms, callback);
            }
        }
    });
}

var cleanExampleRooms = function (callback) {
    console.log("Cleaning old BasicExample Rooms");
    N.API.getRooms(function (roomlist) {
        "use strict";
        var rooms = JSON.parse(roomlist);
        var roomsToCheck = [];
        for (var room in rooms){
            if (rooms[room].data && rooms[room].data.basicExampleRoom && rooms[room].name !== defaultRoomName){
                roomsToCheck.push(rooms[room]);
            }
        }
        deleteRoomsIfEmpty (roomsToCheck, function (res) {
            callback("done");
        });
    });

}

app.get('/getRooms/', function(req, res) {
    "use strict";
    N.API.getRooms(function(rooms) {
        res.send(rooms);
    });
});

app.get('/getUsers/:room', function(req, res) {
    "use strict";
    var room = req.params.room;
    N.API.getUsers(room, function(users) {
        res.send(users);
    });
});


app.post('/createToken/', function(req, res) {
    "use strict";
    var room = defaultRoomName;
    if (req.body.room && !isNaN(req.body.room))
        room = req.body.room

    var username = req.body.username,
    role = req.body.role;

    getOrCreateRoom(room, function (roomId) {
        N.API.createToken(roomId, username, role, function(token) {
            console.log(token);
            res.send(token);
        }, function(error) {
            console.log(error);
            res.status(401).send('No Erizo Controller found');
        });
    });
});


app.use(function(req, res, next) {
    "use strict";
    res.header('Access-Control-Allow-Origin', '*');
    res.header('Access-Control-Allow-Methods', 'POST, GET, OPTIONS, DELETE');
    res.header('Access-Control-Allow-Headers', 'origin, content-type');
    if (req.method == 'OPTIONS') {
        res.send(200);
    } else {
        next();
    }
});

cleanExampleRooms(function(res) {
    console.log("Rooms Clean");
    getOrCreateRoom(defaultRoomName, function (roomId) {
        console.log("Got default Room", roomId);
        defaultRoom = roomId;
        app.listen(3001);
        var server = https.createServer(options, app);
        server.listen(3004);

    });
});
