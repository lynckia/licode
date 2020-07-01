/* global require, setInterval, clearInterval, exports */

/* eslint-disable no-param-reassign */

const rpcPublic = require('./rpc/rpcPublic');
// eslint-disable-next-line import/no-unresolved
const config = require('./../../licode_config');
// eslint-disable-next-line import/no-extraneous-dependencies
const Getopt = require('node-getopt');

// Configuration default values
global.config = config || {};
global.config.erizoController = global.config.erizoController || {};
global.config.erizoController.iceServers =
  global.config.erizoController.iceServers || [{ url: 'stun:stun.l.google.com:19302' }];
global.config.erizoController.defaultVideoBW = global.config.erizoController.defaultVideoBW || 300;
global.config.erizoController.maxVideoBW = global.config.erizoController.maxVideoBW || 300;
global.config.erizoController.publicIP = global.config.erizoController.publicIP || '';
global.config.erizoController.hostname = global.config.erizoController.hostname || '';
global.config.erizoController.port = global.config.erizoController.port || 8080;
global.config.erizoController.ssl = global.config.erizoController.ssl || false;
global.config.erizoController.ssl_key =
  global.config.erizoController.ssl_key || '../../cert/key.pem';
global.config.erizoController.ssl_cert =
  global.config.erizoController.ssl_cert || '../../cert/cert.pem';
global.config.erizoController.sslCaCerts =
  global.config.erizoController.sslCaCerts || undefined;
global.config.erizoController.listen_port = global.config.erizoController.listen_port || 8080;
global.config.erizoController.listen_ssl = global.config.erizoController.listen_ssl || false;
global.config.erizoController.turnServer = global.config.erizoController.turnServer || undefined;
global.config.erizoController.warning_n_rooms = global.config.erizoController.warning_n_rooms || 15;
global.config.erizoController.limit_n_rooms = global.config.erizoController.limit_n_rooms || 20;
global.config.erizoController.interval_time_keepAlive =
  global.config.erizoController.interval_time_keepAlive || 1000;
global.config.erizoController.report.session_events =
  global.config.erizoController.report.session_events || false;
global.config.erizoController.recording_path =
  global.config.erizoController.recording_path || undefined;
global.config.erizoController.exitOnNuveCheckFail =
  global.config.erizoController.exitOnNuveCheckFail || false;
global.config.erizoController.allowSinglePC =
  global.config.erizoController.allowSinglePC || '';
global.config.erizoController.maxErizosUsedByRoom =
  global.config.erizoController.maxErizosUsedByRoom || 100;

global.config.erizoController.roles = global.config.erizoController.roles ||
  { presenter: { publish: true, subscribe: true, record: true },
    viewer: { subscribe: true },
    viewerWithData: { subscribe: true,
      publish: { audio: false,
        video: false,
        screen: false,
        data: true } } };

// Parse command line arguments
const getopt = new Getopt([
  ['r', 'rabbit-host=ARG', 'RabbitMQ Host'],
  ['g', 'rabbit-port=ARG', 'RabbitMQ Port'],
  ['b', 'rabbit-heartbeat=ARG', 'RabbitMQ AMQP Heartbeat Timeout'],
  ['l', 'logging-config-file=ARG', 'Logging Config File'],
  ['t', 'iceServers=ARG', 'Ice Servers URLs Array'],
  ['b', 'defaultVideoBW=ARG', 'Default video Bandwidth'],
  ['M', 'maxVideoBW=ARG', 'Max video bandwidth'],
  ['i', 'publicIP=ARG', 'Erizo Controller\'s public IP'],
  ['H', 'hostname=ARG', 'Erizo Controller\'s hostname'],
  ['p', 'port=ARG', 'Port used by clients to reach Erizo Controller'],
  ['S', 'ssl', 'Enable SSL for clients'],
  ['L', 'listen_port=ARG', 'Port where Erizo Controller will listen to new connections.'],
  ['s', 'listen_ssl', 'Enable HTTPS in server'],
  ['R', 'recording_path=ARG', 'Recording path.'],
  ['h', 'help', 'display this help'],
]);

