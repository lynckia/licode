'use strict';

var _ = require('lodash');

exports.getErizoController = function (room, ecQueue) {
  var sortedByRooms = _.sortBy(ecQueue, ({ rooms }) => rooms);
  return sortedByRooms[0];
};
