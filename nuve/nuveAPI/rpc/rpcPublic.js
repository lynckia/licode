/*global exports, require*/
'use strict';
var tokenRegistry = require('./../mdb/tokenRegistry');
var serviceRegistry = require('./../mdb/serviceRegistry');
var cloudHandler = require('./../cloudHandler');
var logger = require('../logger').logger;

// Logger
var log = logger.getLogger('RPCPublic');

/*
 * This function is used to consume a token. Removes it from the data base
 * and returns to erizoController. Also it removes old tokens.
 */
exports.deleteToken = function (id, callback) {
    tokenRegistry.removeOldTokens();

    tokenRegistry.getToken(id, function (token) {

        if (token === undefined) {
            callback('callback', 'error');
        } else {

            if (token.use !== undefined) {
                var s;
                //Is a test token
                if (token.use > 490) {
                    s = token.service;
                    serviceRegistry.getService(s, function (service) {
                        delete service.testToken;
                        serviceRegistry.updateService(service);
                        tokenRegistry.removeToken(id, function () {
                            log.info('message : delete expired TestToken, tokenId: ' + token._id +
                                     ', roomId: ' + token.room, ', serviceId: ' + token.service);
                            callback('callback', 'error');
                        });

                    });
                } else {
                    token.use += 1;
                    tokenRegistry.updateToken(token);
                    log.info ('message: using testToken, tokenUse: ' + token.use + ', testRoom: ' + 
                        token.room, ', serviceId: ' + token.service);
                    callback('callback', token);
                }

            } else {
                tokenRegistry.removeToken(id, function () {
                    log.info('message: consumed token, tokenId: ', token._id,
                             ', roomId: ' + token.room + ', serviceId: ' + token.service);
                    callback('callback', token);
                });
            }
        }
    });
};

exports.addNewErizoController = function(msg, callback) {
    cloudHandler.addNewErizoController(msg, function (id) {
        callback('callback', id);
    });
};

exports.keepAlive = function(id, callback) {
    cloudHandler.keepAlive(id, function(result) {
        callback('callback', result);
    });
};

exports.setInfo = function(params, callback) {
    cloudHandler.setInfo(params);
    callback('callback');
};

exports.killMe = function(ip, callback) {
    cloudHandler.killMe(ip);
    callback('callback');
};
