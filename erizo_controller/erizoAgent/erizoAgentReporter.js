/*global require, exports, console, setInterval, clearInterval*/

var logger = require('./../common/logger').logger;

// Logger
var log = logger.getLogger("ErizoAgentReporter");

var os = require('os');

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

    var last_total = 0;
    var last_idle = 0;

    var getStats = function () {

        var cpus = os.cpus();

        var user = 0;
        var nice = 0;
        var sys = 0;
        var idle = 0;
        var irq = 0;
        var total = 0;

        for(var cpu in cpus){
            
            user += cpus[cpu].times.user;
            nice += cpus[cpu].times.nice;
            sys += cpus[cpu].times.sys;
            irq += cpus[cpu].times.irq;
            idle += cpus[cpu].times.idle;
        }

        total = user + nice + sys + idle + irq;

        var cpu =  1 - ((idle - last_idle) / (total - last_total));
        var mem = 1 - (os.freemem() / os.totalmem());

        last_total = total;
        last_idle = idle;

    	var data = {
    		perc_cpu: cpu,
            perc_mem: mem
    	};
        
    	return data;
    };

	return that;
};
