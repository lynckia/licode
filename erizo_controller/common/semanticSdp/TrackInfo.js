class TrackInfo {
  constructor(media, id) {
    this.media = media;
    this.id = id;
    this.ssrcs = [];
    this.groups = [];
    this.encodings = [];
  }

  clone() {
    const cloned = new TrackInfo(this.media, this.id);
    if (this.mediaId) {
      cloned.setMediaId(this.mediaId);
    }
    this.ssrcs.forEach((ssrc) => {
      cloned.addSSRC(ssrc);
    });
    this.groups.forEach((group) => {
      cloned.addSourceGroup(group.clone());
    });
    this.encodings.forEach((encoding) => {
      const alternatives = [];
      encoding.forEach((entry) => {
        alternatives.push(entry.cloned());
      });
      cloned.addAlternativeEncoding(alternatives);
    });
    return cloned;
  }

  plain() {
    const plain = {
      media: this.media,
      id: this.id,
      ssrcs: [],
      groups: [],
      encodings: [],
    };
    if (this.mediaId) {
      plain.mediaId = this.mediaId;
    }
    this.ssrcs.forEach((ssrc) => {
      plain.ssrcs.push(ssrc);
    });
    this.groups.forEach((group) => {
      plain.groups.push(group.plain());
    });
    this.encodings.forEach((encoding) => {
      const alternatives = [];
      encoding.forEach((entry) => {
        alternatives.push(entry.plain());
      });
      plain.encodings.push(alternatives);
    });
    return plain;
  }

  getMedia() {
    return this.media;
  }

  setMediaId(mediaId) {
    this.mediaId = mediaId;
  }

  getMediaId() {
    return this.mediaId;
  }

  getId() {
    return this.id;
  }

  addSSRC(ssrc) {
    this.ssrcs.push(ssrc);
  }

  getSSRCs() {
    return this.ssrcs;
  }

  addSourceGroup(group) {
    this.groups.push(group);
  }

  getSourceGroup(schematics) {
    let result;
    this.groups.forEach((group) => {
      if (group.getSemantics().toLowerCase() === schematics.toLowerCase()) {
        result = group;
      }
    });
    return result;
  }

  getSourceGroups() {
    return this.groups;
  }

  hasSourceGroup(schematics) {
    let result = false;
    this.groups.forEach((group) => {
      if (group.getSemantics().toLowerCase() === schematics.toLowerCase()) {
        result = true;
      }
    });
    return result;
  }

  getEncodings() {
    return this.encodings;
  }

  addAlternaticeEncodings(alternatives) {
    this.encodings.push(alternatives);
  }

  setEncodings(encodings) {
    this.encodings = encodings;
  }
}

module.exports = TrackInfo;
