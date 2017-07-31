/*global require, setInterval, clearInterval, Buffer, exports*/
'use strict';
var crypto = require('crypto');
var config = require('config');

var rpcPublic = require('./rpc/rpcPublic');
var ST = require('./Stream');
var Permission = require('./permission');
var Getopt = require('node-getopt');

// Configuration default values
GLOBAL.config = {};
GLOBAL.config.cloudProvider = config.get('cloudProvider');
GLOBAL.config.erizoController = config.get('erizoController');
GLOBAL.config.nuve = config.get('nuve');

// Parse command line arguments
var getopt = new Getopt([
  ['r' , 'rabbit-host=ARG'         , 'RabbitMQ Host'],
  ['g' , 'rabbit-port=ARG'         , 'RabbitMQ Port'],
  ['b' , 'rabbit-heartbeat=ARG'    , 'RabbitMQ AMQP Heartbeat Timeout'],
  ['l' , 'logging-config-file=ARG' , 'Logging Config File'],
  ['t' , 'iceServers=ARG'          , 'Ice Servers URLs Array'],
  ['b' , 'defaultVideoBW=ARG'      , 'Default video Bandwidth'],
  ['M' , 'maxVideoBW=ARG'          , 'Max video bandwidth'],
  ['i' , 'publicIP=ARG'            , 'Erizo Controller\'s public IP'],
  ['H' , 'hostname=ARG'            , 'Erizo Controller\'s hostname'],
  ['p' , 'port'                    , 'Port used by clients to reach Erizo Controller'],
  ['S' , 'ssl'                     , 'Enable SSL for clients'],
  ['L' , 'listen_port'             , 'Port where Erizo Controller will listen to new connections.'],
  ['s' , 'listen_ssl'              , 'Enable HTTPS in server'],
  ['R' , 'recording_path'          , 'Recording path.'],
  ['h' , 'help'                    , 'display this help']
]);

var PUBLISHER_INITAL = 101, PUBLISHER_READY = 104;


var opt = getopt.parse(process.argv.slice(2));

for (var prop in opt.options) {
    if (opt.options.hasOwnProperty(prop)) {
        var value = opt.options[prop];
        switch (prop) {
            case 'help':
                getopt.showHelp();
                process.exit(0);
                break;
            case 'rabbit-host':
                GLOBAL.config.rabbit = GLOBAL.config.rabbit || {};
                GLOBAL.config.rabbit.host = value;
                break;
            case 'rabbit-port':
                GLOBAL.config.rabbit = GLOBAL.config.rabbit || {};
                GLOBAL.config.rabbit.port = value;
                break;
            case 'rabbit-heartbeat':
                GLOBAL.config.rabbit = GLOBAL.config.rabbit || {};
                GLOBAL.config.rabbit.heartbeat = value;
                break;
            case 'logging-config-file':
                GLOBAL.config.logger = GLOBAL.config.logger || {};
                GLOBAL.config.logger.configFile = value;
                break;
            default:
                GLOBAL.config.erizoController[prop] = value;
                break;
        }
    }
}

// Load submodules with updated config
var logger = require('./common/logger').logger;
var amqper = require('./common/amqper');
var controller = require('./roomController');
var ecch = require('./ecCloudHandler').EcCloudHandler({amqper: amqper});

// Logger
var log = logger.getLogger('ErizoController');

var server;

if (GLOBAL.config.erizoController.listen_ssl) {  // jshint ignore:line
    var https = require('https');
    var fs = require('fs');
    var options = {
        key: fs.readFileSync(config.erizoController.ssl_key).toString(), // jshint ignore:line
        cert: fs.readFileSync(config.erizoController.ssl_cert).toString() // jshint ignore:line
    };
    if (config.erizoController.sslCaCerts) {
        options.ca = [];
        for (var ca in config.erizoController.sslCaCerts) {
            options.ca.push(fs.readFileSync(config.erizoController.sslCaCerts[ca]).toString());
        }
    }
    server = https.createServer(options);
} else {
    var http = require('http');
    server = http.createServer();
}

server.listen(GLOBAL.config.erizoController.listen_port); // jshint ignore:line
var io = require('socket.io').listen(server, {log:false});

io.set('transports', ['websocket']);

var nuveKey = GLOBAL.config.nuve.superserviceKey;

var WARNING_N_ROOMS = GLOBAL.config.erizoController.warning_n_rooms; // jshint ignore:line
var LIMIT_N_ROOMS = GLOBAL.config.erizoController.limit_n_rooms; // jshint ignore:line

var INTERVAL_TIME_KEEPALIVE = GLOBAL.config.erizoController.interval_time_keepAlive; // jshint ignore:line

