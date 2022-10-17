/* global Error */
const crypto = require('crypto');
const Channel = require('./models/Channel').Channel;
const logger = require('./../common/logger').logger;
const amqper = require('./../common/amqper');
const nuve = require('./nuveProxy').NuveProxy({ amqper });

// Logger
const log = logger.getLogger('TokenAuthenticator');

const NUVE_KEY = global.config.nuve.superserviceKey;

const returnError = (next, message, socket, isToken = true) => {
  const fullMessage = isToken ? `token: ${message}` : message;
  const err = new Error(fullMessage);
  next(err);
  // We need to wait a bit to disconnect because otherwise it won't send
  // the error message
  process.nextTick(() => {
    socket.disconnect();
  });
};

const calculateSignature = (token, key) => {
  const toSign = `${token.tokenId},${token.host}`;
  const signed = crypto.createHmac('sha1', key).update(toSign).digest('hex');
  return (new Buffer(signed)).toString('base64');
};

const checkSignature = (token, key) => {
  const calculatedSignature = calculateSignature(token, key);

  if (calculatedSignature !== token.signature) {
    log.info('message: invalid token signature');
    return false;
  }
  return true;
};


const checkToken = (token, success, fail) => {
  log.debug('message: token received, ', logger.objectToLog(token));
  if (token && checkSignature(token, NUVE_KEY)) {
    nuve.deleteToken(token.tokenId).then((tokenDB) => {
      if (token.host === tokenDB.host) {
        success(tokenDB, token);
      } else {
        log.warn(`message: Token has invalid host, clientId: ${this.id}, `,
          logger.objectToLog(token));
        fail('Invalid host');
      }
    }).catch((reason) => {
      if (reason === 'error') {
        log.warn('message: Trying to use token that does not exist - ' +
                   `disconnecting Client, clientId: ${this.id}`,
        logger.objectToLog(token));
        fail('Token does not exist');
      } else if (reason === 'timeout') {
        log.warn('message: Nuve does not respond token check - ' +
                   `disconnecting client, clientId: ${this.id}`,
        logger.objectToLog(token));
        fail('Nuve does not respond');
      }
    });
  } else {
    log.warn(`message: Token authentication error, clientId: ${this.id}`,
      logger.objectToLog(token));
    fail('Authentication error');
  }
};


const authenticateWithToken = (socketIn, tokenIn, next) => {
  const socket = socketIn;
  checkToken(tokenIn, (tokenDB, token) => {
    log.info('message: token checked,', logger.objectToLog(tokenDB));
    const channel = new Channel(socket, tokenDB, token);
    socket.channel = channel;
    next();
  }, (failMessage) => {
    returnError(next, failMessage, socket);
  });
};

const authenticateReconnection = (socketIn, clientId, rooms, next) => {
  const socket = socketIn;
  let client;
  rooms.forEachRoom((room) => {
    if (room.hasClientWithId(clientId)) {
      client = room.getClientById(clientId);
    }
  });
  if (client !== undefined) {
    client.getChannel().setSocket(socket);
    socket.channel = client.getChannel();
    next();
  } else {
    log.info(`message: Client reconnected not found, clientId: ${clientId}`);
    returnError(next, 'Connection already closed', socket, false);
  }
};

const authenticate = (rooms, socketIn, next) => {
  try {
    const socket = socketIn;
    if (!socket.handshake.query) {
      returnError(next, 'No token information', socket);
      return;
    }
    const token = socket.handshake.query;
    const clientId = token.clientId;
    if (clientId) {
      log.info(`message: looking for client reconnected, clientId: ${clientId}`);
      authenticateReconnection(socket, clientId, rooms, next);
    } else {
      authenticateWithToken(socket, token, next);
    }
  } catch (e) {
    returnError(next, 'Internal error authenticating request', socketIn);
  }
};

module.exports = authenticate;
