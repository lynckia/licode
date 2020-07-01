/* global require, exports */

/* eslint-disable no-param-reassign */


const db = require('./dataBase').db;

const logger = require('./../logger').logger;

// Logger
const log = logger.getLogger('TokenRegistry');

/*
 * Gets a list of the tokens in the data base.
 */
exports.getList = (callback) => {
  db.tokens.find({}).toArray((err, tokens) => {
    if (err || !tokens) {
      log.info('message: token getList empty');
    } else {
      callback(tokens);
    }
  });
};

const getToken = (id, callback) => {
  db.tokens.findOne({ _id: db.ObjectId(id) }, (err, token) => {
    if (token == null) {
      token = undefined;
      log.info(`message: getToken token not found, tokenId: ${id}`);
    }
    if (callback !== undefined) {
      callback(token);
    }
  });
};

exports.getToken = getToken;

const hasToken = (id, callback) => {
  getToken(id, (token) => {
    if (token === undefined) {
      callback(false);
    } else {
      callback(true);
    }
  });
};

exports.hasToken = hasToken;

/*
 * Adds a new token to the data base.
 */
exports.addToken = (token, callback) => {
  db.tokens.save(token, (error, saved) => {
    if (error) {
      log.warn('message: addToken error,', logger.objectToLog(error));
      return callback(null, true);
    }
    return callback(saved._id, false);
  });
};

/*
 * Removes a token from the data base.
 */
const removeToken = (id, callback) => {
  hasToken(id, (hasT) => {
    if (hasT) {
      db.tokens.remove({ _id: db.ObjectId(id) }, (error) => {
        if (error) {
          log.warn('message: removeToken error,', logger.objectToLog(error));
        }
        callback();
      });
    }
  });
};

exports.removeToken = removeToken;

/*
 * Updates a determined token in the data base.
 */
exports.updateToken = (token) => {
  db.tokens.save(token, (error) => {
    if (error) log.warn('message: updateToken error,', logger.objectToLog(error));
  });
};

exports.removeOldTokens = () => {
  let time;
  let tokenTime;
  let dif;

  db.tokens.find({ use: { $exists: false } }).toArray((err, tokens) => {
    if (err || !tokens) {
      log.warn('message: error removingOldTokens or no tokens present');
    } else {
      tokens.forEach((tokenToRemove) => {
        time = (new Date()).getTime();
        tokenTime = tokenToRemove.creationDate.getTime();
        dif = time - tokenTime;
        if (dif > 3 * 60 * 1000) {
          log.info(`message: removing old token, tokenId: ${tokenToRemove._id}, ` +
            `roomId: ${tokenToRemove.room}, serviceId: ${tokenToRemove.service}`);
          removeToken(`${tokenToRemove._id}`, () => {});
        }
      });
    }
  });
};
