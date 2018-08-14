/*global require, __dirname, console*/
'use strict';
var express         = require('express'),
    bodyParser      = require('body-parser'),
    errorhandler    = require('errorhandler'),
    morgan          = require('morgan'),
    N               = require('./nuve'),
    fs              = require('fs'),
    https           = require('https'),
    config          = require('./../../licode_config');

var options = {
    key : fs.readFileSync('../../cert/key.pem').toString(),
    cert: fs.readFileSync('../../cert/cert.pem').toString()
};

if (config.erizoController.sslCaCerts) {
    options.ca = [];
    for (var ca in config.erizoController.sslCaCerts) {
        options.ca.push(fs.readFileSync(config.erizoController.sslCaCerts[ca]).toString());
    }
}

GLOBAL._triedHooks = {}
function callRoomHooks() {
    N.API.getRooms(function (result) {
        JSON.parse(result).forEach(checkRoomExists);
    });
}

function checkRoomExists(room) {
    if (room.data.creationTime + 30000 > Date.now()) {
        return;
    }
    N.API.getUsers(room._id, function (result) {
 
        if (JSON.parse(result).length <= 0) {

            if (!room.data || !room.data.hookUrl) {
                N.API.deleteRoom(room._id, function () {});
                return;
            }

            if (_triedHooks.hasOwnProperty(room._id)) {
                _triedHooks[room._id] += 1;
            } else {
                _triedHooks[room._id] = 1;
            }

            request.post(
                {
                    'url' : room.data.hookUrl,
                    form : {event : "finished"},
                    strictSSL : false
                },
                function (error, response, body) {
                    if (!error && response.statusCode == 200) {
                        N.API.deleteRoom(room._id, function () {});
                        return;
                    }

                    if (room._id in _triedHooks && _triedHooks[room._id] > 3) {
                        delete _triedHooks[room._id];
                        N.API.deleteRoom(room._id, function () {});
                    }
                }
            );
        }
    });
}

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
app.use(bodyParser.urlencoded({extended: true}));


//app.set('views', __dirname + '/../views/');
//disable layout
//app.set("view options", {layout: false});

N.API.init(config.nuve.superserviceID, config.nuve.superserviceKey, 'http://localhost:3000/');

N.API.getRooms(function (roomlist) {
    JSON.parse(roomlist).forEach(checkRoomExists);
});

setInterval(callRoomHooks, 30000);
app.post('/session/create', function (req, res) {
    N.API.getRooms(function (r) { console.log(r);});
    var roomName   = req.body.room;
    var hookUrl    = req.body.hook_url;
    var data       = {
        data : {
            hookUrl      : hookUrl,
            creationTime : Date.now()
        }
    };
    if (!roomName) {
        res.send(JSON.stringify({success : false, error : 'Invalid Room Name'}));
    }
    N.API.createRoom(
        roomName,
        function (room) {
            res.status(200).send({room_name : room.name, session_id : room._id});
        },
        function () {
            res.status(400).send();
        },
        data
    );
});

app.post('/token/generate', function (req, res) {
    "use strict";
    var session_id = req.body.session_id;
    var user_id    = req.body.user_id;
    var role       = req.body.role;
    var data       = req.body.data || [];

    N.API.getRoom(
        session_id,
        function (resp) {
            var room= JSON.parse(resp);
            N.API.createToken(room._id, user_id, role, function (token) {
                res.status(200).send({token: token, success: true});
            }, function () {
                res.status(400).send();
            });
        },
        function () {
            res.status(410).send();
        }
    );
});

app.get('/session/:session_id/users', function(req, res) {
    "use strict";
    var room = req.params.session_id;

    N.API.getUsers(room, function(users) {
        res.send(users);
    }, function () {
        res.status(400).send({error : 'Invalid room'});
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

app.listen(3001);
var server = https.createServer(options, app);
server.listen(3004);
