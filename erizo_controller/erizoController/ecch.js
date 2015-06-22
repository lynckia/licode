/*global require, exports, console, setInterval, clearInterval*/

var logger = require('./../common/logger').logger;

// Logger
var log = logger.getLogger("ECCH");

var GET_EA_INTERVAL = 5000;

exports.Ecch = function (spec) {
    "use strict";

    var that = {},
	    amqper = spec.amqper;

	var intervalId = setInterval(function (){
		amqper.broadcast('ErizoAgent', 'getErizoAgents', function (resp) {
			console.log('respueeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee ', resp);
		});
	}, GET_EA_INTERVAL);

	that.getErizoJS = function(callback) {
		amqper.callRpc("ErizoAgent", "createErizoJS", [], {callback: function(erizo_id) {
	        callback(erizo_id);
	    }});
	};

	return that;
};