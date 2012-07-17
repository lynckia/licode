var N = N || {};

N.API = (function (N) {
    "use strict";
    var createRoom, getRooms, getRoom, deleteRoom, createToken, createService, getServices, getService, deleteService, getUsers, getUser, deleteUser, params, send, calculateSignature, init;

    params = {
        url: undefined,
        service: undefined,
        key: undefined
    };

    init = function (url, service, key) {
        N.API.params.url = url;
        N.API.params.service = service;
        N.API.params.key = key;
    };

    createRoom = function (name, callback) {
        send(callback, 'POST', name, N.API.params.url + 'rooms');
    };

    getRooms = function (callback) {
        send(callback, 'GET', undefined, N.API.params.url + 'rooms');
    };

    getRoom = function (room, callback) {
        send(callback, 'GET', undefined, N.API.params.url + 'rooms/' + room);
    };

    deleteRoom = function (room, callback) {
        send(callback, 'DELETE', undefined, N.API.params.url + 'rooms/' + room);
    };

    createToken = function (room, username, role, callback) {
        send(callback, 'POST', undefined, N.API.params.url + 'rooms/' + room + "/tokens", username, role);
    };

    createService = function (name, key, callback) {
        send(callback, 'POST', {name: name, key: key}, N.API.params.url + 'services/');
    };

    getServices = function (callback) {
        send(callback, 'GET', undefined, N.API.params.url + 'services/');
    };

    getService = function (service, callback) {
        send(callback, 'GET', undefined, N.API.params.url + 'services/' + service);
    };

    deleteService = function (service, callback) {
        send(callback, 'DELETE', undefined, N.API.params.url + 'services/' + service);
    };

    getUsers = function (room, callback) {
        send(callback, 'GET', undefined, N.API.params.url + 'rooms/' + room + '/users/');
    };

    getUser = function (room, user, callback) {
        send(callback, 'GET', undefined, N.API.params.url + 'rooms/' + room + '/users/' + user);
    };

    deleteUser = function (room, user, callback) {
        send(callback, 'DELETE', undefined, N.API.params.url + 'rooms/' + room + '/users/' + user);
    };

    send = function (callback, method, body, url, username, role) {
        var service, key, timestamp, cnounce, toSign, header, signed, req;
        service = N.API.params.service;
        key = N.API.params.key;

        if (service === '' || key === '') {
            console.log('ServiceID and Key are required!!');
            return;
        }

        timestamp = new Date().getTime();
        cnounce = '123123aaff';

        toSign = timestamp + ',' + cnounce;

        header = 'MAuth realm=http://marte3.dit.upm.es,mauth_signature_method=HMAC_SHA1';

        if (username !== '' && role !== '') {

            header += ',mauth_username=';
            header +=  username;
            header += ',mauth_role=';
            header +=  role;

            toSign += ',' + username + ',' + role;
        }

        signed = calculateSignature(toSign, key);


        header += ',mauth_serviceid=';
        header +=  service;
        header += ',mauth_cnonce=';
        header += cnounce;
        header += ',mauth_timestamp=';
        header +=  timestamp;
        header += ',mauth_signature=';
        header +=  signed;

        req = new XMLHttpRequest();

        req.onreadystatechange = function () {
            if (req.readyState === 4) {
                callback(req.responseText);
            }
        };

        req.open(method, url, true);

        req.setRequestHeader('Authorization', header);
        req.setRequestHeader('Content-Type', 'application/json');
        console.log("Sending " + method + " to " + url);
        req.send(JSON.stringify(body));
    };

    calculateSignature = function (toSign, key) {
        var hash, hex, signed;
        hash = CryptoJS.HmacSHA1(toSign, key);
        hex = hash.toString(CryptoJS.enc.Hex);
        signed = N.Base64.encodeBase64(hex);
        return signed;
    };

    return {
        params: params,
        init: init,
        createRoom: createRoom,
        getRooms: getRooms,
        deleteRoom: deleteRoom,
        createToken: createToken,
        createService: createService,
        getServices: getServices,
        getService: getService,
        deleteService: deleteService,
        getUsers: getUsers,
        getUser: getUser,
        deleteUser: deleteUser
    };
}(N));