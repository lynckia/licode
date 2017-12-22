/*global require, exports*/
'use strict';
var logger = require('./../../common/logger').logger;

// Logger
var log = logger.getLogger('Helpers');

exports.getMediaConfiguration = (mediaConfiguration = 'default') => {
  if (global.mediaConfig && global.mediaConfig.codecConfigurations) {
    if (global.mediaConfig.codecConfigurations[mediaConfiguration]) {
      return JSON.stringify(global.mediaConfig.codecConfigurations[mediaConfiguration]);
    } else if (global.mediaConfig.codecConfigurations.default) {
      return JSON.stringify(global.mediaConfig.codecConfigurations.default);
    } else {
      log.warn('message: Bad media config file. You need to specify a default codecConfiguration.');
      return JSON.stringify({});
    }
  } else {
    log.warn('message: Bad media config file. You need to specify a default codecConfiguration.');
    return JSON.stringify({});
  }
};
