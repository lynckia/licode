/* global */
import ChromeStableStack from './webrtc-stacks/ChromeStableStack';
import FirefoxStack from './webrtc-stacks/FirefoxStack';
import FcStack from './webrtc-stacks/FcStack';
import Logger from './utils/Logger';
import { EventEmitter, ConnectionEvent } from './Events';
import ErizoMap from './utils/ErizoMap';
import ConnectionHelpers from './utils/ConnectionHelpers';

const EventEmitterConst = EventEmitter; // makes google-closure-compiler happy
let ErizoSessionId = 103;

class ErizoConnection extends EventEmitterConst {
  constructor(specInput, erizoId = undefined) {
    super();
    Logger.debug('Building a new Connection');
    this.stack = {};

    this.erizoId = erizoId;
    this.streamsMap = ErizoMap(); // key:streamId, value: stream

    const spec = specInput;
    ErizoSessionId += 1;
    spec.sessionId = ErizoSessionId;
    this.sessionId = ErizoSessionId;

    if (!spec.streamRemovedListener) {
      spec.streamRemovedListener = () => {};
    }
    this.streamRemovedListener = spec.streamRemovedListener;

    // Check which WebRTC Stack is installed.
    this.browser = ConnectionHelpers.getBrowser();
    if (this.browser === 'fake') {
      Logger.warning('Publish/subscribe video/audio streams not supported in erizofc yet');
      this.stack = FcStack(spec);
    } else if (this.browser === 'mozilla') {
      Logger.debug('Firefox Stack');
      this.stack = FirefoxStack(spec);
    } else if (this.browser === 'safari') {
      Logger.debug('Safari using Chrome Stable Stack');
      this.stack = ChromeStableStack(spec);
    } else if (this.browser === 'chrome-stable' || this.browser === 'electron') {
      Logger.debug('Chrome Stable Stack');
      this.stack = ChromeStableStack(spec);
    } else {
      Logger.error('No stack available for this browser');
      throw new Error('WebRTC stack not available');
    }
    if (!this.stack.updateSpec) {
      this.stack.updateSpec = (newSpec, callback = () => {}) => {
        Logger.error('Update Configuration not implemented in this browser');
        callback('unimplemented');
      };
    }
    if (!this.stack.setSignallingCallback) {
      this.stack.setSignallingCallback = () => {
        Logger.error('setSignallingCallback is not implemented in this stack');
      };
    }

    // PeerConnection Events
    if (this.stack.peerConnection) {
      this.peerConnection = this.stack.peerConnection; // For backwards compatibility
      this.stack.peerConnection.onaddstream = (evt) => {
        this.emit(ConnectionEvent({ type: 'add-stream', stream: evt.stream }));
      };

      this.stack.peerConnection.onremovestream = (evt) => {
        this.emit(ConnectionEvent({ type: 'remove-stream', stream: evt.stream }));
        this.streamRemovedListener(evt.stream.id);
      };

      this.stack.peerConnection.oniceconnectionstatechange = () => {
        const state = this.stack.peerConnection.iceConnectionState;
        this.emit(ConnectionEvent({ type: 'ice-state-change', state }));
      };
    }
  }

  close() {
    Logger.debug('Closing ErizoConnection');
    this.streamsMap.clear();
    this.stack.close();
  }

  createOffer(isSubscribe, forceOfferToReceive, streamId) {
    this.stack.createOffer(isSubscribe, forceOfferToReceive, streamId);
  }

  addStream(stream) {
    Logger.debug(`message: Adding stream to Connection, streamId: ${stream.getID()}`);
    this.streamsMap.add(stream.getID(), stream);
    if (stream.local) {
      this.stack.addStream(stream.stream);
    }
  }

  removeStream(stream) {
    const streamId = stream.getID();
    if (!this.streamsMap.has(streamId)) {
      Logger.warning(`message: Cannot remove stream not in map, streamId: ${streamId}`);
      return;
    }
    if (stream.local) {
      this.stack.removeStream(stream.stream);
    } else if (this.streamsMap.size() === 1) {
      this.streamRemovedListener(stream.getLabel());
    }
    this.streamsMap.remove(streamId);
  }

  processSignalingMessage(msg) {
    this.stack.processSignalingMessage(msg);
  }

  sendSignalingMessage(msg) {
    this.stack.sendSignalingMessage(msg);
  }

  setSimulcast(enable) {
    this.stack.setSimulcast(enable);
  }

  setVideo(video) {
    this.stack.setVideo(video);
  }

  setAudio(audio) {
    this.stack.setAudio(audio);
  }

  updateSpec(configInput, streamId, callback) {
    this.stack.updateSpec(configInput, streamId, callback);
  }
}

class ErizoConnectionManager {
  constructor() {
    this.ErizoConnectionsMap = new Map(); // key: erizoId, value: {connectionId: connection}
  }

  getOrBuildErizoConnection(specInput, erizoId = undefined, singlePC = false) {
    Logger.debug(`message: getOrBuildErizoConnection, erizoId: ${erizoId}`);
    let connection = {};

    if (erizoId === undefined) {
      // we have no erizoJS id - p2p
      return new ErizoConnection(specInput);
    }
    if (singlePC) {
      let connectionEntry;
      if (this.ErizoConnectionsMap.has(erizoId)) {
        connectionEntry = this.ErizoConnectionsMap.get(erizoId);
      } else {
        connectionEntry = {};
        this.ErizoConnectionsMap.set(erizoId, connectionEntry);
      }
      if (!connectionEntry['single-pc']) {
        connectionEntry['single-pc'] = new ErizoConnection(specInput, erizoId);
      }
      connection = connectionEntry['single-pc'];
    } else {
      connection = new ErizoConnection(specInput, erizoId);
      if (this.ErizoConnectionsMap.has(erizoId)) {
        this.ErizoConnectionsMap.get(erizoId)[connection.sessionId] = connection;
      } else {
        const connectionEntry = {};
        connectionEntry[connection.sessionId] = connection;
        this.ErizoConnectionsMap.set(erizoId, connectionEntry);
      }
    }
    if (specInput.simulcast) {
      connection.setSimulcast(specInput.simulcast);
    }
    if (specInput.video) {
      connection.setVideo(specInput.video);
    }
    if (specInput.audio) {
      connection.setVideo(specInput.audio);
    }

    return connection;
  }

  maybeCloseConnection(connection) {
    Logger.debug(`Trying to remove connection ${connection.sessionId}
       with erizoId ${connection.erizoId}`);
    if (connection.streamsMap.size() === 0) {
      connection.close();
      if (this.ErizoConnectionsMap.get(connection.erizoId) !== undefined) {
        delete this.ErizoConnectionsMap.get(connection.erizoId)['single-pc'];
        delete this.ErizoConnectionsMap.get(connection.erizoId)[connection.sessionId];
      }
    }
  }
}

export default ErizoConnectionManager;
