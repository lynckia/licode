
/* globals Erizo */

/* eslint-env browser */
/* eslint-disable no-param-reassign, no-console */

const serverUrl = '/';
let localStream;
let room;
let recording = false;
let recordingId = '';
let localStreamIndex = 0;
const localStreams = new Map();
const configFlags = {
  noStart: false, // disable start button when only subscribe
  forceStart: false, // force start button in all cases
  screen: false, // screensharinug
  room: 'basicExampleRoom', // room name
  singlePC: true,
  type: 'erizo', // room type
  onlyAudio: false,
  mediaConfiguration: 'default',
  onlySubscribe: false,
  onlyPublish: false,
  autoSubscribe: false,
  simulcast: false,
  unencrypted: false,
};

const createSubscriberContainer = (stream) => {
  const container = document.createElement('div');
  container.setAttribute('style', 'width: 320px; height: 280px;float:left;');
  container.setAttribute('id', `container_${stream.getID()}`);

  const videoContainer = document.createElement('div');
  videoContainer.setAttribute('style', 'width: 320px; height: 240px;');
  videoContainer.setAttribute('id', `test${stream.getID()}`);
  container.appendChild(videoContainer);
  const unsubscribeButton = document.createElement('button');
  unsubscribeButton.textContent = 'Unsubscribe';
  unsubscribeButton.setAttribute('style', 'float:left;');
  const slideshowButton = document.createElement('button');
  slideshowButton.textContent = 'Toggle Slideshow';
  slideshowButton.setAttribute('style', 'float:left;');
  stream.slideshowMode = false;

  container.appendChild(unsubscribeButton);
  container.appendChild(slideshowButton);
  unsubscribeButton.onclick = () => {
    room.unsubscribe(stream);
    document.getElementById('videoContainer').removeChild(container);
  };
  slideshowButton.onclick = () => {
    stream.updateConfiguration({ slideShowMode: !stream.slideshowMode}, () => {});
    stream.slideshowMode = !stream.slideshowMode;
  };
  document.getElementById('videoContainer').appendChild(container);
  stream.show(`test${stream.getID()}`);

  const unsubscribe_btn = document.getElementById(`subscribe_btn_${stream.getID()}`);
  if (unsubscribe_btn) {
    unsubscribe_btn.disabled = true;
  }
};

const createPublisherContainer = (stream, index) => {
  const container = document.createElement('div');
  container.setAttribute('style', 'width: 320px; height: 280px;float:left;');
  container.setAttribute('id', `container_${index}`);
  const unpublishButton = document.createElement('button');
  unpublishButton.textContent = "Unpublish";
  unpublishButton.setAttribute('style', 'float:left;');

  unpublishButton.onclick = () => {
    room.unpublish(stream);
    document.getElementById('videoContainer').removeChild(container);
  };

  const div = document.createElement('div');
  div.setAttribute('style', 'width: 320px; height: 240px; float:left');
  div.setAttribute('id', `myVideo${index}`);
  container.appendChild(div);
  container.appendChild(unpublishButton);
  document.getElementById('videoContainer').appendChild(container);
};

const getParameterByName = (name) => {
  // eslint-disable-next-line
  name = name.replace(/[\[]/, '\\\[').replace(/[\]]/, '\\\]');
  const regex = new RegExp(`[\\?&]${name}=([^&#]*)`);
  const results = regex.exec(location.search);
  let parameter = configFlags[name];
  if (results !== null) {
    parameter = decodeURIComponent(results[1].replace(/\+/g, ' '));
    if (typeof configFlags[name] === 'boolean') {
      parameter = !!parseInt(parameter, 0);
    }
  }
  return parameter;
};

const fillInConfigFlagsFromParameters = (config) => {
  Object.keys(config).forEach((key) => {
    config[key] = getParameterByName(key);
  });
  console.log('Flags parsed, configuration is ', config);
};

// eslint-disable-next-line no-unused-vars
const testConnection = () => {
  window.location = '/connection_test.html';
};


// eslint-disable-next-line no-unused-vars
function startRecording() {
  if (room !== undefined) {
    if (!recording) {
      room.startRecording(localStream, (id) => {
        recording = true;
        recordingId = id;
        window.recordingId = recordingId;
      });
    } else {
      room.stopRecording(recordingId);
      recording = false;
    }
    window.recording = recording;
  }
}

let slideShowMode = false;

// eslint-disable-next-line no-unused-vars
function toggleSlideShowMode() {
  const streams = room.remoteStreams;
  const cb = (evt) => {
    console.log('SlideShowMode changed', evt);
  };
  slideShowMode = !slideShowMode;
  streams.forEach((stream) => {
    if (localStream.getID() !== stream.getID()) {
      console.log('Updating config');
      stream.updateConfiguration({ slideShowMode }, cb);
    }
  });
}

const publish = (video, audio, screen) => {
  const config = { audio,
    video,
    data: true,
    screen,
    attributes: {} };
  const localStream = Erizo.Stream(config);
  const index = localStreamIndex++;
  localStreams[index] = localStream;
  createPublisherContainer(localStream, index);

  localStream.addEventListener('access-accepted', () => {
    const options = { metadata: { type: 'publisher' } };
      if (configFlags.simulcast) options.simulcast = { numSpatialLayers: 3 };
    room.publish(localStream, options);
    localStream.show(`myVideo${index}`);
  });
  localStream.init();
};

