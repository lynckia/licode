/* global */
import ChromeStableStack from './webrtc-stacks/ChromeStableStack';
import SafariStack from './webrtc-stacks/SafariStack';
import FirefoxStack from './webrtc-stacks/FirefoxStack';
import FcStack from './webrtc-stacks/FcStack';
import Logger from './utils/Logger';
import { EventEmitter, ConnectionEvent } from './Events';
import ErizoMap from './utils/ErizoMap';
import ConnectionHelpers from './utils/ConnectionHelpers';

const EventEmitterConst = EventEmitter; // makes google-closure-compiler happy
let ErizoSessionId = 103;

const QUALITY_LEVEL_GOOD = 'good';
const QUALITY_LEVEL_LOW_PACKET_LOSSES = 'low-packet-losses';
const QUALITY_LEVEL_HIGH_PACKET_LOSSES = 'high-packet-losses';

const QUALITY_LEVELS = [
  QUALITY_LEVEL_HIGH_PACKET_LOSSES,
  QUALITY_LEVEL_LOW_PACKET_LOSSES,
  QUALITY_LEVEL_GOOD,
];

const log = Logger.module('ErizoConnection');

class ErizoConnection extends EventEmitterConst {
  constructor(specInput, erizoId = undefined) {
    super();
    this.stack = {};

    this.erizoId = erizoId;
    this.streamsMap = ErizoMap(); // key:streamId, value: stream

    const spec = specInput;
    ErizoSessionId += 1;
    spec.sessionId = ErizoSessionId;
    this.sessionId = ErizoSessionId;
    this.connectionId = spec.connectionId;
    this.disableIceRestart = spec.disableIceRestart;
    this.qualityLevel = QUALITY_LEVEL_GOOD;
    this.wasAbleToConnect = false;

    log.debug(`message: Building a new Connection, ${this.toLog()}`);
    spec.onEnqueueingTimeout = (step) => {
      const message = `reason: Timeout in ${step}`;
      this.emit(ConnectionEvent({ type: 'connection-failed', connection: this, message }));
    };

    if (!spec.streamRemovedListener) {
      spec.streamRemovedListener = () => {};
    }
    this.streamRemovedListener = spec.streamRemovedListener;

    // Check which WebRTC Stack is installed.
    this.browser = ConnectionHelpers.getBrowser();
    if (this.browser === 'fake') {
      log.warning(`message: Publish/subscribe video/audio streams not supported in erizofc yet, ${this.toLog()}`);
      this.stack = FcStack(spec);
    } else if (this.browser === 'mozilla') {
      log.debug(`message: Firefox Stack, ${this.toLog()}`);
      this.stack = FirefoxStack(spec);
    } else if (this.browser === 'safari') {
      log.debug(`message: Safari Stack, ${this.toLog()}`);
      this.stack = SafariStack(spec);
    } else if (this.browser === 'chrome-stable' || this.browser === 'electron') {
      log.debug(`message: Chrome Stable Stack, ${this.toLog()}`);
      this.stack = ChromeStableStack(spec);
    } else {
      log.error(`message: No stack available for this browser, ${this.toLog()}`);
      throw new Error('WebRTC stack not available');
    }
    if (!this.stack.updateSpec) {
      this.stack.updateSpec = (newSpec, callback = () => {}) => {
        log.error(`message: Update Configuration not implemented in this browser, ${this.toLog()}`);
        callback('unimplemented');
      };
    }
    if (!this.stack.setSignallingCallback) {
      this.stack.setSignallingCallback = () => {
        log.error(`message: setSignallingCallback is not implemented in this stack, ${this.toLog()}`);
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
        if (['completed', 'connected'].indexOf(state) !== -1) {
          this.wasAbleToConnect = true;
        }
        if (state === 'failed' && this.wasAbleToConnect && !this.disableIceRestart) {
          log.warning(`message: Restarting ICE, ${this.toLog()}`);
          this.stack.restartIce();
          return;
        }
        this.emit(ConnectionEvent({ type: 'ice-state-change', state, wasAbleToConnect: this.wasAbleToConnect }));
      };
    }
  }

  toLog() {
    return `connectionId: ${this.connectionId}, sessionId: ${this.sessionId}, qualityLevel: ${this.qualityLevel}, erizoId: ${this.erizoId}`;
  }

  close() {
    log.debug(`message: Closing ErizoConnection, ${this.toLog()}`);
    this.streamsMap.clear();
    this.stack.close();
  }

  addStream(stream) {
    log.debug(`message: Adding stream to Connection, ${this.toLog()}, ${stream.toLog()}`);
    this.streamsMap.add(stream.getID(), stream);
    if (stream.local) {
      this.stack.addStream(stream);
    }
  }

  removeStream(stream) {
    const streamId = stream.getID();
    if (!this.streamsMap.has(streamId)) {
      log.debug(`message: Cannot remove stream not in map, ${this.toLog()}, ${stream.toLog()}`);
      return;
    }
    this.streamsMap.remove(streamId);
    if (stream.local) {
      this.stack.removeStream(stream.stream);
    } else if (this.streamsMap.size() === 0) {
      this.streamRemovedListener(stream.getLabel());
    }
  }

  processSignalingMessage(msg) {
    this.stack.processSignalingMessage(msg);
  }

  sendSignalingMessage(msg) {
    this.stack.sendSignalingMessage(msg);
  }

  updateSpec(configInput, streamId, callback) {
    this.stack.updateSpec(configInput, streamId, callback);
  }

  setQualityLevel(level) {
    this.qualityLevel = QUALITY_LEVELS[level];
  }

  getQualityLevel() {
    return { message: this.qualityLevel, index: QUALITY_LEVELS.indexOf(this.qualityLevel) };
  }
}

class ErizoConnectionManager {
  constructor() {
    this.ErizoConnectionsMap = new Map(); // key: erizoId, value: {connectionId: connection}
  }

  getErizoConnection(erizoConnectionId) {
    let connection;
    this.ErizoConnectionsMap.forEach((entry) => {
      Object.keys(entry).forEach((entryKey) => {
        if (entry[entryKey].connectionId === erizoConnectionId) {
          connection = entry[entryKey];
        }
      });
    });
    return connection;
  }

  getOrBuildErizoConnection(specInput, erizoId = undefined, singlePC = false) {
    log.debug(`message: getOrBuildErizoConnection, erizoId: ${erizoId}, singlePC: ${singlePC}`);
    let connection = {};
    const type = specInput.isRemote ? 'subscribe' : 'publish';

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
      if (!connectionEntry[`single-pc-${type}`]) {
        connectionEntry[`single-pc-${type}`] = new ErizoConnection(specInput, erizoId);
      }
      connection = connectionEntry[`single-pc-${type}`];
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
    return connection;
  }

  maybeCloseConnection(connection, force = false) {
    log.debug(`message: Trying to remove connection, ${connection.toLog()}`);
    if (connection.streamsMap.size() === 0 || force) {
      log.debug(`message: No streams in connection, ${connection.toLog()}`);
      const peerConnection = this.ErizoConnectionsMap.get(connection.erizoId);
      if (peerConnection !== undefined) {
        if ((peerConnection['single-pc-publish'] || peerConnection['single-pc-subscribe']) && !force) {
          log.debug(`message: Will not remove empty connection, ${connection.toLog()}, reason: It is singlePC`);
          return;
        }
      }
      connection.close();
      if (peerConnection !== undefined) {
        delete peerConnection['single-pc-subscribe'];
        delete peerConnection['single-pc-publish'];
        delete peerConnection[connection.sessionId];
      }
    }
  }
}

export default ErizoConnectionManager;
