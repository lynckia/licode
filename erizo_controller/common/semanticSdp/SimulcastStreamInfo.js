class SimulcastStreamInfo {
  constructor(id, paused) {
    this.paused = paused;
    this.id = id;
  }

  clone() {
    return new SimulcastStreamInfo(this.id, this.paused);
  }

  plain() {
    return {
      id: this.id,
      paused: this.paused,
    };
  }

  isPaused() {
    return this.paused;
  }

  getId() {
    return this.id;
  }
}

module.exports = SimulcastStreamInfo;
