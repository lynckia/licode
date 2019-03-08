/* global exports */

/* eslint-disable no-param-reassign */

exports.Stream = (spec) => {
  const that = {};
  const dataSubscribers = [];

  that.getID = () => spec.id;

  that.getClient = () => spec.client;

    // Indicates if the stream has audio activated
  that.hasAudio = () => spec.audio;

    // Indicates if the stream has video activated
  that.hasVideo = () => spec.video;


    // Indicates if the stream has data activated
  that.hasData = () => spec.data;

    // Indicates if the stream has screen activated
  that.hasScreen = () => spec.screen;

    // Retrieves attributes from stream
  that.getAttributes = () => spec.attributes;

    // Set attributes for this stream
  that.setAttributes = (attrs) => {
    spec.attributes = attrs;
  };

    // Retrieves subscribers to data
  that.getDataSubscribers = () => dataSubscribers;

  that.forEachDataSubscriber = (action) => {
    for (let i = 0; i < that.getDataSubscribers().length; i += 1) {
      action(i, that.getDataSubscribers()[i]);
    }
  };

    // Add a new data subscriber to this stream
  that.addDataSubscriber = (id) => {
    if (dataSubscribers.indexOf(id) === -1) {
      dataSubscribers.push(id);
    }
  };

    // Removes a data subscriber from this stream
  that.removeDataSubscriber = (id) => {
    const index = dataSubscribers.indexOf(id);
    if (index !== -1) {
      dataSubscribers.splice(index, 1);
    }
  };

    // Returns the public specification of this stream
  // eslint-disable-next-line arrow-body-style
  that.getPublicStream = () => {
    return { id: spec.id,
      audio: spec.audio,
      video: spec.video,
      data: spec.data,
      label: spec.label,
      screen: spec.screen,
      attributes: spec.attributes };
  };

  // const selectors = {
  //   '/id': '23',
  //   '/attributes/group': '23',
  //   '/attributes/kind': 'professor',
  //   '/attributes/externalId': '10'
  // };
  that.meetAnySelector = (selectors) => {
    // eslint-disable-next-line no-restricted-syntax
    for (const selector of Object.keys(selectors)) {
      const value = selectors[selector];
      if (selector.startsWith('/attributes')) {
        const attribute = selector.replace('/attributes/', '');
        if (that.getAttributes()[attribute] === value) {
          return true;
        }
      } else if (selector === '/id' && value === that.getID()) {
        return true;
      } else if (selector === '/label' && value === spec.label) {
        return true;
      }
    }
    return false;
  };

  return that;
};
