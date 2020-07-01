
/* globals Erizo */

/* eslint-env browser */
/* eslint-disable no-param-reassign, no-console */

const serverUrl = '/';
let localStream;
let room;
let recording = false;
let recordingId = '';
const configFlags = {
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

const startBasicExample = () => {
  document.getElementById('startButton').disabled = true;
  document.getElementById('slideShowMode').disabled = false;
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
    config.extensionId = 'okeephmleflklcdebijnponpabbmmgeo';
  }
  Erizo.Logger.setLogLevel(Erizo.Logger.INFO);
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
          room.subscribe(stream, { slideShowMode, metadata: { type: 'subscriber' }, offerFromErizo: configFlags.offerFromErizo });
          stream.addEventListener('bandwidth-alert', cb);
        }
      });
    };

    room.addEventListener('room-connected', (roomEvent) => {
      const options = { metadata: { type: 'publisher' } };
      if (configFlags.simulcast) options.simulcast = { numSpatialLayers: 2 };
      subscribeToStreams(roomEvent.streams);

      if (!configFlags.onlySubscribe) {
        room.publish(localStream, options);
      }
      room.addEventListener('quality-level', (qualityEvt) => {
        console.log(`New Quality Event, connection quality: ${qualityEvt.message}`);
      });
      if (configFlags.autoSubscribe) {
        room.autoSubscribe({ '/attributes/type': 'publisher' }, {}, { audio: true, video: true, data: false }, () => {});
      }
    });

    room.addEventListener('stream-subscribed', (streamEvent) => {
      const stream = streamEvent.stream;
      const div = document.createElement('div');
      div.setAttribute('style', 'width: 320px; height: 240px;float:left;');
      div.setAttribute('id', `test${stream.getID()}`);

      document.getElementById('videoContainer').appendChild(div);
      stream.show(`test${stream.getID()}`);
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
        const element = document.getElementById(stream.elementID);
        document.getElementById('videoContainer').removeChild(element);
      }
    });

    room.addEventListener('stream-failed', () => {
      console.log('Stream Failed, act accordingly');
    });

    if (configFlags.onlySubscribe) {
      room.connect({ singlePC: configFlags.singlePC });
    } else {
      const div = document.createElement('div');
      div.setAttribute('style', 'width: 320px; height: 240px; float:left');
      div.setAttribute('id', 'myVideo');
      document.getElementById('videoContainer').appendChild(div);

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