const opt = getopt.parse(process.argv.slice(2));

Object.keys(opt.options).forEach((prop) => {
  const value = opt.options[prop];
  switch (prop) {
    case 'help':
      getopt.showHelp();
      process.exit(0);
      break;
    case 'rabbit-host':
      global.config.rabbit = global.config.rabbit || {};
      global.config.rabbit.host = value;
      break;
    case 'rabbit-port':
      global.config.rabbit = global.config.rabbit || {};
      global.config.rabbit.port = value;
      break;
    case 'rabbit-heartbeat':
      global.config.rabbit = global.config.rabbit || {};
      global.config.rabbit.heartbeat = value;
      break;
    case 'logging-config-file':
      global.config.logger = global.config.logger || {};
      global.config.logger.configFile = value;
      break;
    default:
      global.config.erizoController[prop] = value;
      break;
  }
});

// Load submodules with updated config
const logger = require('./../common/logger').logger;
const amqper = require('./../common/amqper');
const ecch = require('./ecCloudHandler').EcCloudHandler({ amqper });
const nuve = require('./nuveProxy').NuveProxy({ amqper });
const Rooms = require('./models/Room').Rooms;
const Channel = require('./models/Channel').Channel;

// Logger
const log = logger.getLogger('ErizoController');

let server;

if (global.config.erizoController.listen_ssl) {
  // eslint-disable-next-line global-require
  const https = require('https');
  // eslint-disable-next-line global-require
  const fs = require('fs');
  const options = {
    key: fs.readFileSync(config.erizoController.ssl_key).toString(),
    cert: fs.readFileSync(config.erizoController.ssl_cert).toString(),
  };
  if (config.erizoController.sslCaCerts) {
    options.ca = [];
    config.erizoController.sslCaCerts.forEach((cert) => {
      options.ca.push(fs.readFileSync(cert).toString());
    });
  }
  server = https.createServer(options);
} else {
  // eslint-disable-next-line global-require
  const http = require('http');
  server = http.createServer();
}

server.listen(global.config.erizoController.listen_port);
// eslint-disable-next-line global-require, import/no-extraneous-dependencies
const io = require('socket.io').listen(server, { log: false });

io.set('transports', ['websocket']);

const EXIT_ON_NUVE_CHECK_FAIL = global.config.erizoController.exitOnNuveCheckFail;
const WARNING_N_ROOMS = global.config.erizoController.warning_n_rooms;
const LIMIT_N_ROOMS = global.config.erizoController.limit_n_rooms;

const INTERVAL_TIME_KEEPALIVE =
  global.config.erizoController.interval_time_keepAlive;

const BINDED_INTERFACE_NAME = global.config.erizoController.networkInterface;

let myId;
const rooms = new Rooms(amqper, ecch);

let myState;

