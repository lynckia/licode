/*global require, logger. setInterval, clearInterval, Buffer, exports*/
var rpc = require('./../common/rpc');
var logger = require('./../common/logger').logger;

// Logger
var log = logger.getLogger("ErizoAgent");

var spawn = require('child_process').spawn;

var config = require('./../../licode_config');

// Configuration default values
config.erizoAgent = config.erizoAgent || {};
config.erizoAgent.maxProcesses = config.erizoAgent.maxProcesses || 1;
config.erizoAgent.prerunProcesses = config.erizoAgent.prerunProcesses || 1;
    
var childs = [];

var SEARCH_INTERVAL = 5000;

var idle_erizos = [];

var erizos = [];

var guid = (function() {
  function s4() {
    return Math.floor((1 + Math.random()) * 0x10000)
               .toString(16)
               .substring(1);
  }
  return function() {
    return s4() + s4() + '-' + s4() + '-' + s4() + '-' +
           s4() + '-' + s4() + s4() + s4();
  };
})();

var saveChild = function(id) {
    childs.push(id);
};

var removeChild = function(id) {
    childs.push(id);
};

var launchErizoJS = function() {
    console.log("Running process");
    var id = guid();
    var fs = require('fs');
    var out = fs.openSync('./erizo-' + id + '.log', 'a');
    var err = fs.openSync('./erizo-' + id + '.log', 'a');
    var erizoProcess = spawn('node', ['./../erizoJS/erizoJS.js', id], { detached: true, stdio: [ 'ignore', out, err ] });
    erizoProcess.unref();
    erizoProcess.on('close', function (code) {
        var index = idle_erizos.indexOf(id);
        var index2 = erizos.indexOf(id);
        if (index > -1) {
            idle_erizos.splice(index, 1);
            launchErizoJS();
        } else if (index2 > -1) {
            erizos.splice(index2, 1);
        }
        fillErizos();
    });
    idle_erizos.push(id);
};

var fillErizos = function() {
    for (var i = idle_erizos.length; i<config.erizoAgent.prerunProcesses; i++) {
        launchErizoJS();
    }
};

var api = {
    createErizoJS: function(id, callback) {
        try {
            var erizo_id = idle_erizos.pop();
            callback("callback", erizo_id);

            // We re-use Erizos
            if ((erizos.length + idle_erizos.length + 1) >= config.erizoAgent.maxProcesses) {
                idle_erizos.push(erizo_id);
            } else {
                erizos.push(erizo_id);
            }

            // We launch more processes
            fillErizos();
            
        } catch (error) {
            console.log("Error in ErizoAgent:", error);
        }
    }
};

fillErizos();

rpc.connect(function () {
    "use strict";
    rpc.setPublicRPC(api);

    var rpcID = "ErizoAgent";
    

    rpc.bind(rpcID);

});

/*
setInterval(function() {
    var search = spawn("ps", ['-aef']);

    search.stdout.on('data', function(data) {

    });

    search.on('close', function (code) {

    });
}, SEARCH_INTERVAL);
*/