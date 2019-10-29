/* global console, XMLHttpRequest */

/* eslint-disable */

var crypto = require('crypto');

var N = N || {};

N.API = (function (N) {
    'use strict';
    var createRoom, getRooms, getRoom, updateRoom, patchRoom,
        deleteRoom, createToken, createService, getServices,
        getService, deleteService, getUsers, getUser, deleteUser,
        params, send, calculateSignature, formatString, init;

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

    createRoom = function (name, callback, callbackError, options, params) {

        if (!options) {
            options = {};
        }

        send(function (roomRtn) {
            var room = JSON.parse(roomRtn);
            callback(room);
        }, callbackError, 'POST', {name: name, options: options}, 'rooms', params);
    };

    getRooms = function (callback, callbackError, params) {
        send(callback, callbackError, 'GET', undefined, 'rooms', params);
    };

    getRoom = function (room, callback, callbackError, params) {
        send(callback, callbackError, 'GET', undefined, 'rooms/' + room, params);
    };

    updateRoom = function (room, name, callback, callbackError, options, params) {
        send(callback, callbackError, 'PUT', {name: name, options: options},
             'rooms/' + room, params);
    };

    patchRoom = function (room, name, callback, callbackError, options, params) {
        send(callback, callbackError, 'PATCH', {name: name, options: options},
             'rooms/' + room, params);
    };

    deleteRoom = function (room, callback, callbackError, params) {
        send(callback, callbackError, 'DELETE', undefined, 'rooms/' + room, params);
    };

    createToken = function (room, username, role, callback, callbackError, params) {
        send(callback, callbackError, 'POST', undefined, 'rooms/' + room + '/tokens',
             params, username, role);
    };

    createService = function (name, key, callback, callbackError, params) {
        send(callback, callbackError, 'POST', {name: name, key: key}, 'services/', params);
    };

    getServices = function (callback, callbackError, params) {
        send(callback, callbackError, 'GET', undefined, 'services/', params);
    };

    getService = function (service, callback, callbackError, params) {
        send(callback, callbackError, 'GET', undefined, 'services/' + service, params);
    };

    deleteService = function (service, callback, callbackError, params) {
        send(callback, callbackError, 'DELETE', undefined, 'services/' + service, params);
    };

    getUsers = function (room, callback, callbackError, params) {
        send(callback, callbackError, 'GET', undefined, 'rooms/' + room + '/users/', params);
    };

    getUser = function (room, user, callback, callbackError, params) {
        send(callback, callbackError, 'GET', undefined, 'rooms/' + room + '/users/' + user, params);
    };

    deleteUser = function (room, user, callback, callbackError, params) {
        send(callback, callbackError, 'DELETE', undefined,
             'rooms/' + room + '/users/' + user, params);
    };

    send = function (callback, callbackError, method, body, url, params, username, role) {
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

        if (username && role) {

            username = formatString(username);

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
                switch (req.status) {
                    case 100:
                    case 200:
                    case 201:
                    case 202:
                    case 203:
                    case 204:
                    case 205:
                        callback(req.responseText);
                        break;
                    default:
                        if (callbackError !== undefined) {
                          callbackError(req.status + ' Error' + req.responseText, req.status);
                        }
                }
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
      var hex = crypto.createHmac('sha1', key)
        .update(toSign)
        .digest('hex');
      return Buffer.from(hex).toString('base64');
    };

    formatString = function(s){
        var r = s.toLowerCase();
        var nonAsciis = {'a': '[àáâãäå]',
                         'ae': 'æ',
                         'c': 'ç',
                         'e': '[èéêë]',
                         'i': '[ìíîï]',
                         'n': 'ñ',
                         'o': '[òóôõö]',
                         'oe': 'œ',
                         'u': '[ùúûűü]',
                         'y': '[ýÿ]'};
        for (var i in nonAsciis) {
          r = r.replace(new RegExp(nonAsciis[i], 'g'), i);
        }
        return r;
    };

    return {
        params: params,
        init: init,
        createRoom: createRoom,
        getRooms: getRooms,
        getRoom: getRoom,
        updateRoom: updateRoom,
        patchRoom: patchRoom,
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
