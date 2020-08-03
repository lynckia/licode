
const events = require('events');
const logger = require('./../../common/logger').logger;
const ReliableSocket = require('./../../common/ReliableSocket.js').ReliableSocket;
// eslint-disable-next-line import/no-extraneous-dependencies
const uuidv4 = require('uuid/v4');

const log = logger.getLogger('ErizoController - Channel');

const WEBSOCKET_NORMAL_CLOSURE = 1000;
const WEBSOCKET_GOING_AWAY_CLOSURE = 1001;

function listenToSocketHandshakeEvents(channel) {
  channel.reliableSocket.on('reconnected', channel.onReconnected.bind(channel));
  channel.reliableSocket.on('disconnect', channel.onDisconnect.bind(channel));
}

function listenToWebSocketCloseCode(inputChannel) {
  const channel = inputChannel;
  channel.closeCode = WEBSOCKET_NORMAL_CLOSURE;
  const onCloseFunction = this.socket.conn.transport.socket.internalOnClose;
  channel.reliableSocket.socket.conn.transport.socket.internalOnClose = (code, reason) => {
    this.closeCode = code;
    if (onCloseFunction) {
      onCloseFunction(code, reason);
    }
  };
}

const CONNECTED = Symbol('connected');
const RECONNECTING = Symbol('reconnecting');
const DISCONNECTED = Symbol('disconnected');

const RECONNECTION_TIMEOUT = 30000;

class Channel extends events.EventEmitter {
  constructor(socket, token, options) {
    super();
    this.socket = socket;
    this.reliableSocket = new ReliableSocket(this.socket);
    this.state = DISCONNECTED;
    this.id = uuidv4();
    this.token = token;
    this.options = options;

    listenToSocketHandshakeEvents(this);

    listenToWebSocketCloseCode(this);
  }

  onDisconnect(reason) {
    log.info('message: socket disconnected, reason:', reason, ', ',
      logger.objectToLog(this.token));
    if (this.closeCode === WEBSOCKET_NORMAL_CLOSURE ||
        this.closeCode === WEBSOCKET_GOING_AWAY_CLOSURE ||
        reason === 'client namespace disconnect' ||
        reason === 'transport error') {
      this.emit('disconnect');
      this.state = DISCONNECTED;
      return;
    }
    this.state = RECONNECTING;
    this.reconnectionTimeout = setTimeout(() => {
      this.emit('disconnect');
    }, RECONNECTION_TIMEOUT);
  }

  socketOn(eventName, listener) {
    this.reliableSocket.on(eventName, listener);
  }

  socketRemoveListener(eventName, listener) {
    this.reliableSocket.off(eventName, listener);
  }

  setConnected() {
    this.state = CONNECTED;
  }

  isConnected() {
    return this.state === CONNECTED;
  }

  setSocket(socket) {
    this.reliableSocket.setSocket(socket);
    clearTimeout(this.reconnectionTimeout);
    this.state = CONNECTED;
  }

  onReconnected(clientId) {
    this.state = CONNECTED;
    this.emit('reconnected', clientId);
  }

  sendMessage(type, arg) {
    this.reliableSocket.emit(type, arg);
  }

  disconnect() {
    this.state = DISCONNECTED;
    this.reliableSocket.disconnect();
  }
}

exports.Channel = Channel;
