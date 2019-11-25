
const events = require('events');
const logger = require('./../../common/logger').logger;
const crypto = require('crypto');
// eslint-disable-next-line import/no-extraneous-dependencies
const uuidv4 = require('uuid/v4');

const log = logger.getLogger('ErizoController - Channel');

const NUVE_KEY = global.config.nuve.superserviceKey;

const RECONNECTION_TIMEOUT = 10000;

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

function listenToSocketHandshakeEvents(channel) {
  channel.socket.on('token', channel.onToken.bind(channel));
  channel.socket.on('reconnected', channel.onReconnected.bind(channel));
  channel.socket.on('disconnect', channel.onDisconnect.bind(channel));
}

const CONNECTED = Symbol('connected');
const RECONNECTING = Symbol('reconnecting');
const DISCONNECTED = Symbol('disconnected');

const WEBSOCKET_NORMAL_CLOSURE = 1000;
const WEBSOCKET_GOING_AWAY_CLOSURE = 1001;

class Channel extends events.EventEmitter {
  constructor(socket, nuve) {
    super();
    this.socket = socket;
    this.nuve = nuve;
    this.state = DISCONNECTED;
    this.messageBuffer = [];
    this.id = uuidv4();

    // Hack to know the exact reason of the WS closure (socket.io does not publish it)
    this.closeCode = WEBSOCKET_NORMAL_CLOSURE;
    const onCloseFunction = this.socket.conn.transport.socket.internalOnClose;
    this.socket.conn.transport.socket.internalOnClose = (code, reason) => {
      this.closeCode = code;
      if (onCloseFunction) {
        onCloseFunction(code, reason);
      }
    };
    listenToSocketHandshakeEvents(this);
  }

  onToken(options, callback) {
    const token = options.token;
    log.debug('message: token received');
    if (token && checkSignature(token, NUVE_KEY)) {
      this.nuve.deleteToken(token.tokenId).then((tokenDB) => {
        if (token.host === tokenDB.host) {
          this.state = CONNECTED;
          this.emit('connected', tokenDB, options, callback);
        } else {
          log.warn(`message: Token has invalid host, clientId: ${this.id}`);
          callback('error', 'Invalid host');
          this.disconnect();
        }
      }).catch((reason) => {
        if (reason === 'error') {
          log.warn('message: Trying to use token that does not exist - ' +
                     `disconnecting Client, clientId: ${this.id}`);
          callback('error', 'Token does not exist');
          this.disconnect();
        } else if (reason === 'timeout') {
          log.warn('message: Nuve does not respond token check - ' +
                     `disconnecting client, clientId: ${this.id}`);
          callback('error', 'Nuve does not respond');
          this.disconnect();
        }
      });
    } else {
      log.warn(`message: Token authentication error, clientId: ${this.id}`);
      callback('error', 'Authentication error');
      this.disconnect();
    }
  }

  onDisconnect() {
    log.debug('message: socket disconnected, code:', this.closeCode);
    if (this.closeCode !== WEBSOCKET_NORMAL_CLOSURE &&
        this.closeCode !== WEBSOCKET_GOING_AWAY_CLOSURE) {
      this.state = RECONNECTING;
      this.disconnecting = setTimeout(() => {
        this.emit('disconnect');
        this.state = DISCONNECTED;
      }, RECONNECTION_TIMEOUT);
      return;
    }
    this.state = DISCONNECTED;
    this.emit('disconnect');
  }

  socketOn(eventName, listener) {
    this.socket.on(eventName, listener);
  }

  socketRemoveListener(eventName, listener) {
    this.socket.removeListener(eventName, listener);
  }

  onReconnected(clientId) {
    this.state = CONNECTED;
    this.emit('reconnected', clientId);
  }

  sendMessage(type, arg) {
    if (this.state === RECONNECTING) {
      this.addToBuffer(type, arg);
      return;
    }
    this.socket.emit(type, arg);
  }

  addToBuffer(type, arg) {
    this.messageBuffer.push([type, arg]);
  }

  getBuffer() {
    return this.messageBuffer;
  }

  sendBuffer(buffer) {
    if (this.state !== CONNECTED) {
      return;
    }
    log.debug('message: sending buffered messages, number:', buffer.length,
      ', channelId:', this.id);
    buffer.forEach((message) => {
      log.debug('message: sending buffered message, message:', message, ', channelId:', this.id);
      this.sendMessage(...message);
    });
  }

  disconnect() {
    this.state = DISCONNECTED;
    clearTimeout(this.disconnecting);
    this.socket.disconnect();
  }
}

exports.Channel = Channel;
