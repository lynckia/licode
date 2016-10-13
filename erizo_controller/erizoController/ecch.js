/*global require, exports, console, setInterval, clearInterval*/

var logger = require('./../common/logger').logger;

// Logger
var log = logger.getLogger("Ecch");

var EA_TIMEOUT = 30000;
var GET_EA_INTERVAL = 5000;
var AGENTS_ATTEMPTS = 5;
var WARN_UNAVAILABLE = 503, WARN_TIMEOUT = 504;
exports.Ecch = function (spec) {
    "use strict";

    var that = {},
	    amqper = spec.amqper,
	    agents = {};


	var getErizoAgents = function () {
		amqper.broadcast('ErizoAgent', {method: 'getErizoAgents', args: []}, function (agent) {
			if (agent === 'timeout') {
				log.warn('message: no agents available, code: ' + WARN_UNAVAILABLE );
				return;
			}

			var new_agent = true;

			for (var a in agents) {
				if (a === agent.info.id) {
					// The agent is already registered, I update its stats and reset its 
					agents[a].stats = agent.stats;
					agents[a].timeout = 0;
					new_agent = false;
				}
			}

			if (new_agent === true) {
				// New agent
				agents[agent.info.id] = agent;
				agents[agent.info.id].timeout = 0;
			}

		});

		// Check agents timeout
		for (var a in agents) {
			agents[a].timeout ++;
			if (agents[a].timeout > EA_TIMEOUT / GET_EA_INTERVAL) {
				log.warn('message: agent timed out is being removed, code: ' + WARN_TIMEOUT + ', agentId: ' + agents[a].info.id);
				delete agents[a];
			}
		}
	};

	var intervalId = setInterval(getErizoAgents, GET_EA_INTERVAL);

	var getErizoAgent = undefined;

	if (config.erizoController.cloudHandlerPolicy) {
	    getErizoAgent = require('./ch_policies/' + config.erizoController.cloudHandlerPolicy).getErizoAgent;
	}

	that.getErizoJS = function(callback) {

		var agent_queue = 'ErizoAgent';

		if (getErizoAgent) {
			agent_queue = getErizoAgent(agents);
		}

		log.info('message: createErizoJS, agentId: ' + agent_queue);

		amqper.callRpc(agent_queue, 'createErizoJS', [], {callback: function(resp) {
			var erizo_id = resp.erizo_id;
			var agent_id = resp.agent_id;
            log.info('message: createErizoJS success, erizoId: ' + erizo_id + ', agentId: ' + agent_id);
			
			if (resp === 'timeout') {
				try_again(0, agent_id, callback);
			} else {
	        	callback(erizo_id, agent_id);
			}
	    }});
	};

	var try_again = function (count, agentId, callback) {

		if (count >= AGENTS_ATTEMPTS) {
			callback('timeout');
			return;
		}

		log.warn('message: agent selected timed out trying again, code: ' + WARN_TIMEOUT + ', agentId: ' + agentId);
				
		amqper.callRpc('ErizoAgent', 'createErizoJS', [], {callback: function(erizo_id) {
			if (erizo_id === 'timeout') {
				try_again(++count, agentId ,callback);
			} else {
				callback(erizo_id);
			}
	    }});
	};

	that.deleteErizoJS = function(erizo_id) {
        log.info ("message: deleting erizoJS, erizoId: " + erizo_id);
        amqper.broadcast("ErizoAgent", {method: "deleteErizoJS", args: [erizo_id]}, function(){}); 
	};

	return that;
};
