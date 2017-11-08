const log4js = require('log4js'); // eslint-disable-line import/no-extraneous-dependencies

const logFile = './log4js_configuration.json';

log4js.configure(logFile);

exports.logger = log4js;
