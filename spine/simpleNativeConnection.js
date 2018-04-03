process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';  // We need this for testing with self signed certs

const XMLHttpRequest = require('xmlhttprequest').XMLHttpRequest; // eslint-disable-line import/no-extraneous-dependencies
const newIo = require('socket.io-client'); // eslint-disable-line import/no-extraneous-dependencies
const nodeUrl = require('url');
const Erizo = require('./erizofc');
const NativeStream = require('./NativeStream.js');
const nativeConnectionManager = require('./NativeConnectionManager.js');
const nativeConnectionHelpers = require('./NativeConnectionHelpers.js');
const logger = require('./logger').logger;

const log = logger.getLogger('ErizoSimpleNativeConnection');


exports.ErizoSimpleNativeConnection = (spec, callback, error) => {
  const that = {};

  let localStream = {};
  const availableStreams = new Map();

  let room = '';
  that.isActive = false;
  localStream.getID = () => 0;
  const createToken = (userName, role, tokenCallback) => {
    const req = new XMLHttpRequest();
    const theUrl = nodeUrl.parse(spec.serverUrl, true);
    if (theUrl.protocol !== 'https') {
      log.warn('Protocol is not https');
    }

    if (theUrl.query.room) {
      room = theUrl.query.room;
      log.info('will Connect to Room', room);
    }

    theUrl.pathname += 'createToken/';

    const url = nodeUrl.format(theUrl);
    log.info('Url to createToken', url);
    const body = { username: userName, role, room };

    req.onreadystatechange = () => {
      if (req.readyState === 4) {
        if (req.status === 200) {
          tokenCallback(req.responseText);
        } else {
          log.error('Could not get token');
          tokenCallback('error');
        }
      }
    };

    req.open('POST', url, true);
    req.setRequestHeader('Content-Type', 'application/json');
    req.send(JSON.stringify(body));
  };

  const subscribeToStreams = (streams) => {
    streams.forEach((stream) => {
      if (spec.subscribeConfig) {
        log.info('Should subscribe', spec.subscribeConfig);
        room.subscribe(stream, spec.subscribeConfig);
      }
    });
  };

  const createConnection = () => {
    createToken('name', 'presenter', (token) => {
      log.info('Getting token', token);
      if (token === 'error') {
        error(`Could not get token from nuve in ${spec.serverUrl}`);
        return;
      }
      room = Erizo.Room(newIo, nativeConnectionHelpers, nativeConnectionManager, { token });

      room.addEventListener('room-connected', (roomEvent) => {
        log.info('Connected to room');
        callback('room-connected');
        that.isActive = true;
        subscribeToStreams(roomEvent.streams);
        if (spec.publishConfig) {
          log.info('Will publish with config', spec.publishConfig);
          localStream = NativeStream.Stream(nativeConnectionHelpers, spec.publishConfig);
          room.publish(localStream, spec.publishConfig, (id, message) => {
            if (id === undefined) {
              log.error('ERROR when publishing', message);
              error(message);
            }
          });
        }
      });

      room.addEventListener('stream-added', (roomEvent) => {
        log.info('stream added', roomEvent.stream.getID());
        callback('stream-added');
        if (roomEvent.stream.getID() !== localStream.getID()) {
          const streams = [];
          streams.push(roomEvent.stream);
          subscribeToStreams(streams);
        } else {
          log.error('Adding localStream, id', localStream.getID());
          availableStreams.set(localStream.getID(), localStream);
        }
      });

      room.addEventListener('stream-removed', (roomEvent) => {
        callback('stream-removed');
        const eventStreamId = roomEvent.stream.getID();
        if (eventStreamId !== localStream.getID()) {
          room.unsubscribe(roomEvent.stream);
          log.info('stream removed', eventStreamId);
        }
        availableStreams.delete(eventStreamId);
      });

      room.addEventListener('stream-subscribed', (streamEvent) => {
        log.info('stream-subscribed');
        callback('stream-subscribed');
        availableStreams.set(streamEvent.stream.getID(), streamEvent.stream);
      });

      room.addEventListener('room-error', (roomEvent) => {
        log.error('Room Error', roomEvent.type, roomEvent.message);
        error(roomEvent.message);
      });

      room.addEventListener('room-disconnected', (roomEvent) => {
        log.info('Room Disconnected', roomEvent.type, roomEvent.message);
        callback(roomEvent.type + roomEvent.message);
      });

      room.connect();
    });

    that.sendData = () => {
      localStream.sendData({ msg: 'Testing Data Connection' });
    };
  };

  that.close = () => {
    log.info('Close');
    room.disconnect();
  };

  that.getStats = () => {
    const promises = [];
    availableStreams.forEach((stream, streamId) => {
      if (stream.pc && stream.pc.peerConnection && stream.pc.peerConnection.connected) {
        promises.push(stream.pc.peerConnection.getStats());
      } else {
        log.warn('No stream to ask for stats: ', streamId);
        promises.push({});
      }
    });
    return Promise.all(promises);
  };

  that.getStatus = () => {
    if (availableStreams.length === 0) {
      return ['disconnected'];
    }
    const connectionStatus = [];
    availableStreams.forEach((stream) => {
      if (!stream.pc || !stream.pc.peerConnection || !stream.pc.peerConnection.connected) {
        connectionStatus.push('disconnected');
      } else {
        connectionStatus.push('connected');
      }
    });
    return connectionStatus;
  };

  createConnection();
  return that;
};
