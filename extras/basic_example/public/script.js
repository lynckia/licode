
/* globals Erizo */

/* eslint-env browser */
/* eslint-disable no-param-reassign, no-console */

const serverUrl = '/';
window.localStream = undefined;
window.room = undefined;
window.recording = false;
window.recordingId = '';
window.configFlags = {
  noStart: false, // disable start button when only subscribe
  forceStart: false, // force start button in all cases
  screen: false, // screensharinug
  room: 'basicExampleRoom', // room name
  singlePC: false,
  type: 'erizo', // room type
  onlyAudio: false,
  mediaConfiguration: 'default',
  onlySubscribe: false,
  onlyPublish: false,
  autoSubscribe: false,
  offerFromErizo: false,
  simulcast: false,
};

const getParameterByName = (name) => {
  // eslint-disable-next-line
  name = name.replace(/[\[]/, '\\\[').replace(/[\]]/, '\\\]');
  const regex = new RegExp(`[\\?&]${name}=([^&#]*)`);
  const results = regex.exec(location.search);
  return results == null ? false : decodeURIComponent(results[1].replace(/\+/g, ' '));
};

const getConfigFlagsFromParameters = () => {
  Object.keys(window.configFlags).forEach((key) => {
    window.configFlags[key] = getParameterByName(key);
  });
  console.log('Flags parsed, configuration is ', window.configFlags);
};

// eslint-disable-next-line no-unused-vars
const testConnection = () => {
  window.location = '/connection_test.html';
};


// eslint-disable-next-line no-unused-vars
function startRecording() {
  if (window.room !== undefined) {
    if (!window.recording) {
      window.room.startRecording(window.localStream, (id) => {
        window.recording = true;
        window.recordingId = id;
      });
    } else {
      window.room.stopRecording(window.recordingId);
      window.recording = false;
    }
  }
}

let slideShowMode = false;

// eslint-disable-next-line no-unused-vars
function toggleSlideShowMode() {
  const streams = window.room.remoteStreams;
  const cb = (evt) => {
    console.log('SlideShowMode changed', evt);
  };
  slideShowMode = !slideShowMode;
  streams.forEach((stream) => {
    if (window.localStream.getID() !== stream.getID()) {
      console.log('Updating config');
      stream.updateConfiguration({ slideShowMode }, cb);
    }
  });
}

const startBasicExample = () => {
  document.getElementById('startButton').disabled = true;
  document.getElementById('slideShowMode').disabled = false;
  document.getElementById('startWarning').hidden = true;
  document.getElementById('startButton').hidden = true;
  window.recording = false;
  console.log('Selected Room', window.configFlags.room, 'of type', window.configFlags.type);
  const config = { audio: true,
    video: !window.configFlags.onlyAudio,
    data: true,
    screen: window.configFlags.screen,
    attributes: {} };
  // If we want screen sharing we have to put our Chrome extension id.
  // The default one only works in our Lynckia test servers.
  // If we are not using chrome, the creation of the stream will fail regardless.
  if (window.configFlags.screen) {
    config.extensionId = 'okeephmleflklcdebijnponpabbmmgeo';
  }
  Erizo.Logger.setLogLevel(Erizo.Logger.INFO);
  window.localStream = Erizo.Stream(config);
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
    room: window.configFlags.room,
    type: window.configFlags.type,
    mediaConfiguration: window.configFlags.mediaConfiguration };

  createToken(roomData, (response) => {
    const token = response;
    console.log(token);
    window.room = Erizo.Room({ token });

    const subscribeToStreams = (streams) => {
      if (window.configFlags.autoSubscribe) {
        return;
      }
      if (window.configFlags.onlyPublish) {
        return;
      }
      const cb = (evt) => {
        console.log('Bandwidth Alert', evt.msg, evt.bandwidth);
      };

      streams.forEach((stream) => {
        if (window.localStream.getID() !== stream.getID()) {
          window.room.subscribe(stream, { slideShowMode, metadata: { type: 'subscriber' }, offerFromErizo: window.configFlags.offerFromErizo });
          stream.addEventListener('bandwidth-alert', cb);
        }
      });
    };

    window.room.addEventListener('room-connected', (roomEvent) => {
      const options = { metadata: { type: 'publisher' } };
      if (window.configFlags.simulcast) options.simulcast = { numSpatialLayers: 2 };
      subscribeToStreams(roomEvent.streams);

      if (!window.configFlags.onlySubscribe) {
        window.room.publish(window.localStream, options);
      }
      window.room.addEventListener('quality-level', (qualityEvt) => {
        console.log(`New Quality Event, connection quality: ${qualityEvt.message}`);
      });
      if (window.configFlags.autoSubscribe) {
        window.room.autoSubscribe({ '/attributes/type': 'publisher' }, {}, { audio: true, video: true, data: false }, () => {});
      }
    });

    window.room.addEventListener('stream-subscribed', (streamEvent) => {
      const stream = streamEvent.stream;
      const div = document.createElement('div');
      div.setAttribute('style', 'width: 320px; height: 240px;float:left;');
      div.setAttribute('id', `test${stream.getID()}`);

      document.getElementById('videoContainer').appendChild(div);
      stream.show(`test${stream.getID()}`);
    });

    window.room.addEventListener('stream-added', (streamEvent) => {
      const streams = [];
      streams.push(streamEvent.stream);
      if (window.localStream) {
        window.localStream.setAttributes({ type: 'publisher' });
      }
      subscribeToStreams(streams);
      document.getElementById('recordButton').disabled = false;
    });

    window.room.addEventListener('stream-removed', (streamEvent) => {
      // Remove stream from DOM
      const stream = streamEvent.stream;
      if (stream.elementID !== undefined) {
        const element = document.getElementById(stream.elementID);
        document.getElementById('videoContainer').removeChild(element);
      }
    });

    window.room.addEventListener('stream-failed', () => {
      console.log('Stream Failed, act accordingly');
    });

    if (window.configFlags.onlySubscribe) {
      window.room.connect({ singlePC: window.configFlags.singlePC });
    } else {
      const div = document.createElement('div');
      div.setAttribute('style', 'width: 320px; height: 240px; float:left');
      div.setAttribute('id', 'myVideo');
      document.getElementById('videoContainer').appendChild(div);

      window.localStream.addEventListener('access-accepted', () => {
        window.room.connect({ singlePC: window.configFlags.singlePC });
        window.localStream.show('myVideo');
      });
      window.localStream.init();
    }
  });
};

window.onload = () => {
  getConfigFlagsFromParameters();

  const shouldSkipButton =
    !window.configFlags.forceStart &&
    (!window.configFlags.onlySubscribe || window.configFlags.noStart);

  if (shouldSkipButton) {
    startBasicExample();
  } else {
    document.getElementById('startButton').disabled = false;
  }
};
