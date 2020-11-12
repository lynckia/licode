class ClientConnection {
  constructor(page, connectionId) {
    this.page = page;
    this.options = { p2p: false, simulcast: true, singlePC: false, audio: true, video: true };
    this.connectionId = connectionId;
    this.erizoId = 'erizoId';
    this.sessionId = parseInt(Math.random() * 10000);
    this.receivedEndOfCandidates = false;
  }

  init() {
    return this.page.evaluate((connectionId, sessionId, erizoId, options) => {
      if (!navigator.connections) {
        navigator.connManager = new Erizo._.ErizoConnectionManager();
        navigator.connections = {};
        navigator.connectionMessages = {};
      }
      navigator.connectionMessages[connectionId] = [];
      navigator.connections[connectionId] = navigator.connManager.getOrBuildErizoConnection({
        callback: (message) => {
          navigator.connectionMessages[connectionId].push(message);
        },
        simulcast: options.simulcast,
        p2p: options.p2p,
        nop2p: !options.p2p,
        label: options.label,
        video: options.video,
        audio: options.audio,
        sessionId: sessionId,
        connectionId: connectionId
      }, erizoId, options.singlePC);
      navigator.connections[connectionId].connectionFailed = false;
      navigator.connections[connectionId].on('connection-failed', () => {
        navigator.connections[connectionId].connectionFailed = true;
      });
    }, this.connectionId, this.sessionId, this.erizoId, this.options);
  }

  addStream(stream) {
    stream.addedToConnection = true;
    return this.page.evaluate((streamId, connectionId) => navigator.connections[connectionId].addStream(navigator.streams[streamId]), stream.id, this.connectionId);
  }

  removeStream(stream) {
    stream.addedToConnection = false;
    return this.page.evaluate((streamId, connectionId) => navigator.connections[connectionId].removeStream(navigator.streams[streamId]), stream.id, this.connectionId);
  }

  async waitForSignalingMessage() {
    await this.page.waitForFunction((connectionId) => navigator.connectionMessages[connectionId].length > 0, {}, this.connectionId);
  }

  async getSignalingMessage() {
    await this.waitForSignalingMessage();
    return this.page.evaluate((connectionId) => navigator.connectionMessages[connectionId].shift(), this.connectionId);
  }

  async isConnectionFailed() {
    return this.page.evaluate((connectionId) => navigator.connections[connectionId].connectionFailed, this.connectionId);
  }

  sleep(ms) {
    return new Promise((resolve) => {
      setTimeout(resolve, ms);
    });
  }

  async waitForCandidates() {
    let lastCandidateReceived = false;
    while(!lastCandidateReceived) {
      const signalingMessages = await this.page.evaluate((connectionId) => navigator.connectionMessages[connectionId], this.connectionId);
      for (const message of signalingMessages) {
        if (message.type === 'candidate' && message.candidate.candidate === 'end') {
          lastCandidateReceived = true;
        }
      }
      if (!lastCandidateReceived) {
        await this.sleep(100);
      }
    }
  }

  async waitForConnected() {
    await this.page.waitForFunction((connectionId) => navigator.connections[connectionId].peerConnection.connectionState === 'connected', {}, this.connectionId);
  }

  async getFSMState() {
    const fsmState = await this.page.evaluate((connectionId) => navigator.connections[connectionId].stack.peerConnectionFsm.state, this.connectionId);
    return fsmState;
  }

  processSignalingMessage(message) {
    return this.page.evaluate((connectionId, message) => {
      navigator.connections[connectionId].processSignalingMessage(message);
    }, this.connectionId, message);
  }

  async getAllCandidates() {
    const candidateMessages = [];
    let moreCandidates = true;
    while(moreCandidates) {
      moreCandidates = await this.page.evaluate((connectionId) => navigator.connectionMessages[connectionId].length, this.connectionId) > 0;
      if (moreCandidates) {
        const clientMessage = await this.page.evaluate((connectionId) => navigator.connectionMessages[connectionId].shift(), this.connectionId);
        candidateMessages.push(clientMessage);
      }
    }
    return candidateMessages;
  }


}
module.exports = ClientConnection;
