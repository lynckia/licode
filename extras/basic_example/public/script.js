
/* globals Erizo */

/* eslint-env browser */
/* eslint-disable no-param-reassign, no-console */

const serverUrl = '/';
let localStream;
let room;
let recording;
let recordingId;

const getParameterByName = (name) => {
  // eslint-disable-next-line
  name = name.replace(/[\[]/, '\\\[').replace(/[\]]/, '\\\]');
  const regex = new RegExp(`[\\?&]${name}=([^&#]*)`);
  const results = regex.exec(location.search);
  return results == null ? '' : decodeURIComponent(results[1].replace(/\+/g, ' '));
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
      });
    } else {
      room.stopRecording(recordingId);
      recording = false;
    }
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
  const screen = getParameterByName('screen');
  const roomName = getParameterByName('room') || 'basicExampleRoom';
  const singlePC = getParameterByName('singlePC') || false;
  const roomType = getParameterByName('type') || 'erizo';
  const audioOnly = getParameterByName('onlyAudio') || false;
  const mediaConfiguration = getParameterByName('mediaConfiguration') || 'default';
  const onlySubscribe = getParameterByName('onlySubscribe');
  const onlyPublish = getParameterByName('onlyPublish');
  const autoSubscribe = getParameterByName('autoSubscribe');
  const offerFromErizo = getParameterByName('offerFromErizo');
  console.log('Selected Room', roomName, 'of type', roomType);
  const config = { audio: true,
    video: !audioOnly,
    data: true,
    screen,
    attributes: {} };
  // If we want screen sharing we have to put our Chrome extension id.
  // The default one only works in our Lynckia test servers.
  // If we are not using chrome, the creation of the stream will fail regardless.
  if (screen) {
    config.extensionId = 'okeephmleflklcdebijnponpabbmmgeo';
  }
  Erizo.Logger.setLogLevel(Erizo.Logger.INFO);
  localStream = Erizo.Stream(config);
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
    room: roomName,
    type: roomType,
    mediaConfiguration };

  createToken(roomData, (response) => {
    const token = response;
    console.log(token);
    room = Erizo.Room({ token });

    const subscribeToStreams = (streams) => {
      if (autoSubscribe) {
        return;
      }
      if (onlyPublish) {
        return;
      }
      const cb = (evt) => {
        console.log('Bandwidth Alert', evt.msg, evt.bandwidth);
      };

      streams.forEach((stream) => {
        if (localStream.getID() !== stream.getID()) {
          room.subscribe(stream, { slideShowMode, metadata: { type: 'subscriber' }, offerFromErizo });
          stream.addEventListener('bandwidth-alert', cb);
        }
      });
    };

    room.addEventListener('room-connected', (roomEvent) => {
      const options = { metadata: { type: 'publisher' } };
      const enableSimulcast = getParameterByName('simulcast');
      if (enableSimulcast) options.simulcast = { numSpatialLayers: 2 };
      subscribeToStreams(roomEvent.streams);

      if (!onlySubscribe) {
        room.publish(localStream, options);
      }
      room.addEventListener('quality-level', (qualityEvt) => {
        console.log(`New Quality Event, connection quality: ${qualityEvt.message}`);
      });
      if (autoSubscribe) {
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

    if (onlySubscribe) {
      room.connect({ singlePC });
    } else {
      const div = document.createElement('div');
      div.setAttribute('style', 'width: 320px; height: 240px; float:left');
      div.setAttribute('id', 'myVideo');
      document.getElementById('videoContainer').appendChild(div);

      localStream.addEventListener('access-accepted', () => {
        room.connect({ singlePC });
        localStream.show('myVideo');
      });
      localStream.init();
    }
  });
};

window.onload = () => {
  const onlySubscribe = getParameterByName('onlySubscribe');
  const bypassStartButton = getParameterByName('noStart');
  if (!onlySubscribe || bypassStartButton) {
    startBasicExample();
  } else {
    document.getElementById('startButton').disabled = false;
  }
};
