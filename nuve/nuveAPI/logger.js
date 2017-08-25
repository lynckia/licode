var log4js = require('log4js');
var config = require('config');
var os = require('os');

var logFile = config.get('logger.configFile');
var loggerConfig = require(logFile);

var logJsonReplacer = function (key, value){
    'use strict';
    if (key) {
        if (typeof(value) === 'object'){
            return '[Object]';
        }
        return value;
    }else{
        return value;
    }
};

var logstashConfig = config.get('logstash');

loggerConfig.appenders.push({
    type: 'log4js-logstash',
    host: logstashConfig.host,
    port: logstashConfig.port,
    fields: {
        app: logstashConfig.appName,
        instance: `${logstashConfig.appName}-${process.pid}`,
        hostname: os.hostname(),
        environment: process.env.NODE_ENV
    },
});

log4js.configure(loggerConfig);

exports.logger = log4js;

exports.logger.objectToLog = function (jsonInput) {
    'use strict';
    if (jsonInput === undefined){
        return '';
    }
    if (typeof(jsonInput) !== 'object') {
        return jsonInput;
    } else if (jsonInput.constructor === Array) {
        return '[Object]';
    }
    var jsonString = JSON.stringify (jsonInput, logJsonReplacer);
    return jsonString.replace (/['"]+/g, '')
        .replace(/[:]+/g, ': ')
        .replace(/[,]+/g, ', ')
        .slice(1,-1);
};
