var log4js = require('log4js'); 
var config = require('./../../licode_config');

var log_file = config.logger.config_file ||  "../log4js_configuration.json";

log4js.configure(log_file);

exports.logger = log4js;