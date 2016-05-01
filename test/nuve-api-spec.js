var assert = require('chai').assert;
var N = require('../nuve/nuveClient/dist/nuve'),
	config = require('../licode_config');

 var TIMEOUT=10000,
     ROOM_NAME="myTestRoom";

var id;

describe("nuve", function () {
    this.timeout (TIMEOUT);

    beforeEach(function () {
        N.API.init(config.nuve.superserviceID, config.nuve.superserviceKey, 'http://localhost:3000/');
    });

    it("should not list rooms with wrong credentials", function () {
        var rooms;
        var received = false, ok = false;

        N.API.init("Jose Antonio", config.nuve.superserviceKey, 'http://localhost:3000/');

        N.API.getRooms(function(rooms_) {
            ok = true;
            throw "Succeeded and it should fail with bad credentials";
        }, function(error) {
            ok = false;
            done();
        });

    });

    it("should list rooms", function () {
        var rooms;
        var received = false, ok = false;

        N.API.getRooms(function(rooms_) {
            assert.isArray(rooms_, "This should be an array of rooms");
            done();
        }, function(error) {
            throw error;
        });
    });

    it("should create normal rooms", function () {
        N.API.createRoom(ROOM_NAME, function(room) {
            done();
        });
    });

    it("should get normal rooms", function () {
        N.API.getRoom(id, function(result) {
            done();
        }, function(error) {
            throw error;
        });
    });

    it("should create tokens for users in normal rooms", function () {
        N.API.createToken(id, "user", "presenter", function(token) {
            done();
        }, function(error) {
            throw error;
        });
    });

    it("should create tokens for users with special characters in normal rooms", function () {
        N.API.createToken(id, "user_with_$?üóñ", "role", function(token) {
            done()
        }, function(error) {
            throw error;
        });
    });

    it("should get users in normal rooms", function () {
        N.API.getUsers(id, function(token) {
            done()
        }, function(error) {
            throw error;
        });
    });

    it("should delete normal rooms", function () {
        N.API.deleteRoom(id, function(result) {
            done;
        }, function(error) {
            throw error;
        });

    });

    it("should create p2p rooms", function () {
        N.API.createRoom(ROOM_NAME, function(room) {
            id = room._id;
            assert.notEqual(id, undefined, 'Did not provide a valid id');
            done()
        }, function() {}, {p2p: true});
    });

    it("should get p2p rooms", function () {
        N.API.getRoom(id, function(result) {
            done()
        }, function(error) {
            throw error;
        });
    });

    it("should delete p2p rooms", function () {
        N.API.deleteRoom(id, function(result) {
            done()
        }, function(error) {
            throw error;
        });
    });
});