const startBasicExample = () => {
  document.getElementById('startButton').disabled = true;
  document.getElementById('slideShowMode').disabled = false;
  document.getElementById('publishCamera').disabled = false;
  document.getElementById('publishScreen').disabled = false;
  document.getElementById('publishScreenWithAudio').disabled = false;
  document.getElementById('publishOnlyVideo').disabled = false;
  document.getElementById('publishOnlyAudio').disabled = false;
  document.getElementById('startWarning').hidden = true;
  document.getElementById('startButton').hidden = true;
  recording = false;
  console.log('Selected Room', configFlags.room, 'of type', configFlags.type);
  const config = { audio: true,
    video: !configFlags.onlyAudio,
    data: true,
    screen: configFlags.screen,
    attributes: {} };
  // If we want screen sharing we have to put our Chrome extension id.
  // The default one only works in our Lynckia test servers.
  // If we are not using chrome, the creation of the stream will fail regardless.
  if (configFlags.screen) {
    // config.extensionId = 'okeephmleflklcdebijnponpabbmmgeo';
  }
  Erizo.Logger.setLogLevel(Erizo.Logger.TRACE);
  localStream = Erizo.Stream(config);
  window.localStream = localStream;
  const createToken = (roomData, callback) => {
    const req = new XMLHttpRequest();
    const url = `${serverUrl}createToken/`;

    req.onreadystatechange = () => {
      if (req.readyState === 4) {
        callback(req.responseText);
      }
    };

    req.open('POST', url, true);
    req.setRequestHeader('Content-Type', 'application/json');
    req.send(JSON.stringify(roomData));
  };

  const roomData = { username: `user ${parseInt(Math.random() * 100, 10)}`,
    role: 'presenter',
    room: configFlags.room,
    type: configFlags.type,
    mediaConfiguration: configFlags.mediaConfiguration };

  createToken(roomData, (response) => {
    const token = response;
    console.log(token);
    room = Erizo.Room({ token });
    window.room = room;

    const subscribeToStreams = (streams) => {
      streams.forEach((stream) => {
        if (!stream.local) {
          const streamContainer = document.createElement('div');
          streamContainer.setAttribute('id', `stream_element_${stream.getID()}`);
          const subscribeButton = document.createElement('button');
          subscribeButton.textContent = stream.getID();
          subscribeButton.setAttribute('style', 'float:left;');
          subscribeButton.setAttribute('id', `subscribe_btn_${stream.getID()}`);
          subscribeButton.onclick = () => {
            room.subscribe(stream);
          };
          streamContainer.appendChild(subscribeButton);
          document.getElementById('remoteStreamList').appendChild(streamContainer);
        }
      });

      if (configFlags.autoSubscribe) {
        return;
      }
      if (configFlags.onlyPublish) {
        return;
      }
      const cb = (evt) => {
        console.log('Bandwidth Alert', evt.msg, evt.bandwidth);
      };

      streams.forEach((stream) => {
        if (localStream.getID() !== stream.getID()) {
          room.subscribe(stream, { slideShowMode, metadata: { type: 'subscriber' }, video: !configFlags.onlyAudio, encryptTransport: !configFlags.unencrypted });
          stream.addEventListener('bandwidth-alert', cb);
        }
      });
    };
    room.on('connection-failed', console.log.bind(console));

    room.addEventListener('room-connected', (roomEvent) => {
      const options = { metadata: { type: 'publisher' } };
      if (configFlags.simulcast) options.simulcast = { numSpatialLayers: 3 };
      options.encryptTransport = !configFlags.unencrypted;
      subscribeToStreams(roomEvent.streams);

      if (!configFlags.onlySubscribe) {
        room.publish(localStream, options);
      }
      room.addEventListener('quality-level', (qualityEvt) => {
        console.log(`New Quality Event, connection quality: ${qualityEvt.message}`);
      });
    });

    room.addEventListener('stream-subscribed', (streamEvent) => {
      const stream = streamEvent.stream;
      createSubscriberContainer(stream);
    });

    room.addEventListener('stream-unsubscribed', (streamEvent) => {
      const stream = streamEvent.stream;
      const unsubscribe_btn = document.getElementById(`subscribe_btn_${stream.getID()}`);
      if (unsubscribe_btn) {
        unsubscribe_btn.disabled = false;
      }
    });

    room.addEventListener('stream-added', (streamEvent) => {
      const streams = [];
      streams.push(streamEvent.stream);
      if (localStream) {
        localStream.setAttributes({ type: 'publisher' });
      }
      subscribeToStreams(streams);
      document.getElementById('recordButton').disabled = false;
    });

    room.addEventListener('stream-removed', (streamEvent) => {
      // Remove stream from DOM
      const stream = streamEvent.stream;
      if (stream.elementID !== undefined) {
        const element = document.getElementById(`container_${stream.getID()}`);
        if (element) {
          document.getElementById('videoContainer').removeChild(element);
        }
      }
      const streamContainer = document.getElementById(`stream_element_${stream.getID()}`);
      console.log('Removing', `stream_element_${stream.getID()}`);
      if (streamContainer) {
        document.getElementById('remoteStreamList').removeChild(streamContainer);
      }
    });

    room.addEventListener('stream-failed', () => {
      console.log('Stream Failed, act accordingly');
    });

    if (configFlags.onlySubscribe) {
      room.connect({ singlePC: configFlags.singlePC });
    } else {
      createPublisherContainer(localStream, '');

      localStream.addEventListener('access-accepted', () => {
        room.connect({ singlePC: configFlags.singlePC });
        localStream.show('myVideo');
      });
      localStream.init();
    }
  });
};

window.onload = () => {
  fillInConfigFlagsFromParameters(configFlags);
  window.configFlags = configFlags;

  const shouldSkipButton =
    !configFlags.forceStart &&
    (!configFlags.onlySubscribe || configFlags.noStart);

  if (shouldSkipButton) {
    startBasicExample();
  } else {
    document.getElementById('startButton').disabled = false;
  }
};
