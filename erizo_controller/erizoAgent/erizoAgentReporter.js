/*global require, exports*/
'use strict';
var os = require('os');

exports.Reporter = function (spec) {
  var that = {},
      myId = spec.id,
      myMeta = spec.metadata || {};

  var lastTotal = 0;
  var lastIdle = 0;

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

    cpu =  1 - ((idle - lastIdle) / (total - lastTotal));
    var mem = 1 - (os.freemem() / os.totalmem());

    lastTotal = total;
    lastIdle = idle;

    var data = {
      perc_cpu: cpu,  // jshint ignore:line
      perc_mem: mem  // jshint ignore:line
    };

    return data;
  };

  that.getErizoAgent = function (callback) {
    var data = {
      info: {
        id: myId,
        rpc_id: 'ErizoAgent_' + myId  // jshint ignore:line
      },
      metadata: myMeta,
      stats: getStats()
    };
    callback(data);
  };


  return that;
};
