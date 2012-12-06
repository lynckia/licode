/*global describe, beforeEach, it, XMLHttpRequest, jasmine, expect, waitsFor*/
describe('server', function () {
    "use strict";
    var room, createToken, token, localStream;

    createToken = function (userName, role, callback) {

        var req = new XMLHttpRequest(),
            serverUrl = "http://localhost:3001/",
            url = serverUrl + 'createToken/',
            body = {username: userName, role: role};

        req.onreadystatechange = function () {
            if (req.readyState === 4) {
                callback(req.responseText);
            }
        };

        req.open('POST', url, true);
        req.setRequestHeader('Content-Type', 'application/json');
        req.send(JSON.stringify(body));
    };

    beforeEach(function () {

    });

    it('should get token', function () {
        var callback = jasmine.createSpy("token");

        createToken("user", "role", callback);

        waitsFor(function () {
            return callback.callCount > 0;
        });

        runs(function(){
            expect(callback).toHaveBeenCalled();
            token = callback.mostRecentCall.args;
        });
    });

    it('should get access to user media', function () {
        var callback = jasmine.createSpy("getusermedia");

        localStream = Erizo.Stream({audio: true, video: true, data: true});

        localStream.addEventListener("access-accepted", callback);

        localStream.init();

        waitsFor(function () {
            return callback.callCount > 0;
        });

        runs(function(){
            expect(callback).toHaveBeenCalled();
        });
    });
});