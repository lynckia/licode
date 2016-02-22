/*global require, exports, console, setInterval, clearInterval*/

var logger = require('./../common/logger').logger;
var exec = require('child_process').exec;
var usage = require('usage');

// Logger
var log = logger.getLogger("REPORTER");

var os = require('os');

exports.Reporter = function (spec) {
    "use strict";

    var that = {},
        my_id = spec.id,
        my_meta = spec.metadata || {};

    that.getErizoAgent = function (callback) {

        getByProcess(0, {}, function (otms) {
            var data = {
                info: {
                    id: my_id,
                    rpc_id: 'ErizoAgent_' + my_id
                },
                metadata: my_meta,
                stats: getStats(),
                otms: otms
            };
            callback(data);
        });

    };

    var getByProcess = function (index, otms, callback) {

        console.log('Ppio', otms);
        if (index === Object.keys(spec.processes).length) {
            console.log('Hago el callback', otms);
            callback(otms);
        } else {
            console.log('Getting by process', index, ' total', Object.keys(spec.processes).length);
            var id = Object.keys(spec.processes)[index];
            exec('pgrep -P ' + spec.processes[id].pid, function (error, stdout, stderr) {
                usage.lookup(parseInt(stdout), { keepHistory: true }, function(err, result) {
                    var res = result.cpu/os.cpus().length;
                    console.log('Result id ', id, ':', res);
                    otms[id] = res;
                    console.log('OTMS', otms);
                    getByProcess(++index, otms, callback);
                });
            });
        }
    }

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
            perc_cpu: cpu*100,
            perc_mem: mem*100
        };
        
        return data;
    };

    return that;
};