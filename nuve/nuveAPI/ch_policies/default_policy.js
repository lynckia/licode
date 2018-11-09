
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
          keepAlive: Int,
          hostname: String,
          port: Int,
          ssl: bool
      }, ...}

Returns

  erizoControler: the erizo controller selected from ecQueue

*/
exports.getErizoController = (room, ecQueue) => {
  const erizoController = ecQueue[0];
  return erizoController;
};
