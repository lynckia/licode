/*global require, exports, console, setInterval, clearInterval*/

var logger = require('./../common/logger').logger;

// Logger
var log = logger.getLogger("REPORTER");

exports.Reporter = function (spec) {
    "use strict";

    var that = {},
    	my_id = spec.id,
    	my_meta = spec.metadata || {};

    that.getErizoAgent = function (callback) {
        var data = {
        	info: {
        		id: my_id,
        		rpc_id: 'ErizoAgent_' + my_id
        	},
        	metadata: my_meta,
        	stats: getStats()
        };
        callback(data);
    };

    var getStats = function () {

    	// TODO: get real monitoring data
    	var data = {
    		cpu: '30'
    	};
    	return data;
    };

	return that;
};