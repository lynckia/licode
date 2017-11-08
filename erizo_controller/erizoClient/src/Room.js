
import Connection from './Connection';
import { EventDispatcher, StreamEvent, RoomEvent } from './Events';
import { Socket } from './Socket';
import Stream from './Stream';
import ErizoMap from './utils/ErizoMap';
import Base64 from './utils/Base64';
import Logger from './utils/Logger';

/*
 * Class Room represents a Licode Room. It will handle the connection, local stream publication and
 * remote stream subscription.
 * Typical Room initialization would be:
 * var room = Erizo.Room({token:'213h8012hwduahd-321ueiwqewq'});
 * It also handles RoomEvents and StreamEvents. For example:
 * Event 'room-connected' points out that the user has been successfully connected to the room.
 * Event 'room-disconnected' shows that the user has been already disconnected.
 * Event 'stream-added' indicates that there is a new stream available in the room.
 * Event 'stream-removed' shows that a previous available stream has been removed from the room.
 */
const Room = (altIo, altConnection, specInput) => {
  const spec = specInput;
  const that = EventDispatcher(specInput);
  const DISCONNECTED = 0;
  const CONNECTING = 1;
  const CONNECTED = 2;

  that.remoteStreams = ErizoMap();
  that.localStreams = ErizoMap();
  that.roomID = '';
  that.state = DISCONNECTED;
  that.p2p = false;
  that.Connection = altConnection === undefined ? Connection : altConnection;

  let socket = Socket(altIo);
  that.socket = socket;
  let remoteStreams = that.remoteStreams;
  let localStreams = that.localStreams;

  // Private functions
  const removeStream = (streamInput) => {
    const stream = streamInput;
    if (stream.stream) {
      // Remove HTML element
      stream.hide();

      stream.stop();
      stream.close();
      delete stream.stream;
    }

    // Close PC stream
    if (stream.pc) {
      if (stream.local && that.p2p) {
        stream.pc.forEach((connection, id) => {
          connection.close();
          stream.pc.remove(id);
        });
      } else {
        stream.pc.close();
        delete stream.pc;
      }
    }
  };

  const onStreamFailed = (streamInput, message) => {
    const stream = streamInput;
    if (that.state !== DISCONNECTED && stream && !stream.failed) {
      stream.failed = true;
      const streamFailedEvt = StreamEvent(
        { type: 'stream-failed',
          msg: message || 'Stream failed after connection',
          stream });
      that.dispatchEvent(streamFailedEvt);
      if (stream.local) {
        that.unpublish(stream);
      } else {
        that.unsubscribe(stream);
      }
    }
  };

  const dispatchStreamSubscribed = (streamInput, evt) => {
    const stream = streamInput;
    // Draw on html
    Logger.info('Stream subscribed');
    stream.stream = evt.stream;
    const evt2 = StreamEvent({ type: 'stream-subscribed', stream });
    that.dispatchEvent(evt2);
  };

  const getP2PConnectionOptions = (stream, peerSocket) => {
    const options = {
      callback(msg) {
        socket.sendSDP('signaling_message', {
          streamId: stream.getID(),
          peerSocket,
          msg });
      },
      audio: stream.hasAudio(),
      video: stream.hasVideo(),
      iceServers: that.iceServers,
      maxAudioBW: stream.maxAudioBW,
      maxVideoBW: stream.maxVideoBW,
      limitMaxAudioBW: spec.maxAudioBW,
      limitMaxVideoBW: spec.maxVideoBW,
      forceTurn: stream.forceTurn,
    };
    return options;
  };

  const createRemoteStreamP2PConnection = (streamInput, peerSocket) => {
    const stream = streamInput;
    stream.pc = that.Connection.buildConnection(getP2PConnectionOptions(stream, peerSocket));

    stream.pc.onaddstream = dispatchStreamSubscribed.bind(null, stream);
    stream.pc.oniceconnectionstatechange = (state) => {
      if (state === 'failed') {
        onStreamFailed(stream);
      }
    };
  };

  const createLocalStreamP2PConnection = (streamInput, peerSocket) => {
    const stream = streamInput;
    if (stream.pc === undefined) {
      stream.pc = ErizoMap();
    }

    const connection = that.Connection.buildConnection(getP2PConnectionOptions(stream, peerSocket));

    stream.pc.add(peerSocket, connection);

    connection.oniceconnectionstatechange = (state) => {
      if (state === 'failed') {
        stream.pc.get(peerSocket).close();
        stream.pc.remove(peerSocket);
      }
    };
    connection.addStream(stream.stream);
    connection.createOffer();
  };

  const removeLocalStreamP2PConnection = (streamInput, peerSocket) => {
    const stream = streamInput;
    if (stream.pc === undefined || !stream.pc.has(peerSocket)) {
      return;
    }
    const pc = stream.pc.get(peerSocket);
    pc.close();
    stream.pc.remove(peerSocket);
  };

  const getErizoConnectionOptions = (stream, options, isRemote) => {
    const connectionOpts = {
      callback(message) {
        Logger.info('Sending message', message);
        socket.sendSDP('signaling_message', {
          streamId: stream.getID(),
          msg: message,
          browser: stream.pc.browser }, undefined, () => {});
      },
      nop2p: true,
      audio: options.audio && stream.hasAudio(),
      video: options.video && stream.hasVideo(),
      maxAudioBW: options.maxAudioBW,
      maxVideoBW: options.maxVideoBW,
      limitMaxAudioBW: spec.maxAudioBW,
      limitMaxVideoBW: spec.maxVideoBW,
      iceServers: that.iceServers,
      forceTurn: stream.forceTurn };
    if (!isRemote) {
      connectionOpts.simulcast = options.simulcast;
    }
    return connectionOpts;
  };

  const createRemoteStreamErizoConnection = (streamInput, options) => {
    const stream = streamInput;
    stream.pc = that.Connection.buildConnection(getErizoConnectionOptions(stream, options, true));

    stream.pc.onaddstream = dispatchStreamSubscribed.bind(null, stream);
    stream.pc.oniceconnectionstatechange = (state) => {
      if (state === 'failed') {
        onStreamFailed(stream);
      }
    };

    stream.pc.createOffer(true);
  };

  const createLocalStreamErizoConnection = (streamInput, options) => {
    const stream = streamInput;
    stream.pc = that.Connection.buildConnection(getErizoConnectionOptions(stream, options));

    stream.pc.addStream(stream.stream);
    stream.pc.oniceconnectionstatechange = (state) => {
      if (state === 'failed') {
        onStreamFailed(stream);
      }
    };
    if (!options.createOffer) { stream.pc.createOffer(); }
  };

  // We receive an event with a new stream in the room.
  // type can be "media" or "data"

  const socketOnAddStream = (arg) => {
    const stream = Stream(that.Connection, { streamID: arg.id,
      local: false,
      audio: arg.audio,
      video: arg.video,
      data: arg.data,
      screen: arg.screen,
      attributes: arg.attributes });
    stream.room = that;
    remoteStreams.add(arg.id, stream);
    const evt = StreamEvent({ type: 'stream-added', stream });
    that.dispatchEvent(evt);
  };

  const socketOnErizoMessage = (arg) => {
    let stream;
    if (arg.peerId) {
      stream = remoteStreams.get(arg.peerId);
    } else {
      stream = localStreams.get(arg.streamId);
    }

    if (stream && !stream.failed) {
      stream.pc.processSignalingMessage(arg.mess);
    }
  };

  const socketOnPeerMessage = (arg) => {
    let stream = localStreams.get(arg.streamId);

    if (stream && !stream.failed) {
      stream.pc.get(arg.peerSocket).processSignalingMessage(arg.msg);
    } else {
      stream = remoteStreams.get(arg.streamId);

      if (!stream.pc) {
        createRemoteStreamP2PConnection(stream, arg.peerSocket);
      }
      stream.pc.processSignalingMessage(arg.msg);
    }
  };

  const socketOnPublishMe = (arg) => {
    const myStream = localStreams.get(arg.streamId);

    createLocalStreamP2PConnection(myStream, arg.peerSocket);
  };

  const socketOnUnpublishMe = (arg) => {
    const myStream = localStreams.get(arg.streamId);
    if (myStream) {
      removeLocalStreamP2PConnection(myStream, arg.peerSocket);
    }
  };

  const socketOnBandwidthAlert = (arg) => {
    Logger.info('Bandwidth Alert on', arg.streamID, 'message',
                        arg.message, 'BW:', arg.bandwidth);
    if (arg.streamID) {
      const stream = remoteStreams.get(arg.streamID);
      if (stream && !stream.failed) {
        const evt = StreamEvent({ type: 'bandwidth-alert',
          stream,
          msg: arg.message,
          bandwidth: arg.bandwidth });
        stream.dispatchEvent(evt);
      }
    }
  };

  // We receive an event of new data in one of the streams
  const socketOnDataStream = (arg) => {
    const stream = remoteStreams.get(arg.id);
    const evt = StreamEvent({ type: 'stream-data', msg: arg.msg, stream });
    stream.dispatchEvent(evt);
  };

  // We receive an event of new data in one of the streams
  const socketOnUpdateAttributeStream = (arg) => {
    const stream = remoteStreams.get(arg.id);
    const evt = StreamEvent({ type: 'stream-attributes-update',
      attrs: arg.attrs,
      stream });
    stream.updateLocalAttributes(arg.attrs);
    stream.dispatchEvent(evt);
  };

  // We receive an event of a stream removed from the room
  const socketOnRemoveStream = (arg) => {
    let stream = localStreams.get(arg.id);
    if (stream) {
      onStreamFailed(stream);
      return;
    }
    stream = remoteStreams.get(arg.id);
    if (stream) {
      remoteStreams.remove(arg.id);
      removeStream(stream);
      const evt = StreamEvent({ type: 'stream-removed', stream });
      that.dispatchEvent(evt);
    }
  };

  // The socket has disconnected
  const socketOnDisconnect = () => {
    Logger.info('Socket disconnected, lost connection to ErizoController');
    if (that.state !== DISCONNECTED) {
      Logger.error('Unexpected disconnection from ErizoController');
      const disconnectEvt = RoomEvent({ type: 'room-disconnected',
        message: 'unexpected-disconnection' });
      that.dispatchEvent(disconnectEvt);
    }
  };

  const socketOnICEConnectionFailed = (arg) => {
    let stream;
    if (!arg.streamId) {
      return;
    }
    const message = `ICE Connection Failed on ${arg.type} ${arg.streamId} ${that.state}`;
    Logger.error(message);
    if (arg.type === 'publish') {
      stream = localStreams.get(arg.streamId);
    } else {
      stream = remoteStreams.get(arg.streamId);
    }
    onStreamFailed(stream, message);
  };

  const socketOnError = (e) => {
    Logger.error('Cannot connect to erizo Controller');
    const connectEvt = RoomEvent({ type: 'room-error', message: e });
    that.dispatchEvent(connectEvt);
  };

  const sendDataSocketFromStreamEvent = (evt) => {
    const stream = evt.stream;
    const msg = evt.msg;
    if (stream.local) {
      socket.sendMessage('sendDataStream', { id: stream.getID(), msg });
    } else {
      Logger.error('You can not send data through a remote stream');
    }
  };

  const updateAttributesFromStreamEvent = (evt) => {
    const stream = evt.stream;
    const attrs = evt.attrs;
    if (stream.local) {
      stream.updateLocalAttributes(attrs);
      socket.sendMessage('updateStreamAttributes', { id: stream.getID(), attrs });
    } else {
      Logger.error('You can not update attributes in a remote stream');
    }
  };

  const socketEventToArgs = (func, event) => {
    if (event.args) {
      func(...event.args);
    } else {
      func();
    }
  };

  const createSdpConstraints = (type, stream, options) => ({
    state: type,
    data: stream.hasData(),
    audio: stream.hasAudio(),
    video: stream.hasVideo(),
    screen: stream.hasScreen(),
    attributes: stream.getAttributes(),
    metadata: options.metadata,
    createOffer: options.createOffer,
    muteStream: options.muteStream,
  });

  const populateStreamFunctions = (id, streamInput, error, callback = () => {}) => {
    const stream = streamInput;
    if (id === null) {
      Logger.error('Error when publishing the stream', error);
      // Unauth -1052488119
      // Network -5
      callback(undefined, error);
      return;
    }
    Logger.info('Stream published');
    stream.getID = () => id;
    stream.on('internal-send-data', sendDataSocketFromStreamEvent);
    stream.on('internal-set-attributes', updateAttributesFromStreamEvent);
    localStreams.add(id, stream);
    stream.room = that;
    callback(id);
  };

  const publishExternal = (streamInput, options, callback = () => {}) => {
    const stream = streamInput;
    let type;
    let arg;
    if (stream.url) {
      type = 'url';
      arg = stream.url;
    } else {
      type = 'recording';
      arg = stream.recording;
    }
    Logger.info('Checking publish options for', stream.getID());
    stream.checkOptions(options);
    socket.sendSDP('publish', createSdpConstraints(type, stream, options), arg,
      (id, error) => {
        populateStreamFunctions(id, stream, error, callback);
      });
  };

  const publishP2P = (streamInput, options, callback = () => {}) => {
    const stream = streamInput;
    // We save them now to be used when actually publishing in P2P mode.
    stream.maxAudioBW = options.maxAudioBW;
    stream.maxVideoBW = options.maxVideoBW;
    socket.sendSDP('publish', createSdpConstraints('p2p', stream, options), undefined, (id, error) => {
      populateStreamFunctions(id, stream, error, callback);
    });
  };

  const publishData = (streamInput, options, callback = () => {}) => {
    const stream = streamInput;
    socket.sendSDP('publish', createSdpConstraints('data', stream, options), undefined, (id, error) => {
      populateStreamFunctions(id, stream, error, callback);
    });
  };

  const publishErizo = (streamInput, options, callback = () => {}) => {
    const stream = streamInput;
    Logger.info('Publishing to Erizo Normally, is createOffer', options.createOffer);
    const constraints = createSdpConstraints('erizo', stream, options);
    constraints.minVideoBW = options.minVideoBW;
    constraints.scheme = options.scheme;

    socket.sendSDP('publish', constraints, undefined, (id, error) => {
      populateStreamFunctions(id, stream, error, undefined);
      createLocalStreamErizoConnection(stream, options);
      callback(id);
    });
  };

  const getVideoConstraints = (stream, video) => {
    const hasVideo = video && stream.hasVideo();
    const width = video && video.width;
    const height = video && video.height;
    const frameRate = video && video.frameRate;
    if (width || height || frameRate) {
      return { width, height, frameRate };
    }
    return hasVideo;
  };

  const subscribeErizo = (streamInput, optionsInput, callback = () => {}) => {
    const stream = streamInput;
    const options = optionsInput;
    options.maxVideoBW = options.maxVideoBW || spec.defaultVideoBW;
    if (options.maxVideoBW > spec.maxVideoBW) {
      options.maxVideoBW = spec.maxVideoBW;
    }
    options.audio = (options.audio === undefined) ? true : options.audio;
    options.video = (options.video === undefined) ? true : options.video;
    options.data = (options.data === undefined) ? true : options.data;
    stream.checkOptions(options);
    const constraint = { streamId: stream.getID(),
      audio: options.audio && stream.hasAudio(),
      video: getVideoConstraints(stream, options.video),
      data: options.data && stream.hasData(),
      browser: that.Connection.getBrowser(),
      createOffer: options.createOffer,
      metadata: options.metadata,
      muteStream: options.muteStream,
      slideShowMode: options.slideShowMode };
    socket.sendSDP('subscribe', constraint, undefined, (result, error) => {
      if (result === null) {
        Logger.error('Error subscribing to stream ', error);
        callback(undefined, error);
        return;
      }

      Logger.info('Subscriber added');
      createRemoteStreamErizoConnection(stream, options);

      callback(true);
    });
  };

  const subscribeData = (streamInput, options, callback = () => {}) => {
    const stream = streamInput;
    socket.sendSDP('subscribe',
      { streamId: stream.getID(),
        data: options.data,
        metadata: options.metadata },
      undefined, (result, error) => {
        if (result === null) {
          Logger.error('Error subscribing to stream ', error);
          callback(undefined, error);
          return;
        }
        Logger.info('Stream subscribed');
        const evt = StreamEvent({ type: 'stream-subscribed', stream });
        that.dispatchEvent(evt);
        callback(true);
      });
  };

  const clearAll = () => {
    that.state = DISCONNECTED;
    socket.state = socket.DISCONNECTED;

    // Remove all streams
    remoteStreams.forEach((stream, id) => {
      removeStream(stream);
      remoteStreams.remove(id);
      if (stream && !stream.failed) {
        const evt2 = StreamEvent({ type: 'stream-removed', stream });
        that.dispatchEvent(evt2);
      }
    });
    remoteStreams = ErizoMap();

    // Close Peer Connections
    localStreams.forEach((stream, id) => {
      removeStream(stream);
      localStreams.remove(id);
    });
    localStreams = ErizoMap();

    // Close socket
    try {
      socket.disconnect();
    } catch (error) {
      Logger.debug('Socket already disconnected');
    }
    socket = undefined;
  };

  // Public functions

  // It stablishes a connection to the room.
  // Once it is done it throws a RoomEvent("room-connected")
  that.connect = () => {
    const token = Base64.decodeBase64(spec.token);

    if (that.state !== DISCONNECTED) {
      Logger.warning('Room already connected');
    }

    // 1- Connect to Erizo-Controller
    that.state = CONNECTING;
    socket.connect(JSON.parse(token), (response) => {
      let stream;
      const streamList = [];
      const streams = response.streams || [];
      const roomId = response.id;

      that.p2p = response.p2p;
      that.iceServers = response.iceServers;
      that.state = CONNECTED;
      spec.defaultVideoBW = response.defaultVideoBW;
      spec.maxVideoBW = response.maxVideoBW;

      // 2- Retrieve list of streams
      const streamIndices = Object.keys(streams);
      for (let index = 0; index < streamIndices.length; index += 1) {
        const arg = streams[streamIndices[index]];
        stream = Stream(that.Connection, { streamID: arg.id,
          local: false,
          audio: arg.audio,
          video: arg.video,
          data: arg.data,
          screen: arg.screen,
          attributes: arg.attributes });
        streamList.push(stream);
        remoteStreams.add(arg.id, stream);
      }

      // 3 - Update RoomID
      that.roomID = roomId;

      Logger.info(`Connected to room ${that.roomID}`);

      const connectEvt = RoomEvent({ type: 'room-connected', streams: streamList });
      that.dispatchEvent(connectEvt);
    }, (error) => {
      Logger.error(`Not Connected! Error: ${error}`);
      const connectEvt = RoomEvent({ type: 'room-error', message: error });
      that.dispatchEvent(connectEvt);
    });
  };

  // It disconnects from the room, dispatching a new RoomEvent("room-disconnected")
  that.disconnect = () => {
    Logger.debug('Disconnection requested');
    // 1- Disconnect from room
    const disconnectEvt = RoomEvent({ type: 'room-disconnected',
      message: 'expected-disconnection' });
    that.dispatchEvent(disconnectEvt);
  };

  // It publishes the stream provided as argument. Once it is added it throws a
  // StreamEvent("stream-added").
  that.publish = (streamInput, optionsInput = {}, callback = () => {}) => {
    const stream = streamInput;
    const options = optionsInput;

    options.maxVideoBW = options.maxVideoBW || spec.defaultVideoBW;
    if (options.maxVideoBW > spec.maxVideoBW) {
      options.maxVideoBW = spec.maxVideoBW;
    }

    if (options.minVideoBW === undefined) {
      options.minVideoBW = 0;
    }

    if (options.minVideoBW > spec.defaultVideoBW) {
      options.minVideoBW = spec.defaultVideoBW;
    }

    stream.forceTurn = options.forceTurn;

    options.simulcast = options.simulcast || false;

    options.muteStream = {
      audio: stream.audioMuted,
      video: stream.videoMuted,
    };

    // 1- If the stream is not local or it is a failed stream we do nothing.
    if (stream && stream.local && !stream.failed && !localStreams.has(stream.getID())) {
      // 2- Publish Media Stream to Erizo-Controller
      if (stream.hasMedia()) {
        if (stream.isExternal()) {
          publishExternal(stream, options, callback);
        } else if (that.p2p) {
          publishP2P(stream, options, callback);
        } else {
          publishErizo(stream, options, callback);
        }
      } else if (stream.hasData()) {
        publishData(stream, options, callback);
      }
    } else {
      Logger.error('Trying to publish invalid stream');
      callback(undefined, 'Invalid Stream');
    }
  };

  // Returns callback(id, error)
  that.startRecording = (stream, callback = () => {}) => {
    if (stream === undefined) {
      Logger.error('Trying to start recording on an invalid stream', stream);
      callback(undefined, 'Invalid Stream');
      return;
    }
    Logger.debug(`Start Recording stream: ${stream.getID()}`);
    socket.sendMessage('startRecorder', { to: stream.getID() }, (id, error) => {
      if (id === null) {
        Logger.error('Error on start recording', error);
        callback(undefined, error);
        return;
      }

      Logger.info('Start recording', id);
      callback(id);
    });
  };

  // Returns callback(id, error)
  that.stopRecording = (recordingId, callback = () => {}) => {
    socket.sendMessage('stopRecorder', { id: recordingId }, (result, error) => {
      if (result === null) {
        Logger.error('Error on stop recording', error);
        callback(undefined, error);
        return;
      }
      Logger.info('Stop recording', recordingId);
      callback(true);
    });
  };

  // It unpublishes the local stream in the room, dispatching a StreamEvent("stream-removed")
  that.unpublish = (streamInput, callback = () => {}) => {
    const stream = streamInput;
    // Unpublish stream from Erizo-Controller
    if (stream && stream.local) {
      // Media stream
      socket.sendMessage('unpublish', stream.getID(), (result, error) => {
        if (result === null) {
          Logger.error('Error unpublishing stream', error);
          callback(undefined, error);
          return;
        }

        delete stream.failed;

        Logger.info('Stream unpublished');
        callback(true);
      });
      stream.room = undefined;
      if (stream.hasMedia() && !stream.isExternal()) {
        removeStream(stream);
      }
      localStreams.remove(stream.getID());

      stream.getID = () => {};
      stream.off('internal-send-data', sendDataSocketFromStreamEvent);
      stream.off('internal-set-attributes', updateAttributesFromStreamEvent);
    } else {
      const error = 'Cannot unpublish, stream does not exist or is not local';
      Logger.error(error);
      callback(undefined, error);
    }
  };

  that.sendControlMessage = (stream, type, action) => {
    if (stream && stream.getID()) {
      const msg = { type: 'control', action };
      socket.sendSDP('signaling_message', { streamId: stream.getID(), msg });
    }
  };

  // It subscribe to a remote stream and draws it inside the HTML tag given by the ID='elementID'
  that.subscribe = (streamInput, optionsInput = {}, callback = () => {}) => {
    const stream = streamInput;
    const options = optionsInput;

    if (stream && !stream.local && !stream.failed) {
      if (stream.hasMedia()) {
        // 1- Subscribe to Stream
        if (!stream.hasVideo() && !stream.hasScreen()) {
          options.video = false;
        }
        if (!stream.hasAudio()) {
          options.audio = false;
        }

        options.muteStream = {
          audio: stream.audioMuted,
          video: stream.videoMuted,
        };

        stream.forceTurn = options.forceTurn;

        if (that.p2p) {
          socket.sendSDP('subscribe', { streamId: stream.getID(), metadata: options.metadata });
          callback(true);
        } else {
          subscribeErizo(stream, options, callback);
        }
      } else if (stream.hasData() && options.data !== false) {
        subscribeData(stream, options, callback);
      } else {
        Logger.warning('There\'s nothing to subscribe to');
        callback(undefined, 'Nothing to subscribe to');
        return;
      }
      // Subscribe to stream stream
      Logger.info(`Subscribing to: ${stream.getID()}`);
    } else {
      let error = 'Error on subscribe';
      if (!stream) {
        Logger.warning('Cannot subscribe to invalid stream');
        error = 'Invalid or undefined stream';
      } else if (stream.local) {
        Logger.warning('Cannot subscribe to local stream, you should ' +
                         'subscribe to the remote version of your local stream');
        error = 'Local copy of stream';
      } else if (stream.failed) {
        Logger.warning('Cannot subscribe to failed stream.');
        error = 'Failed stream';
      }
      callback(undefined, error);
    }
  };

  // It unsubscribes from the stream, removing the HTML element.
  that.unsubscribe = (streamInput, callback = () => {}) => {
    const stream = streamInput;
    // Unsubscribe from stream stream
    if (socket !== undefined) {
      if (stream && !stream.local) {
        socket.sendMessage('unsubscribe', stream.getID(), (result, error) => {
          if (result === null) {
            callback(undefined, error);
            return;
          }
          removeStream(stream);
          delete stream.failed;
          callback(true);
        }, () => {
          Logger.error('Error calling unsubscribe.');
        });
      }
    }
  };

  that.getStreamStats = (stream, callback = () => {}) => {
    if (!socket) {
      return 'Error getting stats - no socket';
    }
    if (!stream) {
      return 'Error getting stats - no stream';
    }

    socket.sendMessage('getStreamStats', stream.getID(), (result) => {
      if (result) {
        callback(result);
      }
    });
    return undefined;
  };

  // It searchs the streams that have "name" attribute with "value" value
  that.getStreamsByAttribute = (name, value) => {
    const streams = [];

    remoteStreams.forEach((stream) => {
      if (stream.getAttributes() !== undefined && stream.getAttributes()[name] === value) {
        streams.push(stream);
      }
    });

    return streams;
  };

  that.on('room-disconnected', clearAll);

  socket.on('onAddStream', socketEventToArgs.bind(null, socketOnAddStream));
  socket.on('signaling_message_erizo', socketEventToArgs.bind(null, socketOnErizoMessage));
  socket.on('signaling_message_peer', socketEventToArgs.bind(null, socketOnPeerMessage));
  socket.on('publish_me', socketEventToArgs.bind(null, socketOnPublishMe));
  socket.on('unpublish_me', socketEventToArgs.bind(null, socketOnUnpublishMe));
  socket.on('onBandwidthAlert', socketEventToArgs.bind(null, socketOnBandwidthAlert));
  socket.on('onDataStream', socketEventToArgs.bind(null, socketOnDataStream));
  socket.on('onUpdateAttributeStream', socketEventToArgs.bind(null, socketOnUpdateAttributeStream));
  socket.on('onRemoveStream', socketEventToArgs.bind(null, socketOnRemoveStream));
  socket.on('disconnect', socketEventToArgs.bind(null, socketOnDisconnect));
  socket.on('connection_failed', socketEventToArgs.bind(null, socketOnICEConnectionFailed));
  socket.on('error', socketEventToArgs.bind(null, socketOnError));

  return that;
};

export default Room;
