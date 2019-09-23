/* global exports */

/* eslint-disable no-param-reassign */

const StreamStates = require('./models/Stream').StreamStates;


class StreamManager {
  constructor() {
    this.publishedStreams = new Map();
  }

  addPublishedStream(streamId, publishedStream) {
    this.publishedStreams.set(streamId, publishedStream);
  }

  removePublishedStream(id) {
    return this.publishedStreams.delete(id);
  }

  forEachPublishedStream(doSomething) {
    this.publishedStreams.forEach((stream) => {
      doSomething(stream);
    });
  }

  getPublishedStreamById(id) {
    return this.publishedStreams.get(id);
  }

  hasPublishedStream(id) {
    return this.publishedStreams.has(id);
  }

  getPublishedStreamState(id) {
    if (this.hasPublishedStream(id)) {
      return this.publishedStreams.get(id).state;
    }
    return StreamStates.PUBLISHER_UNKNOWN;
  }

  getErizoIdForPublishedStreamId(id) {
    if (this.publishedStreams.has(id)) {
      return this.publishedStreams.get(id).erizoId;
    }
    return undefined;
  }

  getPublishersInErizoId(erizoId) {
    const streamsInErizo = [];
    this.publishedStreams.forEach((stream) => {
      if (stream.erizoId === erizoId) {
        streamsInErizo.push(stream);
      }
    });
    return streamsInErizo;
  }

  updateErizoIdForPublishedStream(streamId, erizoId) {
    if (this.publishedStreams.has(streamId)) {
      this.publishedStreams.get(streamId).erizoId = erizoId;
    }
  }
}

exports.StreamManager = StreamManager;
