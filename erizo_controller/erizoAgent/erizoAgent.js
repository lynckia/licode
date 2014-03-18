/*global require, logger. setInterval, clearInterval, Buffer, exports*/
var rpc = require('./../common/rpc');
var logger = require('./../common/logger').logger;

var spawn = require('child_process').spawn;
    
var childs = [];

var SEARCH_INTERVAL = 5000;

var saveChild = function(id) {
    childs.push(id);
};

var removeChild = function(id) {
    childs.push(id);
};

var api = {
    createErizoJS: function(id, callback) {
        console.log("Running process");
        var erizoProcess = spawn('node', ['./../erizoJS/erizoJS.js', id, ' & ']);

        erizoProcess.stdout.on('data', function (data) {
            if (data + "" === "ErizoJS started") {
                console.log("Everything ok", m);
                callback('callback');
            }
            console.log('' + data);
        });

        erizoProcess.stderr.on('data', function (data) {
            console.log('' + data);
        });

        erizoProcess.on('close', function (code) {
          if (code !== 0) {
            console.log('ErizoJS process ' + id + ' exited with code ' + code);
          }
        });

        
    }
};

rpc.connect(function () {
    "use strict";
    rpc.setPublicRPC(api);

    var rpcID = "ErizoAgent";

    rpc.bind(rpcID, function() {

    });

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