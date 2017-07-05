/* global io, L, Erizo*/
this.Erizo = this.Erizo || {};

Erizo.SocketEvent = (type, specInput) => {
  const that = Erizo.LicodeEvent({ type });
  that.args = specInput.args;
  return that;
};

/*
 * Class Stream represents a local or a remote Stream in the Room. It will handle the WebRTC
 * stream and identify the stream and where it should be drawn.
 */
Erizo.Socket = () => {
  const that = Erizo.EventDispatcher();
  const defaultCallback = () => {};

  that.CONNECTED = Symbol('connected');
  that.DISCONNECTED = Symbol('disconnected');

  that.state = that.DISCONNECTED;

  let socket;

  const emit = (type, ...args) => {
    that.emit(Erizo.SocketEvent(type, { args }));
  };

  that.connect = (token, callback = defaultCallback, error = defaultCallback) => {
    const options = {
      reconnect: false,
      secure: token.secure,
      forceNew: true,
      transports: ['websocket'],
      rejectUnauthorized: false,
    };
    socket = io.connect(token.host, options);

    socket.on('onAddStream', emit.bind(that, 'onAddStream'));

    socket.on('signaling_message_erizo', emit.bind(that, 'signaling_message_erizo'));
    socket.on('signaling_message_peer', emit.bind(that, 'signaling_message_peer'));
    socket.on('publish_me', emit.bind(that, 'publish_me'));
    socket.on('onBandwidthAlert', emit.bind(that, 'onBandwidthAlert'));

    // We receive an event of new data in one of the streams
    socket.on('onDataStream', emit.bind(that, 'onDataStream'));

    // We receive an event of new data in one of the streams
    socket.on('onUpdateAttributeStream', emit.bind(that, 'onUpdateAttributeStream'));

    // We receive an event of a stream removed from the room
    socket.on('onRemoveStream', emit.bind(that, 'onRemoveStream'));

    // The socket has disconnected
    socket.on('disconnect', emit.bind(that, 'disconnect'));

    socket.on('connection_failed', emit.bind(that, 'connection_failed'));
    socket.on('error', emit.bind(that, 'error'));

    // First message with the token
    that.sendMessage('token', token, (...args) => {
      that.state = that.CONNECTED;
      callback(...args);
    }, error);
  };

  // Function to send a message to the server using socket.io
  that.sendMessage = (type, msg, callback = defaultCallback, error = defaultCallback) => {
    socket.emit(type, msg, (respType, resp) => {
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
      L.Logger.warning('Trying to send a message over a disconnected Socket');
      return;
    }
    socket.emit(type, options, sdp, (response, respCallback) => {
      callback(response, respCallback);
    });
  };
  return that;
};
