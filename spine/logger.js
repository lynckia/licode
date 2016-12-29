var log4js = require('log4js');
var logFile = './log4js_configuration.json';

log4js.configure(logFile);

exports.logger = log4js;
