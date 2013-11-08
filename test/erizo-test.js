/*global describe, beforeEach, it, XMLHttpRequest, jasmine, expect, waitsFor, runs, Erizo*/
describe('server', function () {
    "use strict";
    var room, createToken, token, localStream, remoteStream;

    var TIMEOUT=10000,
        ROOM_NAME="myTestRoom",
        id;

    createToken = function (userName, role, callback, callbackError) {
        N.API.init(config.nuve.superserviceID, config.nuve.superserviceKey, 'http://localhost:3000/');
        N.API.createRoom(ROOM_NAME, function(room) {
            id = room._id;
            N.API.createToken(id, "user", "role", callback, callbackError);
        });
    };

    beforeEach(function () {
        L.Logger.setLogLevel(L.Logger.NONE);
    });

    it('should get token', function () {
        var callback = jasmine.createSpy("token");
        var received = false;
        var obtained = false;

        createToken("user", "presenter", function(_token) {
            callback();
            token = _token;
            obtained = true;
            received = true;
        }, function(error) {
            obtained = false;
            received = true;
        });

        waitsFor(function () {
            return received;
        }, "The token shoud have been creaded", TIMEOUT);

        runs(function () {
            expect(obtained).toBe(true);
        });
    });

    it('should get access to user media', function () {
        var callback = jasmine.createSpy("getusermedia");

        localStream = Erizo.Stream({audio: true, video: true, fake: true, data: true});

        localStream.addEventListener("access-accepted", callback);

        localStream.init();

        waitsFor(function () {
            return callback.callCount > 0;
        }, "User media should have been accepted", TIMEOUT);

        runs(function () {

            expect(callback).toHaveBeenCalled();
        });
    });

    it('should connect to room', function () {
        var callback = jasmine.createSpy("connectroom");

        room = Erizo.Room({token: token});

        room.addEventListener("room-connected", callback);

        room.connect();

        waitsFor(function () {
            return callback.callCount > 0;
        }, "Client should be connected to room", TIMEOUT);

        runs(function () {
            expect(callback).toHaveBeenCalled();
        });
    });

    it('should publish stream in room', function () {
        var callback = jasmine.createSpy("publishroom");

        room.addEventListener("stream-added", function(event) {
            if (localStream.getID() === event.stream.getID()) {
                callback();
            }
        });
        room.publish(localStream);
        waitsFor(function () {
            return callback.callCount > 0;
        }, "Stream should be published in room", TIMEOUT);

        runs(function () {
            expect(callback).toHaveBeenCalled();
        });
    });

    it('should subscribe to stream in room', function () {
        var callback = jasmine.createSpy("publishroom");

        room.addEventListener("stream-subscribed", function() {
            callback();
        });

        var remoteStream = room.remoteStreams;
        
        for (var index in remoteStream) {
            var stream = remoteStream[index];
            room.subscribe(stream);
        }
        waitsFor(function () {
            return callback.callCount > 0;
        }, "Stream should be subscribed to stream", 20000);

        runs(function () {
            expect(callback).toHaveBeenCalled();
        });
    });

    it('should allow showing a subscribed stream', function () {
        var divg = document.createElement("div");
        divg.setAttribute("id", "myDiv");
        document.body.appendChild(divg);

        localStream.show('myDiv');

        waits(500);

        runs(function () {
            var video = document.getElementById('stream' + localStream.getID());
            expect(video.getAttribute("url")).toBeDefined();
        });
    });

    it('should disconnect from room', function () {
        var callback = jasmine.createSpy("connectroom");

        room.addEventListener("room-disconnected", callback);

        room.disconnect();

        waitsFor(function () {
            return callback.callCount > 0;
        }, "Client should be disconnected from room", TIMEOUT);

        runs(function () {
            expect(callback).toHaveBeenCalled();
        });
    });

    it ('should delete room', function() {
        N.API.deleteRoom(id, function(result) {
            id = undefined;
        }, function(error) {

        });

        waitsFor(function () {
            return id === undefined;
        }, "Nuve should have created the room", TIMEOUT);

        runs(function () {
            expect(id).toBe(undefined);
        });
    })
});
