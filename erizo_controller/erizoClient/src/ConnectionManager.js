/* global */
import ChromeStableStack from './webrtc-stacks/ChromeStableStack';
import FirefoxStack from './webrtc-stacks/FirefoxStack';
import FcStack from './webrtc-stacks/FcStack';
import Logger from './utils/Logger';
import ErizoMap from './utils/ErizoMap';
import ConnectionHelpers from './utils/ConnectionHelpers';


let ErizoSessionId = 103;

const ErizoConnection = (specInput, erizoId = undefined) => {
  Logger.debug('Building a new Connection');
  const that = {};
  let stack = {};

  that.erizoId = erizoId;
  const streamsMap = ErizoMap(); // key:streamId, value: stream

  const spec = specInput;
  ErizoSessionId += 1;
  spec.sessionId = ErizoSessionId;
  that.sessionId = ErizoSessionId;

  // Check which WebRTC Stack is installed.
  that.browser = ConnectionHelpers.getBrowser();
  if (that.browser === 'fake') {
    Logger.warning('Publish/subscribe video/audio streams not supported in erizofc yet');
    stack = FcStack(spec);
  } else if (that.browser === 'mozilla') {
    Logger.debug('Firefox Stack');
    stack = FirefoxStack(spec);
  } else if (that.browser === 'safari') {
    Logger.debug('Safari using Chrome Stable Stack');
    stack = ChromeStableStack(spec);
  } else if (that.browser === 'chrome-stable' || that.browser === 'electron') {
    Logger.debug('Chrome Stable Stack');
    stack = ChromeStableStack(spec);
  } else {
    Logger.error('No stack available for this browser');
    throw new Error('WebRTC stack not available');
  }
  if (!stack.updateSpec) {
    stack.updateSpec = (newSpec, callback = () => {}) => {
      Logger.error('Update Configuration not implemented in this browser');
      callback('unimplemented');
    };
  }
  if (!stack.setSignallingCallback) {
    stack.setSignallingCallback = () => {
      Logger.error('setSignallingCallback is not implemented in this stack');
    };
  }

  that.close = () => {
    Logger.debug('Closing ErizoConnection');
    streamsMap.clear();
    stack.close();
  };

  that.createOffer = () => {
    stack.createOffer();
  };

  that.addStream = (stream) => {
    Logger.debug(`message: Adding stream to Connection, streamId: ${stream.getID()}`);
    streamsMap.add(stream.getID(), stream);
    if (stream.local) {
      stack.addStream(stream.stream);
    }
  };

  that.removeStream = (stream) => {
    const streamId = stream.getID();
    if (!streamsMap.has(streamId)) {
      Logger.warning(`message: Cannot remove stream not in map, streamId: ${streamId}`);
      return;
    }
    streamsMap.remove(streamId);
  };

  that.processSignalingMessage = (msg) => {
    stack.processSignalingMessage(msg);
  };

  that.sendSignalingMessage = (msg) => {
    stack.sendSignalingMessage(msg);
  };

  that.createOffer = (isSubscribe) => {
    stack.createOffer(isSubscribe);
  };

  that.enableSimulcast = (sdpInput) => {
    stack.enableSimulcast(sdpInput);
  };

  that.updateSpec = (configInput, callback) => {
    stack.updateSpec(configInput, callback);
  };

  // PeerConnection Events
  if (stack.peerConnection) {
    that.peerConnection = stack.peerConnection; // For backwards compatibility
    stack.peerConnection.onaddstream = (stream) => {
      if (that.onaddstream) {
        Logger.error('OnAddStream from peerConnection - stream', stream);
        that.onaddstream(stream);
      }
    };

    stack.peerConnection.onremovestream = (stream) => {
      if (that.onremovestream) {
        that.onremovestream(stream);
      }
    };

    stack.peerConnection.oniceconnectionstatechange = (ev) => {
      if (that.oniceconnectionstatechange) {
        that.oniceconnectionstatechange(ev.target.iceConnectionState);
      }
    };
  }
  return that;
};

const ConnectionManager = () => {
  const that = {};
  that.ErizoConnectionsMap = new Map(); // key: erizoId, value: {connectionId: connection}

  that.getOrBuildErizoConnection = (specInput, erizoId = undefined) => {
    Logger.debug(`message: getOrBuildErizoConnection, erizoId: ${erizoId}`);
    let connection = {};

    if (erizoId === undefined) {
      // we have no erizoJS id - p2p
      return ErizoConnection(specInput);
    }
    connection = ErizoConnection(specInput, erizoId);
    if (that.ErizoConnectionsMap.has(erizoId)) {
      that.ErizoConnectionsMap.get(erizoId)[connection.sessionId] = connection;
    } else {
      const connectionEntry = {};
      connectionEntry[connection.sessionId] = connection;
      that.ErizoConnectionsMap.set(erizoId, connectionEntry);
    }
    return connection;
  };

  that.closeConnection = (connection) => {
    Logger.debug(`Removing connection ${connection.sessionId}
       with erizoId ${connection.erizoId}`);
    connection.close();
    if (that.ErizoConnectionsMap.get(connection.erizoId) !== undefined) {
      delete that.ErizoConnectionsMap.get(connection.erizoId)[connection.sessionId];
    }
  };
  return that;
};

export default ConnectionManager;