const addToCloudHandler = (callback) => {
  // eslint-disable-next-line global-require
  const interfaces = require('os').networkInterfaces();
  const addresses = [];
  let address;
  let publicIP;

  if (interfaces) {
    Object.keys(interfaces).forEach((k) => {
      if (!global.config.erizoController.networkinterface ||
        global.config.erizoController.networkinterface === k) {
        Object.keys(interfaces[k]).forEach((k2) => {
          address = interfaces[k][k2];
          if (address.family === 'IPv4' && !address.internal) {
            if (k === BINDED_INTERFACE_NAME || !BINDED_INTERFACE_NAME) {
              addresses.push(address.address);
            }
          }
        });
      }
    });
  }

  if (global.config.erizoController.publicIP === '' ||
        global.config.erizoController.publicIP === undefined) {
    publicIP = addresses[0];
  } else {
    publicIP = global.config.erizoController.publicIP;
  }

  const startKeepAlives = (erizoControllerId, erizoPublicIP) => {
    const intervalId = setInterval(() => {
      nuve.keepAlive(erizoControllerId)
        .then(() => true)
        .catch((result) => {
          if (result === 'whoareyou') {
            // TODO: It should try to register again in Cloud Handler.
            // But taking into account current rooms, users, ...
            log.error('message: This ErizoController does not exist in cloudHandler ' +
                        'to avoid unexpected behavior this ErizoController will die');
            clearInterval(intervalId);
            return false;
          }
          return true;
        }).then((result) => {
          if (!result) {
            nuve.killMe(erizoPublicIP);
            if (EXIT_ON_NUVE_CHECK_FAIL) {
              log.error('message: Closing ErizoController ' +
                   '- does not exist in Nuve CloudHandler');
              process.exit(-1);
            }
          }
        });
    }, INTERVAL_TIME_KEEPALIVE);
  };

  const addECToCloudHandler = (attempt) => {
    if (attempt <= 0) {
      log.error('message: addECtoCloudHandler cloudHandler does not respond - fatal');
      process.exit(-1);
      return;
    }

    const controller = {
      cloudProvider: global.config.cloudProvider.name,
      ip: publicIP,
      hostname: global.config.erizoController.hostname,
      port: global.config.erizoController.port,
      ssl: global.config.erizoController.ssl,
    };
    nuve.addNewErizoController(controller).then((msg) => {
      log.info('message: succesfully added to cloudHandler');

      publicIP = msg.publicIP;
      myId = msg.id;
      myState = 2;

      startKeepAlives(myId, publicIP);
      callback('callback');
    }).catch((reason) => {
      if (reason === 'timeout') {
        log.warn('message: addECToCloudHandler cloudHandler does not respond, ' +
                     `attemptsLeft: ${attempt}`);

        // We'll try it more!
        setTimeout(() => {
          attempt -= 1;
          addECToCloudHandler(attempt);
        }, 3000);
      } else {
        log.error('message: cannot contact cloudHandler');
      }
    });
  };
  addECToCloudHandler(5);
};

//* ******************************************************************
//       When adding or removing rooms we use an algorithm to check the state
//       If there is a state change we send a message to cloudHandler
//
//       States:
//            0: Not available
//            1: Warning
//            2: Available
//* ******************************************************************
const updateMyState = () => {
  let nRooms = 0;
  let newState;

  nRooms = rooms.size();

  log.debug('message: Updating my state, id:', myId, ', rooms:', nRooms);

  if (nRooms < WARNING_N_ROOMS) {
    newState = 2;
  } else if (nRooms > LIMIT_N_ROOMS) {
    log.warn(`message: reached Room Limit, roomLimit:${LIMIT_N_ROOMS}`);
    newState = 0;
  } else {
    log.warn('message: reached Warning room limit, ' +
                 `warningRoomLimit: ${WARNING_N_ROOMS}, ` +
                 `roomLimit: ${LIMIT_N_ROOMS}`);
    newState = 1;
  }

  if (newState === myState) {
    return;
  }

  myState = newState;

  nuve.setInfo({ id: myId, state: myState });
};

const getSinglePCConfig = (singlePC) => {
  log.info(`message: getting singlePC configuration, singlePc: ${singlePC}` +
    `, global: ${global.config.erizoController.allowSinglePC}`);
  return !!singlePC && global.config.erizoController.allowSinglePC;
};

