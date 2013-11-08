var N = require('../nuve/nuveClient/dist/nuve'),
	config = require('../licode_config');

 var TIMEOUT=10000,
     ROOM_NAME="myTestRoom";

var id;

describe("nuve", function () {

    beforeEach(function () {
        N.API.init(config.nuve.superserviceID, config.nuve.superserviceKey, 'http://localhost:3000/');
    });

    it("should not list rooms with wrong credentials", function () {
        var rooms;
        var received = false, ok = false;

        N.API.init("Jose Antonio", config.nuve.superserviceKey, 'http://localhost:3000/');

        N.API.getRooms(function(rooms_) {
            received = true;
            ok = true;
        }, function(error) {
        	received = true;
            ok = false;
        });

        waitsFor(function () {
        	return received;
        }, "Nuve should have answered", TIMEOUT);

        runs(function () {
            expect(ok).toBe(false);
        });
    });

    it("should list rooms", function () {
        var rooms;
        var received = false, ok = false;

        N.API.getRooms(function(rooms_) {
            received = true;
            ok = true;
        }, function(error) {
        	received = true;
            ok = false;
        });

        waitsFor(function () {
        	return received;
        }, "Nuve should have answered", TIMEOUT);

        runs(function () {
            expect(ok).toBe(true);
        });
    });

    it("should create normal rooms", function () {
        N.API.createRoom(ROOM_NAME, function(room) {
            id = room._id;
        });

        waitsFor(function () {
            return id !== undefined;
        }, "Nuve should have created the room", TIMEOUT);

        runs(function () {
            expect(id).toNotBe(undefined);
        });
    });

    it("should get normal rooms", function () {
    	var obtained = false;
    	var received = false;
        N.API.getRoom(id, function(result) {
            obtained = true;
            received = true;
        }, function(error) {
        	obtained = false;
        	received = true;
        });

        waitsFor(function () {
            return received;
        }, "Nuve should have created the room", TIMEOUT);

        runs(function () {
            expect(obtained).toBe(true);
        });
    });

    it("should create tokens for users in normal rooms", function () {
    	var obtained = false;
    	var received = false;
        N.API.createToken(id, "user", "presenter", function(token) {
            obtained = true;
            received = true;
        }, function(error) {
        	obtained = false;
        	received = true;
        });

        waitsFor(function () {
            return received;
        }, "Nuve should have created the room", TIMEOUT);

        runs(function () {
            expect(obtained).toBe(true);
        });
    });

    it("should create tokens for users with special characters in normal rooms", function () {
    	var obtained = false;
    	var received = false;
        N.API.createToken(id, "user_with_$?üóñ", "role", function(token) {
            obtained = true;
            received = true;
        }, function(error) {
        	obtained = false;
        	received = true;
        });

        waitsFor(function () {
            return received;
        }, "Nuve should have created the room", TIMEOUT);

        runs(function () {
            expect(obtained).toBe(true);
        });
    });

    it("should get users in normal rooms", function () {
    	var obtained = false;
    	var received = false;
        N.API.getUsers(id, function(token) {
            obtained = true;
            received = true;
        }, function(error) {
        	obtained = false;
        	received = true;
        });

        waitsFor(function () {
            return received;
        }, "Nuve should have created the room", TIMEOUT);

        runs(function () {
            expect(obtained).toBe(true);
        });
    });

    it("should delete normal rooms", function () {
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
    });

    it("should create p2p rooms", function () {
        N.API.createRoom(ROOM_NAME, function(room) {
            id = room._id;
        }, function() {}, {p2p: true});

        waitsFor(function () {
            return id !== undefined;
        }, "Nuve should have created the room", TIMEOUT);

        runs(function () {
            expect(id).toNotBe(undefined);
        });
    });

    it("should get p2p rooms", function () {
    	var obtained = false;
    	var received = false;
        N.API.getRoom(id, function(result) {
            obtained = true;
            received = true;
        }, function(error) {
        	obtained = false;
        	received = true;
        });

        waitsFor(function () {
            return received;
        }, "Nuve should have created the room", TIMEOUT);

        runs(function () {
            expect(obtained).toBe(true);
        });
    });

    it("should delete p2p rooms", function () {
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
    });
});
