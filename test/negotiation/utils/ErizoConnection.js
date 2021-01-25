const mediaConfig = require('./../../../rtp_media_config');

global.config = {
  logger: {
    configFile: './log4js_configuration.json',
  },

  erizo: {
    useConnectionQualityCheck: true,
    networkinterface: 'en0',
  },
};
global.mediaConfig = mediaConfig;

const erizo = require('./../../../erizoAPI/build/Release/addonDebug');
const NativeErizoConnection = require('./../../../erizo_controller/erizoJS/models/Connection').Connection;
const threadPool = new erizo.ThreadPool(5);
threadPool.start();

const ioThreadPool = new erizo.IOThreadPool(1);
ioThreadPool.start();

class ErizoConnection {
  constructor(connectionId) {
    this.connectionOptions = {
      trickleIce: false,
      metadata: {},
    };
    this.connectionId = connectionId;
    this.messages = [];
    this.isReady = false;
    threadPool.counter++;
    ioThreadPool.counter++;
    this.connection = new NativeErizoConnection('erizoControllerId1', this.connectionId, threadPool, ioThreadPool, this.connectionId, this.connectionOptions);
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

  init() {
    this.onStatusEventCb = (erizoControllerId, clientId, id, info, evt) => {
      if (info.type === 'quality_level') {
        return;
      }
      this.messages.push({ info, evt });
    };
    this.connection.on('status_event', this.onStatusEventCb);
    this.connection.init({ audio: true, video: true, bundle: true });
  }

  getSignalingMessage() {
    const message = this.messages.shift();
    if (!message) {
      return;
    }
    return message.info;
  }

  getSignalingMessages() {
    const messages = this.messages.map(message => message.info);
    this.messages = [];
    return messages;
  }

  processSignalingMessage(message) {
    return this.connection.onSignalingMessage(message);
  }

  sleep(ms) {
    return new Promise((resolve) => {
      setTimeout(resolve, ms);
    });
  }

  async waitForReadyMessage() {
    const checkIfReady = () => this.messages.map(message => message.info).filter(message => message.type === 'ready').length > 0;
    this.isReady = this.isReady || checkIfReady();
    while (!this.isReady) {
      await this.sleep(100);
      this.isReady = checkIfReady()
    }
  }

  async waitForSignalingMessage() {
    const checkMessages = () => this.messages.length > 0;
    let thereIsAMessage = checkMessages();
    while (!thereIsAMessage) {
      await this.sleep(100);
      thereIsAMessage = checkMessages()
    }
  }

  publishStream(clientStream) {
    return this.addStream(clientStream, true, false);
  }

  unpublishStream(clientStream) {
    return this.removeStream(clientStream);
  }

  subscribeStream(remoteStream) {
    remoteStream.addedToConnection = true;
    return this.addStream(remoteStream, false, true);
  }

  async unsubscribeStream(remoteStream) {
    remoteStream.addedToConnection = false;
    await this.removeStream(remoteStream);
  }

  async addStream(stream, isPublisher, offerFromErizo) {
    await this.connection.addMediaStream(stream.id, { label: stream.label }, isPublisher, offerFromErizo);
    this.connection.mediaStreams.get(stream.id).configure(true);
  }

  async removeStream(stream) {
    if (this.connection.mediaStreams.has(stream.id)) {
      await this.connection.removeMediaStream(stream.id, false);
    }
  }

  async removeAllStreams() {
    for (const [id, mediaStream] of this.connection.mediaStreams) {
      await this.connection.removeMediaStream(id);
    }
  }

  async createOffer() {
    await this.waitForIceGathered();
    return this.connection.createOffer();
  }

  async waitForIceGathered() {
    await this.connection.onGathered;
  }

  copyInfoFromSdp(sdp) {
    this.connection.wrtc.copySdpToLocalDescription(sdp);
  }
}

module.exports = ErizoConnection;
