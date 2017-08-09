var _ = require('lodash');

exports.getErizoAgent = function (agents) {
  'use strict';
  const pairs = _.toPairs(agents);
  const leastConnectionsAgent = _.minBy(pairs, ([agentId, { publishersCount, subscribersCount }]) => publishersCount + subscribersCount);
  if (leastConnectionsAgent) {
    return `ErizoAgent_${leastConnectionsAgent[0]}`;
  } else {
    return 'ErizoAgent';
  }
};
