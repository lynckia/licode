var log4js = require('log4js');
var config = require('config');

var logFile = config.get('logger.configFile');

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


log4js.configure(logFile);

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
