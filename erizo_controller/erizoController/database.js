/*global require, exports*/
'use strict';
var config = require('config');

var nuveConfig = config.get('nuve');

var databaseUrl = nuveConfig.dataBaseURL;

var collections = ['erizoJS'];
var mongojs = require('mongojs');
exports.db = mongojs(databaseUrl, collections);
