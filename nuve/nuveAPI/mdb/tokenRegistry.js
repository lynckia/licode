/*global require, exports*/
'use strict';
var db = require('./dataBase').db;

var logger = require('./../logger').logger;

// Logger
var log = logger.getLogger('TokenRegistry');

/*
 * Gets a list of the tokens in the data base.
 */
exports.getList = function (callback) {
    db.tokens.find({}).toArray(function (err, tokens) {
        if (err || !tokens) {
            log.info('message: token getList empty');
        } else {
            callback(tokens);
        }
    });
};

var getToken = exports.getToken = function (id, callback) {
    db.tokens.findOne({_id: db.ObjectId(id)}, function (err, token) {
        if (token == null) {
            token = undefined;
            log.info('message: getToken token not found, tokenId: ' + id);
        }
        if (callback !== undefined) {
            callback(token);
        }
    });
};

var hasToken = exports.hasToken = function (id, callback) {
    getToken(id, function (token) {
        if (token === undefined) {
            callback(false);
        } else {
            callback(true);
        }
    });

};

/*
 * Adds a new token to the data base.
 */
exports.addToken = function (token, callback) {
    db.tokens.save(token, function (error, saved) {
        if (error) log.warn('message: addToken error, ' + logger.objectToLog(error));
        callback(saved._id);
    });
};

/*
 * Removes a token from the data base.
 */
var removeToken = exports.removeToken = function (id, callback) {
    hasToken(id, function (hasT) {
        if (hasT) {
            db.tokens.remove({_id: db.ObjectId(id)}, function (error) {
                if (error) {
                    log.warn('message: removeToken error, ' +
                        logger.objectToLog(error));
                }
                callback();
            });

        }
    });
};

/*
 * Updates a determined token in the data base.
 */
exports.updateToken = function (token) {
    db.tokens.save(token, function (error) {
        if (error) log.warn('message: updateToken error, ' + logger.objectToLog(error));
    });
};

exports.removeOldTokens = function () {
    var i, token, time, tokenTime, dif;

    db.tokens.find({'use':{$exists:false}}).toArray(function (err, tokens) {
        if (err || !tokens) {

        } else {
            for (i in tokens) {
                token = tokens[i];
                time = (new Date()).getTime();
                tokenTime = token.creationDate.getTime();
                dif = time - tokenTime;
                if (dif > 3*60*1000) {

                    log.info('message: removing old token, tokenId: ' + token._id + ', roomId: ' +
                        token.room + ', serviceId: ' + token.service);
                    removeToken(token._id + '', function() {});
                }
            }
        }
    });
};