const listen = () => {
  io.sockets.on('connection', (socket) => {
    log.info(`message: socket connected, socketId: ${socket.id}`);

    const channel = new Channel(socket, nuve);

    channel.on('connected', (token, options, callback) => {
      options = options || {};
      try {
        const room = rooms.getOrCreateRoom(myId, token.room, token.p2p);
        options.singlePC = getSinglePCConfig(options.singlePC);
        const client = room.createClient(channel, token, options);
        log.info(`message: client connected, clientId: ${client.id}, ` +
            `socketId: ${socket.id}, singlePC: ${options.singlePC},`,
        logger.objectToLog(options), logger.objectToLog(options.metadata));
        if (!room.p2p && global.config.erizoController.report.session_events) {
          const timeStamp = new Date();
          amqper.broadcast('event', { room: room.id,
            user: client.id,
            name: token.userName,
            type: 'user_connection',
            timestamp: timeStamp.getTime() });
        }

        const streamList = [];
        room.streamManager.forEachPublishedStream((stream) => {
          streamList.push(stream.getPublicStream());
        });

        callback('success', { streams: streamList,
          id: room.id,
          clientId: client.id,
          singlePC: options.singlePC,
          p2p: room.p2p,
          defaultVideoBW: global.config.erizoController.defaultVideoBW,
          maxVideoBW: global.config.erizoController.maxVideoBW,
          iceServers: global.config.erizoController.iceServers });
      } catch (e) {
        log.warn('message: error creating Room or Client, error:', e,
          logger.objectToLog(options), logger.objectToLog(options.metadata));
      }
    });

    channel.on('reconnected', (clientId) => {
      rooms.forEachRoom((room) => {
        const client = room.getClientById(clientId);
        if (client !== undefined) {
          client.setNewChannel(channel);
        }
      });
    });

    socket.channel = channel;
  });
};


/*
 *Gets a list of users in a given room.
 */
exports.getUsersInRoom = (roomId, callback) => {
  const users = [];
  const room = rooms.getRoomById(roomId);
  if (room === undefined) {
    callback(users);
    return;
  }

  room.forEachClient((client) => {
    users.push(client.user);
  });

  callback(users);
};

/*
 *Remove user from a room.
 */
exports.deleteUser = (user, roomId, callback) => {
  const room = rooms.getRoomById(roomId);

  if (room === undefined) {
    callback('Success');
    return;
  }
  const clientsToDelete = [];

  room.forEachClient((client) => {
    if (client.user.name === user) {
      clientsToDelete.push(client);
    }
  });

  clientsToDelete.forEach((client) => {
    log.info(`message: deleteUser, user: ${client.user.name}`);
    client.disconnect();
  });

  if (clientsToDelete.length !== 0) {
    callback('Success');
  } else {
    log.error(`mesagge: deleteUser user does not exist, user: ${user}`);
    callback('User does not exist');
  }
};


/*
 * Delete a room.
 */
exports.deleteRoom = (roomId, callback) => {
  log.info(`message: deleteRoom, roomId: ${roomId}`);
  const room = rooms.getRoomById(roomId);

  if (room === undefined) {
    callback('Success');
    return;
  }

  if (!room.p2p) {
    room.forEachClient((client) => {
      client.removeSubscriptions();
    });
    room.streamManager.forEachPublishedStream((stream) => {
      if (stream.hasAudio() || stream.hasVideo() || stream.hasScreen()) {
        room.controller.removePublisher(stream.getID());
      }
    });
    room.streamManager.publishedStreams.clear();
  }

  room.forEachClient((client) => {
    client.channel.disconnect();
  });

  rooms.deleteRoom(roomId);

  updateMyState();
  callback('Success');
};

exports.getContext = () => rooms;

exports.connectionStatusEvent = (clientId, connectionId, info, evt) => {
  log.info('connectionStatusEvent', clientId, connectionId, info, JSON.stringify(evt));
  const room = rooms.getRoomWithClientId(clientId);
  if (room) {
    room.sendConnectionMessageToClient(clientId, connectionId, info, evt);
  }
};

amqper.connect(() => {
  try {
    rooms.on('updated', updateMyState);
    amqper.setPublicRPC(rpcPublic);

    addToCloudHandler(() => {
      const rpcID = `erizoController_${myId}`;
      amqper.bind(rpcID, listen);
    });
  } catch (error) {
    log.info(`message: Error in Erizo Controller, ${logger.objectToLog(error)}`);
  }
});
