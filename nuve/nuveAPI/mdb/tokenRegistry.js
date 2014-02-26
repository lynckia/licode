/*global require, exports, console*/
var db = require('./dataBase').db;

/*
 * Gets a list of the tokens in the data base.
 */
var getList = exports.getList = function (callback) {
    "use strict";

    db.tokens.find({}).toArray(function (err, tokens) {
        if (err || !tokens) {
            console.log('Empty list');
        } else {
            callback(tokens);
        }
    });
};

var getToken = exports.getToken = function (id, callback) {
    "use strict";

    db.tokens.findOne({_id: db.ObjectId(id)}, function (err, token) {
        if (token == null) {
            token = undefined;
            console.log('Token ', id, ' not found');
        }
        if (callback !== undefined) {
            callback(token);
        }
    });
};

var hasToken = exports.hasToken = function (id, callback) {
    "use strict";

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
    "use strict";

    db.tokens.save(token, function (error, saved) {
        if (error) console.log('MongoDB: Error adding token: ', error);
        callback(saved._id);
    });
};

/*
 * Removes a token from the data base.
 */
var removeToken = exports.removeToken = function (id, callback) {
    "use strict";

    hasToken(id, function (hasT) {
        if (hasT) {
            db.tokens.remove({_id: db.ObjectId(id)}, function (error, removed) {
                if (error) console.log('MongoDB: Error removing token: ', error);
                callback();
            });
            
        }
    });
};

/*
 * Updates a determined token in the data base.
 */
exports.updateToken = function (token) {
    "use strict";

    db.tokens.save(token, function (error, saved) {
        if (error) console.log('MongoDB: Error updating token: ', error);
    });
};

exports.removeOldTokens = function () {
    "use strict";

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
                    console.log('Removing old token ', token._id, 'from room ', token.room, ' of service ', token.service);
                    removeToken(token._id + '', function() {});
                }
            }
        }
    });
};
