'use strict';

var _ = require('lodash');

exports.getErizoController = function (room, ecQueue) {
  var sortedByRooms = _.sortBy(ecQueue, ({ roomsCount }) => roomsCount);
  return sortedByRooms[0];
};