var BINDED_INTERFACE_NAME = GLOBAL.config.erizoController.networkInterface;

var myId;
var rooms = {};
var myState;

var calculateSignature = function (token, key) {
    var toSign = token.tokenId + ',' + token.host,
        signed = crypto.createHmac('sha1', key).update(toSign).digest('hex');
    return (new Buffer(signed)).toString('base64');
};

var checkSignature = function (token, key) {
    var calculatedSignature = calculateSignature(token, key);

    if (calculatedSignature !== token.signature) {
        log.info('message: invalid token signature');
        return false;
    } else {
        return true;
    }
};

var sendToSocket = function(socketId, type, arg) {
  if (io.sockets.sockets.hasOwnProperty(socketId)) {
    io.sockets.sockets[socketId].emit(type, arg);
  }
};

/*
 * Sends a message of type 'type' to all sockets in a determined room.
 */
var sendMsgToRoom = function (room, type, arg) {
    var sockets = room.sockets,
        id;
    for (id in sockets) {
        if (sockets.hasOwnProperty(id)) {
            log.debug('message: sendMsgToRoom, ' +
                      'clientId: ' + sockets[id] + ', ' +
                      'roomId: ' + room.id + ', ' +
                      logger.objectToLog(type));
            sendToSocket(sockets[id], type, arg);
        }
    }
};

var privateRegexp;
var publicIP;

var addToCloudHandler = function (callback) {
    var interfaces = require('os').networkInterfaces(),
        addresses = [],
        k,
        k2,
        address;

    for (k in interfaces) {
        if (!GLOBAL.config.erizoController.networkinterface ||
            GLOBAL.config.erizoController.networkinterface === k) {
          if (interfaces.hasOwnProperty(k)) {
              for (k2 in interfaces[k]) {
                  if (interfaces[k].hasOwnProperty(k2)) {
                      address = interfaces[k][k2];
                      if (address.family === 'IPv4' && !address.internal) {
                          if (k === BINDED_INTERFACE_NAME || !BINDED_INTERFACE_NAME) {
                              addresses.push(address.address);
                          }
                      }
                  }
              }
          }
        }
    }

    privateRegexp = new RegExp(addresses[0], 'g');

    if (GLOBAL.config.erizoController.publicIP === '' ||
        GLOBAL.config.erizoController.publicIP === undefined){
        publicIP = addresses[0];
    } else {
        publicIP = GLOBAL.config.erizoController.publicIP;
    }

    var addECToCloudHandler = function(attempt) {
        if (attempt <= 0) {
            log.error('message: addECtoCloudHandler cloudHandler does not respond - fatal');
            return;
        }

        var controller = {
            cloudProvider: GLOBAL.config.cloudProvider.name,
            ip: publicIP,
            hostname: GLOBAL.config.erizoController.hostname,
            port: GLOBAL.config.erizoController.port,
            ssl: GLOBAL.config.erizoController.ssl
        };
        amqper.callRpc('nuve', 'addNewErizoController', controller, {callback: function (msg) {

            if (msg === 'timeout') {
                log.warn('message: addECToCloudHandler cloudHandler does not respondr, ' +
                         'attemptsLeft: ' + attempt );

                // We'll try it more!
                setTimeout(function() {
                    attempt = attempt - 1;
                    addECToCloudHandler(attempt);
                }, 3000);
                return;
            }
            if (msg === 'error') {
                log.error('message: cannot contact cloudHandler');
                return;
            }

            log.info('message: succesfully added to cloudHandler');

            publicIP = msg.publicIP;
            myId = msg.id;
            myState = 2;

            var intervarId = setInterval(function () {

                amqper.callRpc('nuve', 'keepAlive', myId, {'callback': function (result) {
                    if (result === 'whoareyou') {

                        // TODO: It should try to register again in Cloud Handler.
                        // But taking into account current rooms, users, ...
                        log.error('message: This ErizoController does not exist in cloudHandler ' +
                                  'to avoid unexpected behavior this ErizoController will die');
                        clearInterval(intervarId);
                        amqper.callRpc('nuve', 'killMe', publicIP, {callback: function () {}});
                    }
                }});

            }, INTERVAL_TIME_KEEPALIVE);

            callback('callback');

        }});
    };
    addECToCloudHandler(5);
};

