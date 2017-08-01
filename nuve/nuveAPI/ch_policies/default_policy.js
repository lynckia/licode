'use strict';
/*
Params

	room: room to which we need to assing a erizoController.
		{
		name: String,
		[p2p: bool],
		[data: Object],
		_id: ObjectId
		}

	ec_queue: available erizo controllers ordered by priority
		{ erizoControllerId: {
        	ip: String,
        	state: Int,
        	keepAliveTs: Date,
        	hostname: String,
        	port: Int,
        	ssl: bool
   	 	}, ...}

Returns

	erizoControler: the erizo controller selected from ecQueue

*/
exports.getErizoController = function (room, ecQueue) {
  var erizoController = ecQueue[0];
  return erizoController;
};
