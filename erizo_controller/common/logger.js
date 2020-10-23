
// eslint-disable-next-line import/no-extraneous-dependencies
const log4js = require('log4js');

global.config = global.config || {};

global.config.logger = global.config.logger || {};

const logFile = global.config.logger.configFile || '../log4js_configuration.json';


const logJsonReplacer = (key, value) => {
  if (key) {
    if (typeof (value) === 'object') {
      const log = exports.logger.objectToLog(value);
      if (log.length === 0) {
        return '{},';
      }
    }
    return value;
  }
  return value;
};

log4js.configure(logFile);

exports.logger = log4js;

exports.logger.objectToLog = (jsonInput) => {
  if (jsonInput === undefined || jsonInput === null) {
    return '';
  }
  if (typeof (jsonInput) !== 'object') {
    return jsonInput;
  } else if (jsonInput.constructor === Array) {
    return '[Object]';
  }
  const jsonString = JSON.stringify(jsonInput, logJsonReplacer);
  return jsonString.replace(/['"]+/g, '')
    .replace(/[:]+/g, ': ')
    .replace(/[,]+/g, ', ')
    .slice(1, -1);
};
