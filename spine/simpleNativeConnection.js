'use strict';
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';  // We need this for testing with self signed certs

var XMLHttpRequest = require('xmlhttprequest').XMLHttpRequest,
    nodeUrl = require ('url'),
    Erizo = require('./erizofc').Erizo,
    NativeStack = require ('./NativeStack.js');

Erizo.Connection = NativeStack.FakeConnection;
var logger = require('./logger').logger;
var log = logger.getLogger('ErizoSimpleNativeConnection');


exports.ErizoSimpleNativeConnection = function (spec, callback, error){
    var that = {};

    var localStream = {};
    var room = '';
    that.isActive = false;
    localStream.getID = function() {return 0;};
    var createToken = function(userName, role, callback) {

        var req = new XMLHttpRequest();
        var theUrl = nodeUrl.parse(spec.serverUrl, true);
        if (theUrl.protocol !== 'https')
            log.warn('Protocol is not https');

        if (theUrl.query.room){
            room = theUrl.query.room;
            log.info('will Connect to Room', room);
        }

        theUrl.pathname = theUrl.pathname + 'createToken/';

        var url = nodeUrl.format(theUrl);
        log.info('Url to createToken', url);
        var body = {username: userName, role: role};

        req.onreadystatechange = function () {
            if (req.readyState === 4) {
                if (req.status===200){
                    callback(req.responseText);
                }else{
                    log.error('Could not get token');
                    callback('error');
                }
            }
        };

        req.open('POST', url, true);
        req.setRequestHeader('Content-Type', 'application/json');
        req.send(JSON.stringify(body));
    };

    var subscribeToStreams = function(streams){
        for (var index in streams){
            if (spec.subscribeConfig){
                log.info('Should subscribe', spec.subscribeConfig);
                room.subscribe(streams[index], spec.subscribeConfig);
            }
        }
    };

    var createConnection = function (){

        createToken('name', 'presenter', function (token) {
            log.info('Getting token', token);
            if (token === 'error'){
                error('Could not get token from nuve in ' + spec.serverUrl);
                return;
            }
            room = Erizo.Room({token: token});

            room.addEventListener('room-connected', function(roomEvent) {
                log.info('Connected to room');
                callback('room-connected');
                that.isActive = true;
                subscribeToStreams(roomEvent.streams);
                if (spec.publishConfig){
                    log.info('Will publish with config', spec.publishConfig);
                    localStream = Erizo.Stream(spec.publishConfig);
                    room.publish(localStream, spec.publishConfig, function(id, message){
                        if (id === undefined){
                            log.error('ERROR when publishing', message);
                            error(message);
                        }
                    });
                }
            });

            room.addEventListener('stream-added', function(roomEvent) {
                log.info('stream added', roomEvent.stream.getID());
                callback('stream-added');
                if (roomEvent.stream.getID()!==localStream.getID()){
                    var streams = [];
                    streams.push(roomEvent.stream);
                    subscribeToStreams(streams);
                }

            });

            room.addEventListener('stream-removed', function(roomEvent) {
                callback('stream-removed');
                if (roomEvent.stream.getID()!==localStream.getID()){
                    room.unsubscribe(roomEvent.stream);
                    log.info('stream removed', roomEvent.stream.getID());
                }

            });

            room.addEventListener('stream-subscribed', function() {
                log.info('stream-subscribed');
                callback('stream-subscribed');
            });

            room.addEventListener('room-error', function(roomEvent) {
                log.error('Room Error', roomEvent.type, roomEvent.message);
                error(roomEvent.message);
            });

            room.addEventListener('room-disconnected', function(roomEvent) {
                log.info('Room Disconnected', roomEvent.type, roomEvent.message);
                callback(roomEvent.type+roomEvent.message);
            });

            room.connect();
        });

        that.sendData = function(){
            localStream.sendData({msg: 'Testing Data Connection'});
        };
    };

    that.close = function(){
        log.info('Close');
        room.disconnect();
    };

    createConnection();
    return that;
};
