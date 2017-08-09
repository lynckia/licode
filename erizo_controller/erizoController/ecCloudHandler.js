/*global require, exports, setInterval*/
'use strict';
var logger = require('./common/logger').logger;

// Logger
var log = logger.getLogger('EcCloudHandler');
var db = require('./database').db;
var _ = require('lodash');

var EA_TIMEOUT = 30000;
var EA_OLD_TIMEOUT = 30 * 60 * 1000;
var GET_EA_INTERVAL = 5000;
var AGENTS_ATTEMPTS = 5;
var WARN_UNAVAILABLE = 503, WARN_TIMEOUT = 504;
exports.EcCloudHandler = function (spec) {
  var that = {},
  amqper = spec.amqper,
  agents = {},
  timedOutErizos = [];

  that.getErizoAgents = function () {
    db.erizoJS
      .find({})
      .toArray((err, erizos) => {
        if (err) {
          log.error('message: failed to fetch erizos from db, ' + logger.objectToLog(err));
          return;
        }
        var groupedByAgent = _.groupBy(erizos, 'erizoAgentID');
        var agentsStats = _.mapValues(groupedByAgent, erizos => {
          return _.reduce(erizos, (acc, { publishersCount, subscribersCount, lastUpdated }) => {
            acc.publishersCount += publishersCount;
            acc.subscribersCount += subscribersCount;
            if (!acc.timeout) {
              acc.timeout = lastUpdated;
            }
            acc.timeout = lastUpdated > acc.timeout ? lastUpdated : acc.timeout;
            return acc;
          }, { publishersCount: 0, subscribersCount: 0, timeout: null });
        });
        var pairs = _.toPairs(agentsStats);
        agents = _.fromPairs(pairs.filter(([agentId, { timeout }]) => timeout <= EA_TIMEOUT));
        var now = new Date();
        timedOutErizos = erizos.filter(({ lastUpdated }) => {
          var diff = now - lastUpdated;
          return diff > EA_TIMEOUT && diff <= EA_OLD_TIMEOUT;
        });
        var oldErizos = erizos.filter(({ lastUpdated }) => (now - lastUpdated) > EA_OLD_TIMEOUT);
        if (oldErizos.length) {
          deleteOldErizos(oldErizos);
        }
      });
  };

  that.getTimedOutErizos = function () {
    return timedOutErizos;
  };

  setInterval(that.getErizoAgents, GET_EA_INTERVAL);

  var deleteOldErizos = function (erizos) {
    var ids = erizos.find(({ _id }) => _id);
    db.erizoJS
      .remove({ _id: { $in: ids } }, function(error) {
        if (error) {
          log.warn('message: failed to remove old erizos, ' + logger.objectToLog(error));
        }
      });
  };

  var getErizoAgent;

  if (GLOBAL.config.erizoController.cloudHandlerPolicy) {
    getErizoAgent = require('./ch_policies/' +
                      GLOBAL.config.erizoController.cloudHandlerPolicy).getErizoAgent;
  }

  var tryAgain = function (count, agentId, callback) {
    if (count >= AGENTS_ATTEMPTS) {
      callback('timeout');
      return;
    }

    log.warn('message: agent selected timed out trying again, ' +
             'code: ' + WARN_TIMEOUT + ', agentId:' + agentId);

    var agentQueue = 'ErizoAgent';

    if (getErizoAgent) {
      agentQueue = getErizoAgent(agents);
    }

    amqper.callRpc(agentQueue, 'createErizoJS', [], {callback: function(erizoId) {
      if (erizoId === 'timeout') {
        tryAgain(++count, agentId ,callback);
      } else {
        callback(erizoId.erizoId, erizoId.agentId);
      }
    }});
  };

  that.getErizoJS = function(callback) {

    var agentQueue = 'ErizoAgent';

    if (getErizoAgent) {
      agentQueue = getErizoAgent(agents);
    }

    log.info('message: createErizoJS, agentId: ' + agentQueue);

    amqper.callRpc(agentQueue, 'createErizoJS', [], {callback: function(resp) {
      var erizoId = resp.erizoId;
      var agentId = resp.agentId;
      log.info('message: createErizoJS success, erizoId: ' + erizoId + ', agentId: ' + agentId);

      if (resp === 'timeout') {
        tryAgain(0, agentId, callback);
      } else {
        callback(erizoId, agentId);
      }
    }});
  };

  that.deleteErizoJS = function(erizoId) {
    log.info ('message: deleting erizoJS, erizoId: ' + erizoId);
    amqper.broadcast('ErizoAgent', {method: 'deleteErizoJS', args: [erizoId]}, function(){});
  };

  return that;
};
