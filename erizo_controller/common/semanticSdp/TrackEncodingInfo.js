class TrackEncodingInfo {
  constructor(id, paused) {
    this.id = id;
    this.paused = paused;
    this.codecs = new Map();
    this.params = new Map();
  }

  clone() {
    const cloned = new TrackEncodingInfo(this.id, this.paused);
    this.codecs.forEach((codec) => {
      cloned.addCodec(codec.cloned());
    });
    cloned.setParams(this.params);
    return cloned;
  }

  plain() {
    const plain = {
      id: this.id,
      paused: this.paused,
      codecs: {},
      params: {},
    };
    this.codecs.keys().forEach((id) => {
      plain.codecs[id] = this.codecs.get(id).plain();
    });
    this.params.keys().forEach((id) => {
      plain.params[id] = this.params.get(id).param;
    });
    return plain;
  }

  getId() {
    return this.id;
  }

  getCodecs() {
    return this.codecs;
  }

  addCodec(codec) {
    this.codecs.set(codec.getType(), codec);
  }

  getParams() {
    return this.params;
  }

  setParams(params) {
    this.params = new Map(params);
  }

  isPaused() {
    return this.paused;
  }
}

module.exports = TrackEncodingInfo;
