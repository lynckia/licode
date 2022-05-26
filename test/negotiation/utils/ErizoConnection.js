const mediaConfig = require('./../../../rtp_media_config');

global.config = {
  logger: {
    configFile: './log4js_configuration.json',
  },

  erizo: {
    useConnectionQualityCheck: true,
    networkinterface: 'en0',
    addon: 'addonDebug',
  },
};
global.mediaConfig = mediaConfig;
global.bwDistributorConfig = {
  defaultType: 'TargetVideoBW',
};

const erizo = require(`./../../../erizoAPI/build/Release/${global.config.erizo.addon}`);
const RTCPeerConnection = require('./../../../erizo_controller/erizoJS/models/RTCPeerConnection');
const threadPool = new erizo.ThreadPool(5);
threadPool.start();

const ioThreadPool = new erizo.IOThreadPool(1);
ioThreadPool.start();

class ErizoConnection {
  constructor(connectionId) {
    this.webRtcConnectionConfiguration = {
      threadPool,
      ioThreadPool,
      trickleIce: false,
      metadata: {},
      mediaConfiguration: 'default',
      id: connectionId,
      erizoControllerId: 'erizoControllerTest1',
      clientId: 'clientTest1',
      encryptTransport: true,
      options: {},
    };
    this.connectionId = connectionId;
    this.messages = [];
    this.isReady = false;
    threadPool.counter++;
    ioThreadPool.counter++;
    this.connection = new RTCPeerConnection(this.webRtcConnectionConfiguration);
    this.connection.onReady.then(() => {
      this.isReady = true;
    });
    this.connection.init();
  }

  close() {
    this.connection.close();
    ioThreadPool.counter-=1;
    if (ioThreadPool.counter === 0) {
      ioThreadPool.close();
    }
    threadPool.counter-=1;
    if (threadPool.counter === 0) {
      threadPool.close();
    }
  }

  onceNegotiationIsNeeded() {
    return new Promise(resolve => {
      this.connection.once('negotiationneeded', resolve);
    });
  }

  sleep(ms) {
    return new Promise((resolve) => {
      setTimeout(resolve, ms);
    });
  }

  async waitForReadyMessage() {
    await this.connection.onReady;
  }

  async waitForStartedMessage() {
    await this.connection.onStarted;
  }

  publishStream(clientStream) {
    return this.addStream(clientStream, true);
  }

  unpublishStream(clientStream) {
    return this.removeStream(clientStream);
  }

  subscribeStream(remoteStream) {
    remoteStream.addedToConnection = true;
    return this.addStream(remoteStream, false);
  }

  async unsubscribeStream(remoteStream) {
    remoteStream.addedToConnection = false;
    await this.removeStream(remoteStream);
  }

  async addStream(stream, isPublisher) {
    await this.connection.addStream(stream.id, {
      label: stream.label,
      audio: true,
      video: true,
    }, isPublisher);
  }

  async removeStream(stream) {
    if (this.connection.getStream(stream.id)) {
      await this.connection.removeStream(stream.id);
    }
  }

  async removeAllStreams() {
    for (const [id, mediaStream] of this.connection.getStreams()) {
      await this.connection.removeStream(id);
    }
  }

  async createOffer() {
    await this.connection.onGathered;
    await this.connection.setLocalDescription();
    return { sdp: this.connection.localDescription, type: 'offer' };
  }

  async createAnswer() {
    await this.waitForIceGathered();
    await this.connection.setLocalDescription();
    return { sdp: this.connection.localDescription, type: 'answer' };
  }

  async setLocalDescription() {
    await this.connection.setLocalDescription();
  }

  async getLocalDescription() {
    return this.connection.localDescription;
  }

  async setRemoteDescription(description) {
    await this.connection.setRemoteDescription(description);
  }

  async addIceCandidate(candidate) {
    await this.connection.addIceCandidate(candidate);
  }

  async waitForIceGathered() {
    await this.connection.onGathered;
  }

  copyInfoFromSdp(sdp) {
    this.connection.wrtc.copySdpToLocalDescription(sdp);
  }
}

module.exports = ErizoConnection;
