class ClientStream {
  constructor(page) {
    this.page = page;
    this.id = parseInt(Math.random() * 10000);
    this.audio = true;
    this.video = true;
    this.data = true;
    this.label = this.id;
    this.addedToConnection = false;
  }

  init() {
    return this.page.evaluate((streamId, audio, video, data) => {
      if (!navigator.streamsAccepted) {
        navigator.streamsAccepted = {};
        navigator.streams = {};
      }
      const stream = Erizo.Stream({ audio, video, data, attributes: {} });
      navigator.streamsAccepted[streamId] = false;
      stream.on('access-accepted', () => {
        navigator.streamsAccepted[streamId] = true;
      });
      stream.init();
      navigator.streams[streamId] = stream;
    }, this.id, this.audio, this.video, this.data);
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

  async remove() {
    await this.page.evaluate((streamId) => {
      navigator.streams[streamId].close();
      delete navigator.streams[streamId];
    }, this.id);
  }
}

module.exports = ClientStream;
