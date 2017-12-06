class StreamInfo {
  constructor(id) {
    this.id = id;
    this.tracks = new Map();
  }

  clone() {
    const cloned = new StreamInfo(this.id);
    this.tracks.forEach((track) => {
      cloned.addTrack(track.clone());
    });
    return cloned;
  }

  plain() {
    const plain = {
      id: this.id,
      tracks: [],
    };
    this.tracks.forEach((track) => {
      plain.tracks.push(track.plain());
    });
    return plain;
  }

  getId() {
    return this.id;
  }

  addTrack(track) {
    this.tracks.set(track.getId(), track);
  }

  getFirstTrack(media) {
    let result;
    this.tracks.forEach((track) => {
      if (track.getMedia().toLowerCase() === media.toLowerCase()) {
        result = track;
      }
    });
    return result;
  }

  getTracks() {
    return this.tracks;
  }

  removeAllTracks() {
    this.tracks.clear();
  }

  getTrack(trackId) {
    return this.tracks.get(trackId);
  }
}

module.exports = StreamInfo;