//*******************************************************************
//       When adding or removing rooms we use an algorithm to check the state
//       If there is a state change we send a message to cloudHandler
//
//       States:
//            0: Not available
//            1: Warning
//            2: Available
//*******************************************************************
var updateMyState = function () {
    var nRooms = 0, newState, i, info;

    for (i in rooms) {
        if (rooms.hasOwnProperty(i)) {
            nRooms += 1;
        }
    }

    if (nRooms < WARNING_N_ROOMS) {
        newState = 2;
    } else if (nRooms > LIMIT_N_ROOMS) {
        log.warn('message: reached Room Limit, roomLimit:' + LIMIT_N_ROOMS);
        newState = 0;
    } else {
        log.warn('message: reached Warning room limit, ' +
                 'warningRoomLimit: ' + WARNING_N_ROOMS + ', ' +
                 'roomLimit: ' + LIMIT_N_ROOMS);
        newState = 1;
    }

    if (newState === myState) {
        return;
    }

    myState = newState;

    info = {id: myId, state: myState};
    amqper.callRpc('nuve', 'setInfo', info, {callback: function () {}});
};

var listen = function () {
    io.sockets.on('connection', function (socket) {
        log.info('message: erizoClient connected, clientId: ' + socket.id);

        // Gets 'token' messages on the socket. Checks the signature and ask nuve if it is valid.
        // Then registers it in the room and callback to the client.
        socket.on('token', function (token, callback) {

            //log.debug("New token", token);

            var tokenDB, user, streamList = [], index;
            if (checkSignature(token, nuveKey)) {

                amqper.callRpc('nuve', 'deleteToken', token.tokenId, {callback: function (resp) {
                    if (resp === 'error') {
                        log.warn('message: Trying to use token that does not exist - ' +
                                 'disconnecting Client, clientId: ' + socket.id);
                        callback('error', 'Token does not exist');
                        socket.disconnect();

                    } else if (resp === 'timeout') {
                        log.warn('message: Nuve does not respond token check - ' +
                                 'disconnecting client, clientId: ' + socket.id);
                        callback('error', 'Nuve does not respond');
                        socket.disconnect();

                    } else if (token.host === resp.host) {
                        tokenDB = resp;
                        if (rooms[tokenDB.room] === undefined) {
                            var room = {};

                            room.id = tokenDB.room;
                            room.sockets = [];
                            room.sockets.push(socket.id);
                            room.streams = {}; //streamId: Stream
                            if (tokenDB.p2p) {
                                log.debug('message: Requested token for p2p room');
                                room.p2p = true;
                            } else {
                                room.controller = controller.RoomController({amqper: amqper,
                                                                             ecch: ecch});
                                room.controller.addEventListener(function(type, event) {
                                    if (type === 'unpublish') {
                                        // It's supposed to be an integer.
                                        var streamId = parseInt(event);
                                        log.warn('message: Triggering removal of stream ' +
                                                 'because of ErizoJS timeout, ' +
                                                 'streamId: ' + streamId);
                                        sendMsgToRoom(room, 'onRemoveStream', {id: streamId});
                                        /*
                                        room.controller.removePublisher(streamId);

                                        for (var s in room.sockets) {
                                            var streams =
                                                io.sockets.sockets[room.sockets[s]].streams;
                                            var index = streams.indexOf(streamId);
                                            if (index !== -1) {
                                                streams.splice(index, 1);
                                            }
                                        }

                                        if (room.streams[streamId]) {
                                            delete room.streams[streamId];
                                        }
                                        */
                                    }

                                });
                            }
                            rooms[tokenDB.room] = room;
                            updateMyState();
                        } else {
                            rooms[tokenDB.room].sockets.push(socket.id);
                        }
                        user = {name: tokenDB.userName, role: tokenDB.role};
                        socket.user = user;
                        var permissions = GLOBAL.config.erizoController.roles[tokenDB.role] || [];
                        socket.user.permissions = {};
                        for (var right in permissions) {
                            socket.user.permissions[right] = permissions[right];
                        }
                        socket.room = rooms[tokenDB.room];
                        socket.streams = []; //[list of streamIds]
                        socket.state = 'sleeping';

                        log.debug('message: Token approved, clientId: ' + socket.id);

                        if (!tokenDB.p2p &&
                            GLOBAL.config.erizoController.report.session_events) {  // jshint ignore:line
                            var timeStamp = new Date();
                            amqper.broadcast('event', {room: tokenDB.room,
                                                       user: socket.id,
                                                       type: 'user_connection',
                                                       timestamp:timeStamp.getTime()});
                        }

                        for (index in socket.room.streams) {
                            if (socket.room.streams.hasOwnProperty(index)) {
                                if (socket.room.streams[index].status === PUBLISHER_READY){
                                    streamList.push(socket.room.streams[index].getPublicStream());
                                }
                            }
                        }

                        callback('success', {streams: streamList,
                                            id: socket.room.id,
                                            p2p: socket.room.p2p,
                                            defaultVideoBW:
                                                GLOBAL.config.erizoController.defaultVideoBW,
                                            maxVideoBW: GLOBAL.config.erizoController.maxVideoBW,
                                            iceServers: GLOBAL.config.erizoController.iceServers
                                            });

                    } else {
                        log.warn('message: Token has invalid host, clientId: ' + socket.id);
                        callback('error', 'Invalid host');
                        socket.disconnect();
                    }
                }});

            } else {
                log.warn('message: Token authentication error, clientId: ' + socket.id);
                callback('error', 'Authentication error');
                socket.disconnect();
            }
        });

        //Gets 'sendDataStream' messages on the socket in order to write a message in a dataStream.
        socket.on('sendDataStream', function (msg) {
            if  (socket.room.streams[msg.id] === undefined){
              log.warn('message: Trying to send Data from a non-initialized stream, ' +
                       'clientId: ' + socket.id + ', ' +
                       logger.objectToLog(msg));
              return;
            }
            var sockets = socket.room.streams[msg.id].getDataSubscribers(), id;
            for (id in sockets) {
                if (sockets.hasOwnProperty(id)) {
                    log.debug('message: sending dataStream, ' +
                    'clientId: ' + sockets[id] + ', dataStream: ' + msg.id);
                    sendToSocket(sockets[id], 'onDataStream', msg);
                }
            }
        });

        var hasPermission = function(user, action) {
          return user && user.permissions[action] === true;
        };

        socket.on('signaling_message', function (msg) {
            if (socket.room.p2p) {
                sendToSocket(msg.peerSocket, 'signaling_message_peer',
                        {streamId: msg.streamId, peerSocket: socket.id, msg: msg.msg});
            } else {
                var isControlMessage = msg.msg.type === 'control';
                if (!isControlMessage ||
                    (isControlMessage && hasPermission(socket.user, msg.msg.action.name))) {
                  socket.room.controller.processSignaling(msg.streamId, socket.id, msg.msg);
                } else {
                  log.info('message: User unauthorized to execute action on stream, action: ' +
                    msg.msg.action.name + ', streamId: ' + msg.streamId);
                }
            }
        });

        // Gets 'updateStreamAttributes' messages on the socket
        // in order to update attributes from the stream.
        socket.on('updateStreamAttributes', function (msg) {
            if  (socket.room.streams[msg.id] === undefined){
              log.warn('message: Update attributes to a uninitialized stream, ' +
                       logger.objectToLog(msg));
              return;
            }
            var sockets = socket.room.streams[msg.id].getDataSubscribers(), id;
            socket.room.streams[msg.id].setAttributes(msg.attrs);
            for (id in sockets) {
                if (sockets.hasOwnProperty(id)) {
                    log.debug('message: Sending new attributes, ' +
                              'clientId: ' + sockets[id] + ', streamId: ' + msg.id);
                    sendToSocket(sockets[id], 'onUpdateAttributeStream', msg);
                }
            }
        });

        // Gets 'publish' messages on the socket in order to add new stream to the room.
        // Returns callback(id, error)
        socket.on('publish', function (options, sdp, callback) {
            var id, st;
            if (socket.user === undefined || !socket.user.permissions[Permission.PUBLISH]) {
                callback(null, 'Unauthorized');
                return;
            }
            if (socket.user.permissions[Permission.PUBLISH] !== true) {
                var permissions = socket.user.permissions[Permission.PUBLISH];
                for (var right in permissions) {
                    if ((options[right] === true) && (permissions[right] === false))
                        return callback(null, 'Unauthorized');
                }
            }

            // generate a 18 digits safe integer
            id = Math.floor(100000000000000000 + Math.random() * 900000000000000000);

            if (options.state === 'url' || options.state === 'recording') {
                var url = sdp;
                if (options.state === 'recording') {
                    var recordingId = sdp;
                    if (GLOBAL.config.erizoController.recording_path) {  // jshint ignore:line
                        url = GLOBAL.config.erizoController.recording_path + recordingId + '.mkv'; // jshint ignore:line
                    } else {
                        url = '/tmp/' + recordingId + '.mkv';
                    }
                }
                socket.room.controller.addExternalInput(id, url, function (result) {
                    if (result === 'success') {
                        st = new ST.Stream({id: id, socket: socket.id,
                                            audio: options.audio,
                                            video: options.video,
                                            data: options.data,
                                            attributes: options.attributes});
                        st.status = PUBLISHER_READY;
                        socket.streams.push(id);
                        socket.room.streams[id] = st;
                        callback(id);
                        sendMsgToRoom(socket.room, 'onAddStream', st.getPublicStream());
                    } else {
                        callback(null, 'Error adding External Input');
                    }
                });
            } else if (options.state === 'erizo') {
                log.info('message: addPublisher requested, ' +
                         'streamId: ' + id + ', clientId: ' + socket.id + ', ' +
                         logger.objectToLog(options) + ', ' +
                         logger.objectToLog(options.attributes));
                socket.room.controller.addPublisher(id, options, function (signMess) {

                    if (signMess.type === 'initializing') {
                        callback(id);
                        st = new ST.Stream({id: id, socket: socket.id,
                                            audio: options.audio,
                                            video: options.video,
                                            data: options.data,
                                            screen: options.screen,
                                            attributes: options.attributes});
                        socket.streams.push(id);
                        socket.room.streams[id] = st;
                        st.status = PUBLISHER_INITAL;
                        log.info('message: addPublisher, ' +
                                 'state: PUBLISHER_INITIAL, ' +
                                 'clientId: ' + socket.id + ', ' +
                                 'streamId: ' + id);

                        if (GLOBAL.config.erizoController.report.session_events) {  // jshint ignore:line
                            var timeStamp = new Date();
                            amqper.broadcast('event', {room: socket.room.id,
                                                       user: socket.id,
                                                       name: socket.user.name,
                                                       type: 'publish',
                                                       stream: id,
                                                       timestamp: timeStamp.getTime(),
                                                       agent: signMess.agentId,
                                                       attributes: options.attributes});
                        }
                        return;
                    } else if (signMess.type === 'failed'){
                        log.warn('message: addPublisher ICE Failed, ' +
                                 'state: PUBLISHER_FAILED, ' +
                                 'streamId: ' + id + ', ' +
                                 'clientId: ' + socket.id);
                        socket.emit('connection_failed',{type:'publish', streamId: id});
                        //We're going to let the client disconnect let the client do it

                        /*
                        if (!socket.room.p2p) {
                            socket.room.controller.removePublisher(id);
                            if (GLOBAL.config.erizoController.report.session_events) {
                                var timeStamp = new Date();
                                amqper.broadcast('event', {room: socket.room.id,
                                                           user: socket.id,
                                                           type: 'failed',
                                                           stream: id,
                                                           sdp: signMess.sdp,
                                                           timestamp: timeStamp.getTime()});
                            }
                        }

                        //We assume it does not have subscribers because, for now
                        // libnice will ONLY fail on establishment
                        //TODO: If we upgrade libnice check if this is true and
                        // also notify subscribers

                        var index = socket.streams.indexOf(id);
                        if (index !== -1) {
                            socket.streams.splice(index, 1);
                        }
                        */
                        return;
                    } else if (signMess.type === 'ready') {
                        st.status = PUBLISHER_READY;
                        sendMsgToRoom(socket.room, 'onAddStream', st.getPublicStream());
                        log.info('message: addPublisher, ' +
                                 'state: PUBLISHER_READY, ' +
                                 'streamId: ' + id + ', ' +
                                 'clientId: ' + socket.id);
                    } else if (signMess === 'timeout-erizojs') {
                        log.error('message: addPublisher timeout when contacting ErizoJS, ' +
                                  'streamId: ' + id + ', clientId: ' + socket.id);
                        callback(null, 'ErizoJS is not reachable');
                        return;
                    } else if (signMess === 'timeout-agent'){
                        log.error('message: addPublisher timeout when contacting Agent, ' +
                                  'streamId: ' + id + ', clientId: ' + socket.id);
                        callback(null, 'ErizoAgent is not reachable');
                        return;
                    } else if (signMess === 'timeout'){
                        log.error('message: addPublisher Undefined RPC Timeout, ' +
                                  'streamId: ' + id + ', clientId: ' + socket.id);
                        callback(null, 'ErizoAgent or ErizoJS is not reachable');
                        return;
                    }

                    socket.emit('signaling_message_erizo', {mess: signMess, streamId: id});
                });
            } else {
                st = new ST.Stream({id: id,
                                    socket: socket.id,
                                    audio: options.audio,
                                    video: options.video,
                                    data: options.data,
                                    screen: options.screen,
                                    attributes: options.attributes});
                socket.streams.push(id);
                socket.room.streams[id] = st;
                st.status = PUBLISHER_READY;
                callback(id);
                sendMsgToRoom(socket.room, 'onAddStream', st.getPublicStream());
            }
        });

        //Gets 'subscribe' messages on the socket in order to add new
        // subscriber to a determined stream (options.streamId).
        // Returns callback(result, error)
        socket.on('subscribe', function (options, sdp, callback) {
            //log.info("Subscribing", options, callback);
            if (socket.user === undefined || !socket.user.permissions[Permission.SUBSCRIBE]) {
                callback(null, 'Unauthorized');
                return;
            }

            if (socket.user.permissions[Permission.SUBSCRIBE] !== true) {
                var permissions = socket.user.permissions[Permission.SUBSCRIBE];
                for (var right in permissions) {
                    if ((options[right] === true) && (permissions[right] === false))
                        return callback(null, 'Unauthorized');
                }
            }

            var stream = socket.room.streams[options.streamId];

            if (stream === undefined) {
                return;
            }

            if (stream.hasData() && options.data !== false) {
                stream.addDataSubscriber(socket.id);
            }

            if (stream.hasAudio() || stream.hasVideo() || stream.hasScreen()) {

                if (socket.room.p2p) {
                    var s = stream.getSocket();
                    sendToSocket(s, 'publish_me', {streamId: options.streamId,
                                                             peerSocket: socket.id});

                } else {
                    log.info('message: addSubscriber requested, ' +
                             'streamId: ' + options.streamId + ', ' +
                             'clientId: ' + socket.id);
                    socket.room.controller.addSubscriber(socket.id, options.streamId, options,
                                                         function (signMess) {
                        if (signMess.type === 'initializing') {
                            log.info('message: addSubscriber, ' +
                                     'state: SUBSCRIBER_INITIAL, ' +
                                     'clientId: ' + socket.id + ', ' +
                                     'streamId: ' + options.streamId);
                            callback(true);

                            if (GLOBAL.config.erizoController.report.session_events) {  // jshint ignore:line
                                var timeStamp = new Date();
                                amqper.broadcast('event', {room: socket.room.id,
                                                           user: socket.id,
                                                           name: socket.user.name,
                                                           type: 'subscribe',
                                                           stream: options.streamId,
                                                           timestamp: timeStamp.getTime()});
                            }
                            return;
                        } else if (signMess.type === 'failed'){
                            //TODO: Add Stats event
                            log.warn('message: addSubscriber ICE Failed, ' +
                                     'state: SUBSCRIBER_FAILED, ' +
                                     'streamId: ' + options.streamId + ', ' +
                                     'clientId: ' + socket.id);
                            socket.emit('connection_failed', {type: 'subscribe',
                                                              streamId: options.streamId});
                            return;
                        } else if (signMess.type === 'ready') {
                            log.info('message: addSubscriber, ' +
                                     'state: SUBSCRIBER_READY, ' +
                                     'streamId: ' + options.streamId + ', ' +
                                     'clientId: ' + socket.id);

                        } else if (signMess.type === 'bandwidthAlert') {
                            socket.emit('onBandwidthAlert', {streamID: options.streamId,
                                                             message: signMess.message,
                                                             bandwidth: signMess.bandwidth});
                        } else if (signMess === 'timeout') {
                            log.error('message: addSubscriber timeout when contacting ErizoJS, ' +
                                      'streamId: ', options.streamId, ', ' +
                                      'clientId: ' + socket.id);
                            callback(null, 'ErizoJS is not reachable');
                            return;
                        }

                        socket.emit('signaling_message_erizo', {mess: signMess,
                                                                peerId: options.streamId});
                    });
                }
            } else {
                callback(true);
            }

        });

        // Gets 'startRecorder' messages
        // Returns callback(id, error)
        socket.on('startRecorder', function (options, callback) {
            if (socket.user === undefined || !socket.user.permissions[Permission.RECORD]) {
                callback(null, 'Unauthorized');
                return;
            }
            var streamId = options.to;
            var recordingId = Math.random() * 1000000000000000000;
            var url;

            if (GLOBAL.config.erizoController.recording_path) {  // jshint ignore:line
                url = GLOBAL.config.erizoController.recording_path + recordingId + '.mkv';  // jshint ignore:line
            } else {
                url = '/tmp/' + recordingId + '.mkv';
            }

            log.info('message: startRecorder, ' +
                     'state: RECORD_REQUESTED, ' +
                     'streamId: ' + streamId + ', ' +
                     'url: ' + url);

            if (socket.room.p2p) {
               callback(null, 'Stream can not be recorded');
            }

            if (socket.room.streams[streamId].hasAudio() ||
                socket.room.streams[streamId].hasVideo() ||
                socket.room.streams[streamId].hasScreen()) {

                socket.room.controller.addExternalOutput(streamId, url, function (result) {
                    if (result === 'success') {
                        log.info('message: startRecorder, ' +
                                 'state: RECORD_STARTED, ' +
                                 'streamId: ' + streamId + ', ' +
                                 'url: ' + url);
                        callback(recordingId);
                    } else {
                        log.warn('message: startRecorder stream not found, ' +
                                 'state: RECORD_FAILED, ' +
                                 'streamId: ' + streamId + ', ' +
                                 'url: ' + url);
                        callback(null, 'Unable to subscribe to stream for recording, ' +
                                       'publisher not present');
                    }
                });

            } else {
                log.warn('message: startRecorder stream cannot be recorded, ' +
                         'state: RECORD_FAILED, ' +
                         'streamId: ' + streamId + ', ' +
                         'url: ' + url);
                callback(null, 'Stream can not be recorded');
            }
        });

        // Gets 'stopRecorder' messages
        // Returns callback(result, error)
        socket.on('stopRecorder', function (options, callback) {
            if (socket.user === undefined || !socket.user.permissions[Permission.RECORD]) {
                if (callback) callback(null, 'Unauthorized');
                return;
            }
            var recordingId = options.id;
            var url;

            if (GLOBAL.config.erizoController.recording_path) {  // jshint ignore:line
                url = GLOBAL.config.erizoController.recording_path + recordingId + '.mkv';  // jshint ignore:line
            } else {
                url = '/tmp/' + recordingId + '.mkv';
            }

            log.info('message: startRecorder, ' +
                     'state: RECORD_STOPPED, ' +
                     'streamId: ' + options.id + ', ' +
                     'url: ' + url);
            socket.room.controller.removeExternalOutput(url, callback);
        });

        //Gets 'unpublish' messages on the socket in order to remove a stream from the room.
        // Returns callback(result, error)
        socket.on('unpublish', function (streamId, callback) {
            if (socket.user === undefined || !socket.user.permissions[Permission.PUBLISH]) {
                if (callback) callback(null, 'Unauthorized');
                return;
            }

            // Stream has been already deleted or it does not exist
            if (socket.room.streams[streamId] === undefined) {
                return;
            }
            var index;

            sendMsgToRoom(socket.room, 'onRemoveStream', {id: streamId});

            if (socket.room.streams[streamId].hasAudio() ||
                socket.room.streams[streamId].hasVideo() ||
                socket.room.streams[streamId].hasScreen()) {

                socket.state = 'sleeping';
                if (!socket.room.p2p) {
                    socket.room.controller.removePublisher(streamId);
                    if (GLOBAL.config.erizoController.report.session_events) {  // jshint ignore:line
                        var timeStamp = new Date();
                        amqper.broadcast('event', {room: socket.room.id,
                                                   user: socket.id,
                                                   type: 'unpublish',
                                                   stream: streamId,
                                                   timestamp: timeStamp.getTime()});
                    }
                }
            }

            index = socket.streams.indexOf(streamId);
            if (index !== -1) {
                socket.streams.splice(index, 1);
            }
            if (socket.room.streams[streamId]) {
                delete socket.room.streams[streamId];
            }
            if (typeof callback === 'function') {
              callback(true);
            }
        });

        //Gets 'unsubscribe' messages on the socket in order to
        // remove a subscriber from a determined stream (to).
        // Returns callback(result, error)
        socket.on('unsubscribe', function (to, callback) {
            if (socket.user === undefined || !socket.user.permissions[Permission.SUBSCRIBE]) {
                if (callback) callback(null, 'Unauthorized');
                return;
            }
            if (socket.room.streams[to] === undefined) {
                return;
            }

            socket.room.streams[to].removeDataSubscriber(socket.id);

            if (socket.room.streams[to].hasAudio() ||
                socket.room.streams[to].hasVideo() ||
                socket.room.streams[to].hasScreen()) {

                if (!socket.room.p2p) {
                    socket.room.controller.removeSubscriber(socket.id, to);
                    if (GLOBAL.config.erizoController.report.session_events) {  // jshint ignore:line
                        var timeStamp = new Date();
                        amqper.broadcast('event', {room: socket.room.id,
                                                   user: socket.id,
                                                   type: 'unsubscribe',
                                                   stream: to,
                                                   timestamp: timeStamp.getTime()});
                    }
                }
            }
            if (typeof callback === 'function') {
              callback(true);
            }
        });

        //When a client leaves the room erizoController removes its streams from the room if exists.
        socket.on('disconnect', function () {
            var i, index, id;

            var timeStamp = new Date();

            log.info('message: Socket disconnect, clientId: ' + socket.id);

            for (i in socket.streams) {
                if (socket.streams.hasOwnProperty(i)) {
                    sendMsgToRoom(socket.room, 'onRemoveStream', {id: socket.streams[i]});
                }
            }

            if (socket.room !== undefined) {

                for (i in socket.room.streams) {
                    if (socket.room.streams.hasOwnProperty(i)) {
                        socket.room.streams[i].removeDataSubscriber(socket.id);
                    }
                }

                index = socket.room.sockets.indexOf(socket.id);
                if (index !== -1) {
                    socket.room.sockets.splice(index, 1);
                }

                if (socket.room.controller) {
                    socket.room.controller.removeSubscriptions(socket.id);
                }

                for (i in socket.streams) {
                    if (socket.streams.hasOwnProperty(i)) {
                        id = socket.streams[i];
                        if( socket.room.streams[id]) {
                            if (socket.room.streams[id].hasAudio() ||
                                socket.room.streams[id].hasVideo() ||
                                socket.room.streams[id].hasScreen()) {

                                if (!socket.room.p2p) {
                                    socket.room.controller.removePublisher(id);
                                    if (GLOBAL.config.erizoController.report.session_events) {  // jshint ignore:line

                                        amqper.broadcast('event', {room: socket.room.id,
                                                                   user: socket.id,
                                                                   type: 'unpublish',
                                                                   stream: id,
                                                                   timestamp: timeStamp.getTime()});
                                    }
                                }
                            }

                            delete socket.room.streams[id];
                        }
                    }
                }
            }

            if (socket.room !== undefined &&
                !socket.room.p2p &&
                GLOBAL.config.erizoController.report.session_events) {  // jshint ignore:line
                amqper.broadcast('event', {room: socket.room.id,
                                          user: socket.id,
                                          type: 'user_disconnection',
                                          timestamp: timeStamp.getTime()});
            }

            if (socket.room !== undefined && socket.room.sockets.length === 0) {
                log.debug('message: deleting empty room, roomId: ' + socket.room.id);
                delete rooms[socket.room.id];
                updateMyState();
            }
        });

        socket.on('getStreamStats', function (streamId, callback) {
            log.debug('Getting stats for streamId ' + streamId);
            if (socket.user === undefined || !socket.user.permissions[Permission.STATS]) {
                log.info('message: unauthorized getStreamStats request');
                if (callback) callback(null, 'Unauthorized');
                return;
            }
            if (socket.room.streams[streamId] === undefined) {
                log.info('message: bad getStreamStats request');
                return;
            }
            if (socket.room !== undefined && !socket.room.p2p) {
                socket.room.controller.getStreamStats(streamId, function (result) {
                    callback(result);
                });
            }
        });
    });

};


