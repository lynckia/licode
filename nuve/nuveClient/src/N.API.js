/*global console, CryptoJS, XMLHttpRequest*/
var N = N || {};

N.API = (function (N) {
    "use strict";
    var createRoom, getRooms, getRoom, deleteRoom, createToken, createService, getServices, getService, deleteService, getUsers, getUser, deleteUser, params, send, calculateSignature, init;

    params = {
        service: undefined,
        key: undefined,
        url: undefined
    };

    init = function (service, key, url) {
        N.API.params.service = service;
        N.API.params.key = key;
        N.API.params.url = url;
    };

    createRoom = function (name, callback, options, params) {

        send(function (roomRtn) {
            var room = JSON.parse(roomRtn);
            callback(room);
        }, 'POST', {name: name, options: options}, 'rooms', params);
    };

    getRooms = function (callback, params) {
        send(callback, 'GET', undefined, 'rooms', params);
    };

    getRoom = function (room, callback, params) {
        send(callback, 'GET', undefined, 'rooms/' + room, params);
    };

    deleteRoom = function (room, callback, params) {
        send(callback, 'DELETE', undefined, 'rooms/' + room, params);
    };

    createToken = function (room, username, role, callback, params) {
        send(callback, 'POST', undefined, 'rooms/' + room + "/tokens", params, username, role);
    };

    createService = function (name, key, callback, params) {
        send(callback, 'POST', {name: name, key: key}, 'services/', params);
    };

    getServices = function (callback, params) {
        send(callback, 'GET', undefined, 'services/', params);
    };

    getService = function (service, callback, params) {
        send(callback, 'GET', undefined, 'services/' + service, params);
    };

    deleteService = function (service, callback, params) {
        send(callback, 'DELETE', undefined, 'services/' + service, params);
    };

    getUsers = function (room, callback, params) {
        send(callback, 'GET', undefined, 'rooms/' + room + '/users/', params);
    };

    getUser = function (room, user, callback, params) {
        send(callback, 'GET', undefined, 'rooms/' + room + '/users/' + user, params);
    };

    deleteUser = function (room, user, callback, params) {
        send(callback, 'DELETE', undefined, 'rooms/' + room + '/users/' + user);
    };

    send = function (callback, method, body, url, params, username, role) {
        var service, key, timestamp, cnounce, toSign, header, signed, req;

        if (params === undefined) {
            service = N.API.params.service;
            key = N.API.params.key;
            url = N.API.params.url + url;
        } else {
            service = params.service;
            key = params.key;
            url = params.url + url;
        }

        if (service === '' || key === '') {
            console.log('ServiceID and Key are required!!');
            return;
        }

        timestamp = new Date().getTime();
        cnounce = Math.floor(Math.random() * 99999);

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

        if (body !== undefined) {
            req.setRequestHeader('Content-Type', 'application/json');
            req.send(JSON.stringify(body));
        } else {
            req.send();
        }

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
        getRoom: getRoom,
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
