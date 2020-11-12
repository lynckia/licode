/* global window */

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
 * Class Socket represents a client Socket.IO connection to ErizoController.
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
  let pageUnloaded = false;

  const emit = (type, ...args) => {
    that.emit(SocketEvent(type, { args }));
  };

  that.connect = (token, userOptions, callback = defaultCallback, error = defaultCallback) => {
    const query = userOptions;
    Object.assign(userOptions, token);
    // Reconnection Logic: 3 attempts.
    // 1st attempt: 1000 +/-  500ms
    // 2nd attempt: 2000 +/- 1000ms
    // 3rd attempt: 4000 +/- 2000ms

    // Example of a failing reconnection with 3 reconnection Attempts:
    // - connect            // the client successfully establishes a connection to the server
    // - disconnect         // some bad thing happens (the client goes offline, for example)
    // - reconnect_attempt  // after a given delay, the client tries to reconnect
    // - reconnect_error    // the first attempt fails
    // - reconnect_attempt  // after a given delay, the client tries to reconnect
    // - reconnect_error    // the second attempt fails
    // - reconnect_attempt  // after a given delay, the client tries to reconnect
    // - reconnect_error    // the third attempt fails
    // - reconnect_failed   // the client won't try to reconnect anymore

    // Example of a success reconnection:
    // - connect            // the client successfully establishes a connection to the server.
    // - disconnect         // some bad thing happens (the server crashes, for example).
    // - reconnect_attempt  // after a given delay, the client tries to reconnect.
    // - reconnect_error    // the first attempt fails.
    // - reconnect_attempt  // after a given delay, the client tries to reconnect again
    // - connect            // the client successfully restore the connection to the server.
    const options = {
      reconnection: true,
      reconnectionAttempts: 3,
      reconnectionDelay: 1000,
      reconnectionDelayMax: 4000,
      randomizationFactor: 0.5,
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
      log.info(`message: connected, previousState: ${that.state.toString()}, id: ${that.id}`);
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

    reliableSocket.on('connection_failed', (evt) => {
      log.warning(`message: connection failed, id: ${that.id}, evt: ${evt}`);
      emit('connection_failed', evt);
    });

    // Socket.io Internal events
    reliableSocket.on('connect', () => {
      log.info(`message: connect, previousState: ${that.state.toString()}, id: ${that.id}`);
      if (that.state === that.RECONNECTING) {
        log.info(`message: reconnected, id: ${that.id}`);
        that.state = that.CONNECTED;
        emit('reconnected', that.id);
      }
    });

    reliableSocket.on('error', (err) => {
      log.warning(`message: socket error, id: ${that.id}, state: ${that.state.toString()}, error: ${err}`);
      const tokenIssue = 'token: ';
      if (err.startsWith(tokenIssue)) {
        that.state = that.DISCONNECTED;
        error(err.slice(tokenIssue.length));
        reliableSocket.disconnect();
        return;
      }
      if (that.state === that.RECONNECTING) {
        that.state = that.DISCONNECTED;
        reliableSocket.disconnect(true);
        emit('disconnect', err);
        return;
      }
      if (that.state === that.DISCONNECTED) {
        reliableSocket.disconnect(true);
        return;
      }
      emit('error');
    });

    // The socket has disconnected
    reliableSocket.on('disconnect', (reason) => {
      const pendingMessages = reliableSocket.getNumberOfPending();
      log.info(`message: disconnect, id: ${that.id}, reason: ${reason}, closeCode: ${closeCode}, pending: ${pendingMessages}`);
      if (that.clientInitiated) {
        that.state = that.DISCONNECTED;
        if (!pageUnloaded) {
          emit('disconnect', reason);
        }
        reliableSocket.disconnect(true);
      } else {
        that.state = that.RECONNECTING;
        emit('reconnecting', `reason: ${reason}, pendingMessages: ${pendingMessages}`);
      }
    });

    reliableSocket.on('connect_error', (err) => {
      // This can be thrown during reconnection attempts too
      log.warning(`message: connect error, id: ${that.id}, error: ${err.message}`);
    });

    reliableSocket.on('connect_timeout', (err) => {
      log.warning(`message: connect timeout, id: ${that.id}, error: ${err.message}`);
    });

    reliableSocket.on('reconnecting', (attemptNumber) => {
      log.info(`message: reconnecting, id: ${that.id}, attempt: ${attemptNumber}`);
    });

    reliableSocket.on('reconnect', (attemptNumber) => {
      // Underlying WS has been reconnected, but we still need to wait for the 'connect' message.
      log.info(`message: internal ws reconnected, id: ${that.id}, attempt: ${attemptNumber}`);
    });

    reliableSocket.on('reconnect_attempt', (attemptNumber) => {
      // We are starting a new reconnection attempt, so we will update the query to let
      // ErizoController know that the new socket is a reconnection attempt.
      log.debug(`message: reconnect attempt, id: ${that.id}, attempt: ${attemptNumber}`);
      query.clientId = that.id;
      socket.io.opts.query = query;
    });

    reliableSocket.on('reconnect_error', (err) => {
      // The last reconnection attempt failed.
      log.info(`message: error reconnecting, id: ${that.id}, error: ${err.message}`);
    });

    reliableSocket.on('reconnect_failed', () => {
      // We could not reconnect after all attempts.
      log.info(`message: reconnect failed, id: ${that.id}`);
      that.state = that.DISCONNECTED;
      emit('disconnect', 'reconnect failed');
      reliableSocket.disconnect(true);
    });
  };

  const onBeforeUnload = (evtIn) => {
    const evt = evtIn;
    if (that.state === that.DISCONNECTED) {
      return;
    }
    evt.preventDefault();
    delete evt.returnValue;
    pageUnloaded = true;
    that.disconnect(true);
  };

  that.disconnect = (clientInitiated) => {
    log.warning(`message: disconnect, id: ${that.id}, clientInitiated: ${clientInitiated}, state: ${that.state.toString()}`);
    that.state = that.DISCONNECTED;
    that.clientInitiated = clientInitiated;
    if (clientInitiated) {
      reliableSocket.emit('clientDisconnection');
    }
    reliableSocket.disconnect();
    window.removeEventListener('beforeunload', onBeforeUnload);
  };

  window.addEventListener('beforeunload', onBeforeUnload);

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
