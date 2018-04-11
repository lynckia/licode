/*global require, exports, setInterval*/
'use strict';
var logger = require('./../common/logger').logger;

// Logger
var log = logger.getLogger('EcCloudHandler');

var EA_TIMEOUT = 30000;
var GET_EA_INTERVAL = 5000;
var AGENTS_ATTEMPTS = 5;
var WARN_UNAVAILABLE = 503, WARN_TIMEOUT = 504;
exports.EcCloudHandler = function (spec) {
  var that = {},
  amqper = spec.amqper,
  agents = {};


  that.getErizoAgents = function () {
    amqper.broadcast('ErizoAgent', {method: 'getErizoAgents', args: []}, function (agent) {
      if (agent === 'timeout') {
        log.warn('message: no agents available, code: ' + WARN_UNAVAILABLE );
        return;
      }

      var newAgent = true;

      for (var a in agents) {
        if (a === agent.info.id) {
          // The agent is already registered, I update its stats and reset its
          agents[a].stats = agent.stats;
          agents[a].timeout = 0;
          newAgent = false;
        }
      }

      if (newAgent === true) {
        // New agent
        agents[agent.info.id] = agent;
        agents[agent.info.id].timeout = 0;
      }

    });

    // Check agents timeout
    for (var a in agents) {
      agents[a].timeout ++;
      if (agents[a].timeout > EA_TIMEOUT / GET_EA_INTERVAL) {
        log.warn('message: agent timed out is being removed, ' +
                 'code: ' + WARN_TIMEOUT + ', agentId: ' + agents[a].info.id);
        delete agents[a];
      }
    }
  };

  setInterval(that.getErizoAgents, GET_EA_INTERVAL);

  var getErizoAgent;

  if (global.config.erizoController.cloudHandlerPolicy) {
    getErizoAgent = require('./ch_policies/' +
                      global.config.erizoController.cloudHandlerPolicy).getErizoAgent;
  }

  var tryAgain = function (count, callback) {
    if (count >= AGENTS_ATTEMPTS) {
      callback('timeout');
      return;
    }

    log.warn('message: agent selected timed out trying again, ' +
             'code: ' + WARN_TIMEOUT);

    amqper.callRpc('ErizoAgent', 'createErizoJS', [undefined], {callback: function(resp) {
      var erizoId = resp.erizoId;
      var agentId = resp.agentId;
      var internalId = resp.internalId;
      if (resp === 'timeout') {
        tryAgain(++count, callback);
      } else {
        callback(erizoId, agentId, internalId);
      }
    }});
  };

  that.getErizoJS = function(agentId, internalId, callback) {

    var agentQueue = 'ErizoAgent';

    if (getErizoAgent) {
      agentQueue = getErizoAgent(agents, agentId);
    }

    log.info('message: createErizoJS, agentId: ' + agentQueue);

    amqper.callRpc(agentQueue, 'createErizoJS', [internalId], {callback: function(resp) {
      var erizoId = resp.erizoId;
      var agentId = resp.agentId;
      var internalId = resp.internalId;
      log.info('message: createErizoJS success, erizoId: ' + erizoId +
               ', agentId: ' + agentId + ', internalId: ' + internalId);

      if (resp === 'timeout') {
        tryAgain(0, callback);
      } else {
        callback(erizoId, agentId, internalId);
      }
    }});
  };

  that.deleteErizoJS = function(erizoId) {
    log.info ('message: deleting erizoJS, erizoId: ' + erizoId);
    amqper.broadcast('ErizoAgent', {method: 'deleteErizoJS', args: [erizoId]}, function(){});
  };

  return that;
};
