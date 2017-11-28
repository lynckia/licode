class SourceInfo {
  constructor(ssrc) {
    this.ssrc = ssrc;
  }

  clone() {
    const clone = new SourceInfo(this.ssrc);
    clone.setCName(this.cname);
    clone.setStreamId(this.streamId);
    this.setTrackId(this.trackId);
  }


  plain() {
    const plain = {
      ssrc: this.ssrc,
    };
    if (this.cname) plain.cname = this.cname;
    if (this.label) plain.label = this.label;
    if (this.mslabel) plain.mslabel = this.mslabel;
    if (this.streamId) plain.streamId = this.streamId;
    if (this.trackId) plain.trackid = this.trackId;
    return plain;
  }

  getCName() {
    return this.cname;
  }

  setCName(cname) {
    this.cname = cname;
  }

  getStreamId() {
    return this.streamId;
  }

  setStreamId(streamId) {
    this.streamId = streamId;
  }

  getTrackId() {
    return this.trackId;
  }

  setTrackId(trackId) {
    this.trackId = trackId;
  }

  getMSLabel() {
    return this.mslabel;
  }

  setMSLabel(mslabel) {
    this.mslabel = mslabel;
  }

  getLabel() {
    return this.label;
  }

  setLabel(label) {
    this.label = label;
  }

  getSSRC() {
    return this.ssrc;
  }
}

module.exports = SourceInfo;
