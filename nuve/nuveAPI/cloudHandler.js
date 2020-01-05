/* global require, setInterval, clearInterval, exports */

/* eslint-disable no-param-reassign */

const rpc = require('./rpc/rpc');
// eslint-disable-next-line import/no-unresolved
const config = require('./../../licode_config');
const logger = require('./logger').logger;
const erizoControllerRegistry = require('./mdb/erizoControllerRegistry');
const roomRegistry = require('./mdb/roomRegistry');

// Logger
const log = logger.getLogger('CloudHandler');

let AWS;

const INTERVAL_TIME_EC_READY = 500;
const TOTAL_ATTEMPTS_EC_READY = 20;
const INTERVAL_TIME_CHECK_KA = 1000;
const MAX_KA_COUNT = 10;

let getErizoController;

const getEcQueue = (callback) => {
  //* ******************************************************************
  // States:
  //  0: Not available
  //  1: Warning
  //  2: Available
  //* ******************************************************************

  erizoControllerRegistry.getErizoControllers((erizoControllers) => {
    let ecQueue = [];
    const noAvailable = [];
    const warning = [];
    erizoControllers.forEach((ec) => {
      if (ec.state === 2) {
        ecQueue.push(ec);
      }
      if (ec.state === 1) {
        warning.push(ec);
      }
      if (ec.state === 0) {
        noAvailable.push(ec);
      }
    });

    ecQueue = ecQueue.concat(warning);

    if (ecQueue.length === 0) {
      log.error('No erizoController is available.');
    }
    ecQueue = ecQueue.concat(noAvailable);
    warning.forEach((ec) => {
      log.warn(`Erizo Controller in ${ec.ip} ` +
                     'has reached the warning number of rooms');
    });
    noAvailable.forEach((ec) => {
      log.warn(`Erizo Controller in ${ec.ip} ` +
                     'has reached the limit number of rooms');
    });
    callback(ecQueue);
  });
};

const assignErizoController = (erizoControllerId, room, callback) => {
  roomRegistry.assignErizoControllerToRoom(room, erizoControllerId, callback);
};

const unassignErizoController = (erizoControllerId) => {
  roomRegistry.getRooms((rooms) => {
    rooms.forEach((room) => {
      if (room.erizoControllerId &&
        room.erizoControllerId.equals(erizoControllerId)) {
        room.erizoControllerId = undefined;
        roomRegistry.updateRoom(room._id, room);
      }
    });
  });
};

const checkKA = () => {
  erizoControllerRegistry.getErizoControllers((erizoControllers) => {
    erizoControllers.forEach((ec) => {
      const id = ec._id;
      if (ec.keepAlive > MAX_KA_COUNT) {
        log.warn('ErizoController', id, ' in ', ec.ip,
          'does not respond. Deleting it.');
        erizoControllerRegistry.removeErizoController(id);
        unassignErizoController(id);
        return;
      }
      erizoControllerRegistry.incrementKeepAlive(id);
    });
  });
};

setInterval(checkKA, INTERVAL_TIME_CHECK_KA);

if (config.nuve.cloudHandlerPolicy) {
  // eslint-disable-next-line
  getErizoController = require(`./ch_policies/${config.nuve.cloudHandlerPolicy}`).getErizoController;
}

const addNewPrivateErizoController = (ip, hostname, port, ssl, callback) => {
  const erizoController = {
    ip,
    state: 2,
    keepAlive: 0,
    hostname,
    port,
    ssl,
  };
  erizoControllerRegistry.addErizoController(erizoController, (erizoControllerCb) => {
    const id = erizoControllerCb._id;
    log.info('New erizocontroller (', id, ') in: ', erizoControllerCb.ip);
    callback({ id, publicIP: ip, hostname, port, ssl });
  });
};

const addNewAmazonErizoController = (privateIP, hostname, port, ssl, callback) => {
  let publicIP;

  if (AWS === undefined) {
    // eslint-disable-next-line global-require, import/no-extraneous-dependencies
    AWS = require('aws-sdk');
  }
  log.info('private ip ', privateIP);
  new AWS.MetadataService({
    httpOptions: {
      timeout: 5000,
    },
  }).request('/latest/meta-data/public-ipv4', (err, data) => {
    if (err) {
      log.info('Error: ', err);
      callback('error');
    } else {
      publicIP = data;
      log.info('public IP: ', publicIP);
      addNewPrivateErizoController(publicIP, hostname, port, ssl, callback);
    }
  });
};

