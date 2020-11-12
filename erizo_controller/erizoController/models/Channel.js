
const events = require('events');
const logger = require('./../../common/logger').logger;
const ReliableSocket = require('./../../common/ReliableSocket.js').ReliableSocket;
// eslint-disable-next-line import/no-extraneous-dependencies
const uuidv4 = require('uuid/v4');

const log = logger.getLogger('ErizoController - Channel');

const WEBSOCKET_NORMAL_CLOSURE = 1000;

function listenToSocketHandshakeEvents(channel) {
  channel.reliableSocket.on('disconnect', channel.onDisconnect.bind(channel));
}

function listenToWebSocketCloseCode(inputChannel) {
  const channel = inputChannel;
  channel.closeCode = WEBSOCKET_NORMAL_CLOSURE;
  const onCloseFunction = channel.socket.conn.transport.socket.internalOnClose;
  channel.socket.conn.transport.socket.internalOnClose = (code, reason) => {
    channel.closeCode = code;
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
    this.clientWillDisconnectReceived = false;

    listenToSocketHandshakeEvents(this);

    listenToWebSocketCloseCode(this);
  }

  onDisconnect(reason) {
    log.info('message: socket disconnected, reason:', reason, ', closeCode: ', this.closeCode,
      ', waitReconnection: ', !this.clientWillDisconnectReceived, ', id: ', this.id, ', ',
      logger.objectToLog(this.token));
    if (this.clientWillDisconnectReceived) {
      this.emit('disconnect');
      this.state = DISCONNECTED;
      return;
    }
    this.state = RECONNECTING;
    this.reconnectionTimeout = setTimeout(() => {
      log.info('message: socket reconnection timeout, id: ', this.id, ', ',
        logger.objectToLog(this.token));
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
    const pendingMessages = this.reliableSocket.getNumberOfPending();
    this.reliableSocket.setSocket(socket);
    clearTimeout(this.reconnectionTimeout);
    log.info('mesage: socket reconnected, id: ', this.id, ', pending: ', pendingMessages, ', ', logger.objectToLog(this.token));
    this.state = CONNECTED;
  }

  sendMessage(type, arg) {
    this.reliableSocket.emit(type, arg);
  }

  clientWillDisconnect() {
    log.info('mesage: client will disconnect', ', id: ', this.id, ', ', logger.objectToLog(this.token));
    this.clientWillDisconnectReceived = true;
  }

  disconnect() {
    this.state = DISCONNECTED;
    this.reliableSocket.disconnect();
  }
}

exports.Channel = Channel;
