let currentClientStreamId = 0;
class ClientStream {
  constructor(page, color, frequency) {
    this.page = page;
    this.id = currentClientStreamId++;
    this.audio = true;
    this.video = true;
    this.data = true;
    this.label = this.id;
    this.addedToConnection = false;
    this.color = color;
    this.frequency = frequency;
    this.backgroundColor = color ? "#ffffff" : undefined;
  }

  init() {
    return this.page.evaluate((streamId, audio, video, data, background, color, frequency) => {
      if (!navigator.streamsAccepted) {
        navigator.streamsAccepted = {};
        navigator.streams = {};
      }
      const stream = Erizo.Stream({ audio, video, data, attributes: {} });
      stream.stream = navigator.startLocalVideo(background, color, frequency);
      navigator.streamsAccepted[streamId] = false;
      stream.on('access-accepted', () => {
        navigator.streamsAccepted[streamId] = true;
      });
      // stream.init();
      navigator.streamsAccepted[streamId] = true;
      navigator.streams[streamId] = stream;
    }, this.id, this.audio, this.video, this.data, this.backgroundColor, this.color, this.frequency);
  }

  registerLocalVideoCreator() {
    return this.page.evaluate(() => {
      navigator.startLocalVideo = (fill = undefined, background = undefined, frequency = undefined) => {
        let tone = (frequency) => {
          let ctx = new AudioContext(), oscillator = ctx.createOscillator();
          const gainNode = ctx.createGain();
          const dst = ctx.createMediaStreamDestination();
          oscillator.type = 'sine';
          oscillator.frequency.setValueAtTime(frequency, ctx.currentTime); // value in hertz

          oscillator.connect(gainNode);
          gainNode.connect(dst);

          gainNode.gain.setValueAtTime(1, ctx.currentTime);

          oscillator.start();
          setInterval(function() {
            gainNode.gain.setValueAtTime(1, ctx.currentTime);
            setTimeout(function () {
              gainNode.gain.setValueAtTime(0, ctx.currentTime);
            }, 100);
          }, 500);

          return dst.stream.getAudioTracks()[0];
        }
        function whiteNoise(width, height, fill, background) {
          const canvas = Object.assign(document.createElement('canvas'), {width, height});
          const ctx = canvas.getContext('2d');
          var start = new Date().getTime();
          requestAnimationFrame(function draw () {
            ctx.clearRect(0,0,width,height);
            ctx.fillStyle = background;
            ctx.fillRect(0, 0, width, height);
            ctx.textBaseline = "top";
            ctx.font = '48px serif';
            ctx.fillStyle = fill;
            var now = new Date().getTime();
            ctx.fillText(now - start, 50, 50);
            requestAnimationFrame(draw);
          });
          let stream = canvas.captureStream();
          return stream.getVideoTracks()[0];
        }
        const tracks = [];
        if (fill && background) {
          tracks.push(whiteNoise(640, 480, fill, background));
        }
        if (frequency) {
          tracks.push(tone(frequency));
        }
        return new MediaStream(tracks);
      };
    });
  }

  async getLabel() {
    const label = await this.page.evaluate((streamId) => {
      return navigator.streams[streamId].getLabel();
    }, this.id);
    this.label = label;
  }

  waitForAccepted() {
    return this.page.waitForFunction((streamId) => navigator.streamsAccepted[streamId], {}, this.id);
  }

  async show() {
    await this.page.evaluate((streamId) => {
      const stream = navigator.streams[streamId];
      const div = document.createElement('div');
      div.setAttribute('style', 'width: 320px; height: 240px;float:left;');
      div.setAttribute('id', `test${streamId}`);

      document.getElementById('videoContainer').appendChild(div);
      stream.show(`test${streamId}`);
    }, this.id);
  }

  async remove() {
    await this.page.evaluate((streamId) => {
      navigator.streams[streamId].close();
      delete navigator.streams[streamId];
    }, this.id);
  }
}

module.exports = ClientStream;