exports.addNewErizoController = (msg, callback) => {
  if (msg.cloudProvider === '') {
    addNewPrivateErizoController(msg.ip, msg.hostname, msg.port, msg.ssl, callback);
  } else if (msg.cloudProvider === 'amazon') {
    addNewAmazonErizoController(msg.ip, msg.hostname, msg.port, msg.ssl, callback);
  }
};

exports.keepAlive = (id, callback) => {
  let result;

  erizoControllerRegistry.getErizoController(id, (erizoController) => {
    if (erizoController) {
      erizoControllerRegistry.updateErizoController(id, { keepAlive: 0 });
      result = 'ok';
      // log.info('KA: ', id);
    } else {
      result = 'whoareyou';
      log.warn('I received a keepAlive message from an unknown erizoController');
    }
    callback(result);
  });
};

exports.setInfo = (params) => {
  log.info('Received info ', params, '. Recalculating erizoControllers priority');
  erizoControllerRegistry.updateErizoController(params.id, { state: params.state });
};

exports.killMe = (ip) => {
  log.info('[CLOUD HANDLER]: ErizoController in host ', ip, 'wants to be killed.');
};

const getErizoControllerForRoom = (room, callback) => {
  const roomId = room._id;

  roomRegistry.getRoom(roomId, (roomResult) => {
    const id = roomResult.erizoControllerId;
    if (id) {
      erizoControllerRegistry.getErizoController(id, (erizoController) => {
        if (erizoController) {
          callback(erizoController);
        } else {
          roomResult.erizoControllerId = undefined;
          roomRegistry.updateRoom(roomResult._id, roomResult);
          getErizoControllerForRoom(roomResult, callback);
        }
      });
      return;
    }

    let attempts = 0;
    let intervalId;

    getEcQueue((ecQueue) => {
      intervalId = setInterval(() => {
        let erizoController;
        if (getErizoController) {
          erizoController = getErizoController(room, ecQueue);
        } else {
          erizoController = ecQueue[0];
        }
        const erizoControllerId = erizoController ? erizoController._id : undefined;

        if (erizoControllerId !== undefined) {
          assignErizoController(erizoControllerId, room, (assignedEc) => {
            callback(assignedEc);
            clearInterval(intervalId);
          });
        }

        if (attempts > TOTAL_ATTEMPTS_EC_READY) {
          clearInterval(intervalId);
          callback('timeout');
        }
        attempts += 1;
      }, INTERVAL_TIME_EC_READY);
    });
  });
};

exports.getErizoControllerForRoom = getErizoControllerForRoom;

exports.getUsersInRoom = (roomId, callback) => {
  roomRegistry.getRoom(roomId, (room) => {
    if (room && room.erizoControllerId) {
      const rpcID = `erizoController_${room.erizoControllerId}`;
      rpc.callRpc(rpcID, 'getUsersInRoom', [roomId], { callback(users) {
        callback(users);
      } });
    } else {
      callback([]);
    }
  });
};

exports.deleteRoom = (roomId, callback) => {
  roomRegistry.getRoom(roomId, (room) => {
    if (room && room.erizoControllerId) {
      const rpcID = `erizoController_${room.erizoControllerId}`;
      rpc.callRpc(rpcID, 'deleteRoom', [roomId], { callback(result) {
        callback(result);
      } });
    } else {
      callback('Success');
    }
  });
};

exports.deleteUser = (user, roomId, callback) => {
  roomRegistry.getRoom(roomId, (room) => {
    if (room && room.erizoControllerId) {
      const rpcID = `erizoController_${room.erizoControllerId}`;
      rpc.callRpc(rpcID,
        'deleteUser',
        [{ user, roomId }],
        { callback(result) {
          callback(result);
        } });
    } else {
      callback('Room does not exist or the user is not connected');
    }
  });
};

exports.getEcQueue = getEcQueue;
