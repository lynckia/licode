/* global */

import io from '../lib/socket.io';
import Logger from './utils/Logger';
import { ReliableSocket } from '../../common/ReliableSocket';

import { EventDispatcher, LicodeEvent } from './Events';

const log = Logger.module('Socket');

const SocketEvent = (type, specInput) => {
  const that = LicodeEvent({ type });
  that.args = specInput.args;
  return that;
};

/*
 * Class Stream represents a local or a remote Stream in the Room. It will handle the WebRTC
 * stream and identify the stream and where it should be drawn.
 */
const Socket = (newIo) => {
  const that = EventDispatcher();
  const defaultCallback = () => {};

  that.CONNECTED = Symbol('connected');
  that.RECONNECTING = Symbol('reconnecting');
  that.DISCONNECTED = Symbol('disconnected');

  const WEBSOCKET_NORMAL_CLOSURE = 1000;
  that.state = that.DISCONNECTED;
  that.IO = newIo === undefined ? io : newIo;

  let socket;
  let reliableSocket;

  const emit = (type, ...args) => {
    that.emit(SocketEvent(type, { args }));
  };


  that.connect = (token, userOptions, callback = defaultCallback, error = defaultCallback) => {
    const query = userOptions;
    Object.assign(userOptions, token);
    const options = {
      reconnection: true,
      reconnectionAttempts: 3,
      secure: token.secure,
      forceNew: true,
      transports: ['websocket'],
      rejectUnauthorized: false,
      query,
    };
    const transport = token.secure ? 'wss://' : 'ws://';
    const host = token.host;
    socket = that.IO.connect(transport + host, options);
    reliableSocket = new ReliableSocket(socket);

    that.reliableSocket = reliableSocket;

    // Hack to know the exact reason of the WS closure (socket.io does not publish it)
    let closeCode = WEBSOCKET_NORMAL_CLOSURE;
    const socketOnCloseFunction = socket.io.engine.transport.ws.onclose;
    socket.io.engine.transport.ws.onclose = (closeEvent) => {
      log.info(`message: WebSocket closed, code: ${closeEvent.code}, id: ${that.id}`);
      closeCode = closeEvent.code;
      socketOnCloseFunction(closeEvent);
    };

    reliableSocket.on('connected', (response) => {
      that.state = that.CONNECTED;
      that.id = response.clientId;
      callback(response);
    });

    reliableSocket.on('onAddStream', emit.bind(that, 'onAddStream'));

    reliableSocket.on('stream_message_erizo', emit.bind(that, 'stream_message_erizo'));
    reliableSocket.on('stream_message_p2p', emit.bind(that, 'stream_message_p2p'));
    reliableSocket.on('connection_message_erizo', emit.bind(that, 'connection_message_erizo'));
    reliableSocket.on('publish_me', emit.bind(that, 'publish_me'));
    reliableSocket.on('unpublish_me', emit.bind(that, 'unpublish_me'));
    reliableSocket.on('onBandwidthAlert', emit.bind(that, 'onBandwidthAlert'));

    // We receive an event of new data in one of the streams
    reliableSocket.on('onDataStream', emit.bind(that, 'onDataStream'));

    // We receive an event of new data in one of the streams
    reliableSocket.on('onUpdateAttributeStream', emit.bind(that, 'onUpdateAttributeStream'));

    // We receive an event of a stream removed from the room
    reliableSocket.on('onRemoveStream', emit.bind(that, 'onRemoveStream'));

    reliableSocket.on('onAutomaticStreamsSubscription', emit.bind(that, 'onAutomaticStreamsSubscription'));

    // The socket has disconnected
    reliableSocket.on('disconnect', (reason) => {
      log.info(`message: disconnect, id: ${that.id}, reason: ${reason}`);
      if (reason === 'io server disconnect' ||
          closeCode === WEBSOCKET_NORMAL_CLOSURE) {
        emit('disconnect', reason);
        reliableSocket.close();
      }
    });

    reliableSocket.on('connection_failed', (evt) => {
      log.warning(`message: connection failed, id: ${that.id}`);
      emit('connection_failed', evt);
    });
    reliableSocket.on('error', (err) => {
      log.warning(`message: socket error, id: ${that.id}, error: ${err}`);
      const tokenIssue = 'token: ';
      if (err.startsWith(tokenIssue)) {
        error(err.slice(tokenIssue.length));
        reliableSocket.disconnect();
        return;
      }
      emit('error');
    });

    reliableSocket.on('connect_error', (err) => {
      log.warning(`message: connect error, id: ${that.id}, error: ${err.message}`);
      error(err);
    });

    reliableSocket.on('connect_timeout', (err) => {
      log.warning(`message: connect timeout, id: ${that.id}, error: ${err.message}`);
    });

    reliableSocket.on('reconnecting', (attemptNumber) => {
      log.info(`message: reconnecting, id: ${that.id}, attempt: ${attemptNumber}`);
    });

    reliableSocket.on('reconnect', (attemptNumber) => {
      log.info(`message: reconnected, id: ${that.id}, attempt: ${attemptNumber}`);
    });

    reliableSocket.on('reconnect_attempt', (attemptNumber) => {
      log.debug(`message: reconnect attempt, id: ${that.id}, attempt: ${attemptNumber}`);
      query.clientId = that.id;
      socket.io.opts.query = query;
    });

    reliableSocket.on('reconnect_error', (err) => {
      log.info(`message: error reconnecting, id: ${that.id}, error: ${err.message}`);
    });

    reliableSocket.on('reconnect_failed', () => {
      log.info(`message: reconnect failed, id: ${that.id}`);
      that.state = that.DISCONNECTED;
      emit('disconnect', 'reconnect failed');
    });
  };

  that.disconnect = () => {
    that.state = that.DISCONNECTED;
    reliableSocket.disconnect();
  };

  // Function to send a message to the server using socket.io
  that.sendMessage = (type, msg, callback = defaultCallback, error = defaultCallback) => {
    if (that.state === that.DISCONNECTED) {
      log.debug(`message: Trying to send a message over a disconnected Socket, id: ${that.id}, type: ${type}`);
      return;
    }
    reliableSocket.emit(type, msg, (respType, resp) => {
      if (respType === 'success') {
        callback(resp);
      } else if (respType === 'error') {
        error(resp);
      } else {
        callback(respType, resp);
      }
    });
  };

  // It sends a SDP message to the server using socket.io
  that.sendSDP = (type, options, sdp, callback = defaultCallback) => {
    if (that.state === that.DISCONNECTED) {
      log.warning(`message: Trying to send a message over a disconnected Socket, id: ${that.id}`);
      return;
    }
    reliableSocket.emit(type, { options, sdp }, (...args) => {
      callback(...args);
    });
  };
  return that;
};

export { SocketEvent, Socket };
