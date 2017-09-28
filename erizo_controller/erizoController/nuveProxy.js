/*global require, exports*/
'use strict';
var logger = require('./../common/logger').logger;

// Logger
var log = logger.getLogger('NuveProxy');

exports.NuveProxy = function (spec) {
  var that = {};

  var callNuve = (method, additionalErrors, args) => new Promise((resolve, reject) => {
    try {
      spec.amqper.callRpc('nuve', method, args, {callback: function (response) {
        const errors = ['timeout', 'error'].concat(additionalErrors);
        if (errors.indexOf(response) !== -1) {
          reject(response);
          return;
        }
        resolve(response);
      }});
    } catch(err) {
      log.error('message: Error calling RPC', err);
    }
  });

  that.killMe = (publicIP) => callNuve('killMe', [], publicIP);
  that.keepAlive = (myId) => callNuve('keepAlive', ['whoareyou'], myId);
  that.addNewErizoController = (controller) => callNuve('addNewErizoController', [], controller);
  that.setInfo = (info) => callNuve('setInfo', [], info);
  that.deleteToken = (token) => callNuve('deleteToken', [], token);

  return that;
};
