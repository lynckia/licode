/* global exports */

/* eslint-disable no-param-reassign */

const StreamStates = Object.freeze({
  PUBLISHER_CREATED: 100,
  PUBLISHER_INITIAL: 101,
  PUBLISHER_READY: 104,
  PUBLISHER_FAILED: 105,
  PUBLISHER_REQUESTED_CLOSE: 106,
  PUBLISHER_UNKNOWN: 404,
  SUBSCRIBER_CREATED: 200,
  SUBSCRIBER_INITIAL: 201,
  SUBSCRIBER_READY: 204,
  SUBSCRIBER_FAILED: 205,
  SUBSCRIBER_REQUESTED_CLOSE: 206,
  SUBSCRIBER_UNKNOWN: 404,
});

class AvSubscriber {
  constructor(clientId) {
    this.clientId = clientId;
    this.state = StreamStates.SUBSCRIBER_CREATED;
  }
}

class ExternalOutputSubscriber {
  constructor(url) {
    this.url = url;
    this.state = StreamStates.SUBSCRIBER_CREATED;
  }
}

class PublishedStream {
  constructor(spec) {
    this.id = spec.id;
    this.clientId = spec.client;
    this.audio = spec.audio;
    this.video = spec.video;
    this.data = spec.data;
    this.screen = spec.screen;
    this.attributes = spec.attributes;
    this.dataSubscribers = [];
    this.avSubscribers = new Map();
    this.externalOutputSubscribers = new Map();
    this.label = spec.label;
    this.erizoId = null;
    this.state = StreamStates.PUBLISHER_CREATED;
  }
  getAttributes() {
    return this.attributes;
  }

  setAttributes(newAttributes) {
    this.attributes = Object.assign({}, newAttributes);
  }

  getID() {
    return this.id;
  }

  getClientId() {
    return this.clientId;
  }

  hasVideo() {
    return this.video;
  }

  hasAudio() {
    return this.audio;
  }

  hasData() {
    return this.data;
  }

  hasScreen() {
    return this.screen;
  }

  updateErizoId(erizoId) {
    this.erizoId = erizoId;
  }

  updateStreamState(state) {
    this.state = state;
  }

  forEachDataSubscriber(action) {
    for (let i = 0; i < this.dataSubscribers.length; i += 1) {
      action(i, this.dataSubscribers[i]);
    }
  }
  addDataSubscriber(id) {
    if (this.dataSubscribers.indexOf(id) === -1) {
      this.dataSubscribers.push(id);
    }
  }
  // Removes a data subscriber from this stream
  removeDataSubscriber(id) {
    const index = this.dataSubscribers.indexOf(id);
    if (index !== -1) {
      this.dataSubscribers.splice(index, 1);
    }
  }

  addAvSubscriber(clientId) {
    this.avSubscribers.set(clientId, new AvSubscriber(clientId));
  }

  removeAvSubscriber(clientId) {
    this.avSubscribers.delete(clientId);
  }

  getAvSubscriber(clientId) {
    return this.avSubscribers.get(clientId);
  }

  hasAvSubscriber(clientId) {
    return this.avSubscribers.has(clientId);
  }

  updateAvSubscriberState(clientId, state) {
    this.avSubscribers.get(clientId).state = state;
  }

  getAvSubscriberState(clientId) {
    if (this.hasAvSubscriber(clientId)) {
      return this.avSubscribers.get(clientId).state;
    }
    return StreamStates.SUBSCRIBER_UNKNOWN;
  }

  addExternalOutputSubscriber(url) {
    this.externalOutputSubscribers.set(url, new ExternalOutputSubscriber(url));
  }

  getExternalOutputSubscriber(url) {
    return this.externalOutputSubscribers.get(url);
  }

  updateExternalOutputSubscriberState(url, state) {
    this.externalOutputSubscribers.get(url).state = state;
  }

  getExternalOutputSubscriberState(url) {
    if (this.hasExternalOutputSubscriber(url)) {
      return this.externalOutputSubscribers.get(url).state;
    }
    return StreamStates.SUBSCRIBER_UNKNOWN;
  }

  hasExternalOutputSubscriber(url) {
    return this.externalOutputSubscribers.has(url);
  }

  removeExternalOutputSubscriber(url) {
    this.externalOutputSubscribers.delete(url);
  }

  // Returns the public specification of this stream
  // eslint-disable-next-line arrow-body-style
  getPublicStream() {
    return { id: this.id,
      audio: this.audio,
      video: this.video,
      data: this.data,
      label: this.label,
      screen: this.screen,
      attributes: this.attributes };
  }

  // const selectors = {
  //   '/id': '23',
  //   '/attributes/group': '23',
  //   '/attributes/kind': 'professor',
  //   '/attributes/externalId': '10'
  // };
  meetAnySelector(selectors) {
    // eslint-disable-next-line no-restricted-syntax
    for (const selector of Object.keys(selectors)) {
      const value = selectors[selector];
      if (selector.startsWith('/attributes')) {
        const attribute = selector.replace('/attributes/', '');
        if (this.attributes[attribute] === value) {
          return true;
        }
      } else if (selector === '/id' && value === this.id) {
        return true;
      } else if (selector === '/label' && value === this.label) {
        return true;
      }
    }
    return false;
  }
}

exports.PublishedStream = PublishedStream;
exports.StreamStates = StreamStates;
