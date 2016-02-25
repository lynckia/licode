/*global require, exports, console, setInterval, clearInterval*/

var logger = require('./../common/logger').logger;

// Logger
var log = logger.getLogger("ECCH");

var EA_TIMEOUT = 30000;
var GET_EA_INTERVAL = 5000;
var AGENTS_ATTEMPTS = 5;

var regresion = require('./ch_policies/regresion.js');

exports.Ecch = function (spec) {
    "use strict";

    var that = {},
	    amqper = spec.amqper,
	    agents = {};

	var getErizoAgents = function () {
		amqper.broadcast('ErizoAgent', {method: 'getErizoAgents', args: []}, function (agent) {

			if (agent === 'timeout') {
				log.warn('No agents available');
				return;
			}

			var new_agent = true;

			for (var a in agents) {
				if (a === agent.info.id) {
					// The agent is already registered, I update its stats and reset its
					agents[a].otms = agent.otms;
					agents[a].stats = agent.stats;
					agents[a].timeout = 0;
					new_agent = false;
				}
			}

			if (new_agent === true) {
				// New agent
				agents[agent.info.id] = agent;
				agents[agent.info.id].timeout = 0;
				agents[agent.info.id].erizoJSs = [];
			}

			//console.log('all ', agents);
		});

		regresion.update(agents, that.rooms);

		// Check agents timeout
		for (var a in agents) {
			agents[a].timeout ++;
			if (agents[a].timeout > EA_TIMEOUT / GET_EA_INTERVAL) {
				log.warn('Agent ', agents[a].info.id, ' does not respond. Deleting it.');
				delete agents[a];
			}
		}
	};

	var intervalId = setInterval(getErizoAgents, GET_EA_INTERVAL);

	var getErizoAgent = undefined;

	if (config.erizoController.cloudHandlerPolicy) {
	    getErizoAgent = require('./ch_policies/' + config.erizoController.cloudHandlerPolicy).getErizoAgent;
	}

	// {room1: {otm1: n_subs, otm2: n_subs}, room2: {otm3: n_subs, otm4: n_subs}}
	that.rooms = {};

	that.getErizoJS = function(roomId, callback) {

		var agent_queue = 'ErizoAgent';

		if (getErizoAgent) {
			agent_queue = getErizoAgent(agents, that.rooms, roomId);
		}

		log.info('Asking erizoJS to agent ', agent_queue);

		amqper.callRpc(agent_queue, 'createErizoJS', [], {callback: function(erizo_id) {

			// Solo en caso de usar algo diferente a round robin
			if (agent_queue !== 'ErizoAgent') {

				var agent_id;

				for (var a in agents) {
					if (agents[a].info.rpc_id === agent_queue) {
						agent_id = a;
					}
				}
				if (agents[agent_id].erizoJSs.indexOf(erizo_id) === -1) {
					agents[agent_id].erizoJSs.push(erizo_id);
					log.info('ErizoJS ', erizo_id, 'created in Agent', agent_id);
				}
			};

			if (erizo_id === 'timeout') {
				try_again(0, callback);
			} else {
	        	callback(erizo_id);
			}
	    }});
	};

	var try_again = function (count, callback) {

		if (count >= AGENTS_ATTEMPTS) {
			callback('timeout');
			return;
		}

		log.warn('Agent selected not available, trying with another one in ErizoAgent...');
				
		amqper.callRpc('ErizoAgent', 'createErizoJS', [], {callback: function(erizo_id) {
			if (erizo_id === 'timeout') {
				try_again(++count, callback);
			} else {
				callback(erizo_id);
			}
	    }});
	};

	that.deleteErizoJS = function(erizo_id) {
        amqper.broadcast("ErizoAgent", {method: "deleteErizoJS", args: [erizo_id]}, function(){}); 
	};

	return that;
};