/*
 *Gets a list of users in a given room.
 */
exports.getUsersInRoom = function (room, callback) {
    var users = [], sockets, id;
    if (rooms[room] === undefined) {
        callback(users);
        return;
    }

    sockets = rooms[room].sockets;

    for (id in sockets) {
        if (sockets.hasOwnProperty(id)) {
            users.push(io.sockets.sockets[sockets[id]].user);
        }
    }

    callback(users);
};

/*
 *Remove user from a room.
 */
exports.deleteUser = function (user, room, callback) {
    var sockets, id;

    if (rooms[room] === undefined) {
       callback('Success');
       return;
    }
    sockets = rooms[room].sockets;
    var socketsToDelete = [];

    for (id in sockets) {
        if (sockets.hasOwnProperty(id)) {
            if (io.sockets.sockets[sockets[id]].user.name === user){
                socketsToDelete.push(sockets[id]);
            }
        }
    }

    for (var s in socketsToDelete) {

        log.info('message: deleteUser, user: ' + io.sockets.sockets[socketsToDelete[s]].user.name);
        io.sockets.sockets[socketsToDelete[s]].disconnect();
    }

    if (socketsToDelete.length !== 0) {
        callback('Success');
        return;
    }
    else {
        log.error('mesagge: deleteUser user does not exist, user: ' + user );
        callback('User does not exist');
        return;
    }


};


/*
 * Delete a room.
 */
exports.deleteRoom = function (room, callback) {
    var sockets, streams, id, j;

    log.info('message: deleteRoom, roomId: ' + room);

    if (rooms[room] === undefined) {
        callback('Success');
        return;
    }
    sockets = rooms[room].sockets;

    for (id in sockets) {
        if (sockets.hasOwnProperty(id)) {
            rooms[room].controller.removeSubscriptions(sockets[id]);
        }
    }

    streams = rooms[room].streams;

    for (j in streams) {
        if (streams[j].hasAudio() || streams[j].hasVideo() || streams[j].hasScreen()) {
            if (!room.p2p) {
                rooms[room].controller.removePublisher(j);
            }
        }
    }

    delete rooms[room];
    updateMyState();
    callback('Success');
};
amqper.connect(function () {
    try {
        amqper.setPublicRPC(rpcPublic);

        addToCloudHandler(function () {
            var rpcID = 'erizoController_' + myId;

            amqper.bind(rpcID, listen);

        });
    } catch (error) {
        log.info('message: Error in Erizo Controller, ' + logger.objectToLog(error));
    }
});
