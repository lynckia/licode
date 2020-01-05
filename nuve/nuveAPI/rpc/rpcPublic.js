/* global exports, require */

/* eslint-disable no-param-reassign */

const tokenRegistry = require('./../mdb/tokenRegistry');
const serviceRegistry = require('./../mdb/serviceRegistry');
const cloudHandler = require('./../cloudHandler');
const logger = require('../logger').logger;

// Logger
const log = logger.getLogger('RPCPublic');

/*
 * This function is used to consume a token. Removes it from the data base
 * and returns to erizoController. Also it removes old tokens.
 */
exports.deleteToken = (id, callback) => {
  tokenRegistry.removeOldTokens();

  tokenRegistry.getToken(id, (token) => {
    if (token === undefined) {
      callback('callback', 'error');
    } else if (token.use !== undefined) {
      let s;
      // Is a test token
      if (token.use > 490) {
        s = token.service;
        serviceRegistry.getService(s, (service) => {
          delete service.testToken;
          serviceRegistry.updateService(service);
          tokenRegistry.removeToken(id, () => {
            log.info(`message : delete expired TestToken, tokenId: ${token._id}, ` +
            `roomId: ${token.room}`, `, serviceId: ${token.service}`);
            callback('callback', 'error');
          });
        });
      } else {
        token.use += 1;
        tokenRegistry.updateToken(token);
        log.info(`message: using testToken, tokenUse: ${token.use}, testRoom: ` +
          `${token.room}`, `, serviceId: ${token.service}`);
        callback('callback', token);
      }
    } else {
      tokenRegistry.removeToken(id, () => {
        log.info(`message: consumed token, tokenId: ${token._id}, ` +
                             `roomId: ${token.room}, serviceId: ${token.service}`);
        callback('callback', token);
      });
    }
  });
};

exports.addNewErizoController = (msg, callback) => {
  cloudHandler.addNewErizoController(msg, (id) => {
    callback('callback', id);
  });
};

exports.getErizoControllers = (msg, callback) => {
  cloudHandler.getEcQueue((ecQueue) => {
    callback('callback', ecQueue);
  });
};

exports.keepAlive = (id, callback) => {
  cloudHandler.keepAlive(id, (result) => {
    callback('callback', result);
  });
};

exports.setInfo = (params, callback) => {
  cloudHandler.setInfo(params);
  callback('callback');
};

exports.killMe = (ip, callback) => {
  cloudHandler.killMe(ip);
  callback('callback');
};
