var log4js = require('log4js');
var config = require('./../../licode_config');

var logFile = config.logger.configFile ||  '../log4js_configuration.json';

log4js.configure(logFile);

exports.logger = log4js;
