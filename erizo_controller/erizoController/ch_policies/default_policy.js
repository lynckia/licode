/*
Params

  agents: object with the available agents
    agent_id : {
          info: {
            id: String,
            rpc_id: String
          },
          metadata: Object,
          stats: {
        perc_cpu: Int
          },
      timeout: Int // number of periods during the agent has not respond
      }

Returns

  rpc_id: agent.info.rpc_id field of the selected agent.
 *default value: "ErizoAgent" - select the agent in round-robin mode

*/
exports.getErizoAgent = (agents, agentId) => {
  if (agentId) {
    return `ErizoAgent_${agentId}`;
  }
  return 'ErizoAgent';
};
