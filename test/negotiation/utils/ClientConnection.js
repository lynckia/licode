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
          console.log("New message:", message);
          navigator.connectionMessages[connectionId].push(message);
        },
        managed: true,
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

  registerFrameProcessingFunctions() {
    return this.page.evaluate(() => {
      navigator.getImageData = function (canvas, img) {
        return canvas.getContext('2d').getImageData(0, 0, img.width, img.height);
      }
    });
  }

  addStream(stream) {
    stream.addedToConnection = true;
    return this.page.evaluate((streamId, connectionId) => navigator.connections[connectionId].addStream(navigator.streams[streamId]), stream.id, this.connectionId);
  }

  removeStream(stream) {
    stream.addedToConnection = false;
    return this.page.evaluate((streamId, connectionId) => navigator.connections[connectionId].removeStream(navigator.streams[streamId]), stream.id, this.connectionId);
  }

  showStream(erizoStream) {
    return this.page.evaluate((streamId, connectionId) => {
      const stream = navigator.connections[connectionId]
        .stack.peerConnection.getRemoteStreams().find(s => s.label = streamId);
      if (!stream) {
        return;
      }
      const div = document.createElement('div');
      div.setAttribute('id', `player_${streamId}`);
      div.setAttribute('class', 'licode_player');
      div.setAttribute('style', 'width: 100%; height: 100%; position: relative; ' +
                                'background-color: black; overflow: hidden;');
      video = document.createElement('video');
      video.setAttribute('id', `stream${streamId}`);
      video.setAttribute('class', 'licode_stream');
      video.setAttribute('style', 'width: 100%; height: 100%; position: absolute; object-fit: cover');
      video.setAttribute('autoplay', 'autoplay');
      video.setAttribute('playsinline', 'playsinline');
      const container = document.createElement('div');
      container.setAttribute('style', 'width: 320px; height: 240px;float:left;');
      container.setAttribute('id', `test${streamId}`);

      document.getElementById('videoContainer').appendChild(container);
      container.appendChild(div);
      div.appendChild(video);
      video.srcObject = stream;
      video.muted = true;

      // const context = new AudioContext();
      // const peerInput = context.createMediaStreamSource(stream);
      // const panner = document.context.createPanner();
      // panner.setPosition(0, 0, 0);
      // peerInput.connect(panner);
      // peerInput.connect(context.destination);
      const audioStream = new MediaStream(stream.getAudioTracks());
      // const recvAudio = new Audio();
      // recvAudio.srcObject = audioStream;
      // recvAudio.autoplay = true;
      // recvAudio.muted = true;
      const audioCtx = new AudioContext();
      const source = audioCtx.createMediaStreamSource(audioStream);
      source.connect(audioCtx.destination);
      console.log("OK");

    }, erizoStream.label, this.connectionId);
  }

  async getImageData(erizoStream) {
    const tryImageData = () => {
      return this.page.evaluate(async (streamId, connectionId) => {
        try {
          const stream = navigator.connections[connectionId]
            .stack.peerConnection.getRemoteStreams().find(s => s.label = streamId);
          if (!stream) {
            return;
          }

          const imageCapture = new ImageCapture(stream.getVideoTracks()[0]);
          const imageBitmap = await imageCapture.grabFrame();
          const width = imageBitmap.width;
          const height = imageBitmap.height;
          const canvas = Object.assign(document.createElement('canvas'), {width, height});
          canvas.getContext('2d').drawImage(imageBitmap, 0, 0, width, height);
          const image = canvas.getContext('2d').getImageData(0, 0, width, height);

          return { result: image.data };
        } catch(e) {
          return { error: e.message };
        }
      }, erizoStream.label, this.connectionId);
    }
    for (let attempt = 0; attempt < 3; attempt++) {
      const result = await tryImageData();
      if (result.error !== 'The associated Track is in an invalid state') {
        return result.result;
      }
    }
    return 'The associated Track is in an invalid state';
  }

  setLocalDescription() {
    return this.page.evaluate(async (connectionId) => navigator.connections[connectionId].stack.setLocalDescription(), this.connectionId);
  }

  getLocalDescription() {
    return this.page.evaluate((connectionId) => navigator.connections[connectionId].stack.getLocalDescription(), this.connectionId);
  }

  setRemoteDescription(description) {
    return this.page.evaluate((description, connectionId) => navigator.connections[connectionId].stack.setRemoteDescription(description), description, this.connectionId);
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
    let candidates = 0;
    while(candidates < 2) {
      const signalingMessages = await this.page.evaluate((connectionId) => navigator.connectionMessages[connectionId], this.connectionId);
      for (const message of signalingMessages) {
        if (message.type === 'candidate') {
          candidates++;
        }
      }
      if (candidates < 2) {
        await this.sleep(100);
      }
    }
  }

  async waitForConnected() {
    await this.page.waitForFunction((connectionId) => {
      console.log(navigator.connections[connectionId].peerConnection.connectionState);
      return navigator.connections[connectionId].peerConnection.connectionState === 'connected';
    }, {}, this.connectionId);
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
