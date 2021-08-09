
/* globals Erizo */

/* eslint-env browser */
/* eslint-disable no-param-reassign, no-console */

const serverUrl = '/';
let localStream;
let dataStream;
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
  numVideoTiles: 3,
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
    stream.updateConfiguration({ slideShowMode: !stream.slideshowMode }, () => {});
    stream.slideshowMode = !stream.slideshowMode;
  };
  document.getElementById('videoContainer').appendChild(container);
  stream.show(`test${stream.getID()}`);

  const unsubscribeBtn = document.getElementById(`subscribe_btn_${stream.getID()}`);
  if (unsubscribeBtn) {
    unsubscribeBtn.disabled = true;
  }
};

const createPublisherContainer = (stream, index) => {
  const container = document.createElement('div');
  container.setAttribute('style', 'width: 320px; height: 280px;float:left;');
  container.setAttribute('id', `container_${index}`);
  const unpublishButton = document.createElement('button');
  unpublishButton.textContent = 'Unpublish';
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

// eslint-disable-next-line no-unused-vars
const publish = (video, audio, screen) => {
  const config = { audio,
    video,
    data: true,
    screen,
    attributes: {} };
  const stream = Erizo.Stream(config);
  const index = localStreamIndex;
  localStreamIndex += 1;
  localStreams[index] = stream;
  createPublisherContainer(stream, index);

  stream.addEventListener('access-accepted', () => {
    const options = { metadata: { type: 'publisher' } };
    if (configFlags.simulcast) options.simulcast = { numSpatialLayers: 3 };
    room.publish(stream, options);
    stream.show(`myVideo${index}`);
  });
  stream.init();
};

const createVideoTile = () => {
  room.createVideoTiles(configFlags.numVideoTiles, {});
};

const assignVideoTiles = (streamIds) => {
  console.log('Assigning video tiles', streamIds);
  const streams = streamIds.map(streamId => room.remoteStreams.get(streamId));
  room.assignStreamsToVideoTiles(streams);
};

function shuffle(array) {
  let currentIndex = array.length;
  let randomIndex;

  // While there remain elements to shuffle...
  while (currentIndex !== 0) {
    // Pick a remaining element...
    randomIndex = Math.floor(Math.random() * currentIndex);
    currentIndex -= 1;

    // And swap it with the current element.
    [array[currentIndex], array[randomIndex]] = [
      array[randomIndex], array[currentIndex]];
  }

  return array;
}

const onDataFromStream = (stream, evt) => {
  console.log(`Event from ${stream.getID()}, evt: ${JSON.stringify(evt.msg)}`);
  if (evt.msg && evt.msg.startsWith('shuffle-video-tile-')) {
    let streamIds = evt.msg.replace('shuffle-video-tile-', '');
    streamIds = streamIds.split(', ');
    assignVideoTiles(streamIds);
  }
};

// eslint-disable-next-line no-unused-vars
const applyAssignVideoTilesToEveryone = () => {
  let streamIds = [];
  const numVideoTiles = configFlags.numVideoTiles;
  room.remoteStreams.forEach((stream) => {
    if (stream.hasMedia()) {
      streamIds.push(stream.getID());
    }
  });

  shuffle(streamIds);
  streamIds = streamIds.splice(0, numVideoTiles);
  while (streamIds.length < numVideoTiles) {
    streamIds.push(undefined);
  }
  console.log(streamIds);
  shuffle(streamIds);

  const msg = `shuffle-video-tile-${streamIds.join(', ')}`;
  console.log('Applying video tiles to all', msg, dataStream.hasData());

  dataStream.sendData(msg);
  assignVideoTiles(streamIds);
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
  dataStream = Erizo.Stream({ audio: false, video: false, screen: false, data: true });
  window.localStream = localStream;
  window.dataStream = dataStream;
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
        if (!stream.local && stream.hasMedia()) {
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
        } else if (!stream.local && !stream.hasMedia() && stream.hasData()) {
          room.subscribe(stream);
          console.log('Subscribing to stream-data');
          stream.addEventListener('stream-data', onDataFromStream.bind(null, stream));
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
        if (!stream.local) {
          room.subscribe(stream, { slideShowMode, metadata: { type: 'subscriber' }, video: !configFlags.onlyAudio, encryptTransport: !configFlags.unencrypted });
          stream.addEventListener('bandwidth-alert', cb);
        } else if (!stream.local && !stream.hasMedia() && stream.hasData()) {
          room.subscribe(stream);
          console.log('Subscribing to stream-data');
          stream.addEventListener('stream-data', onDataFromStream.bind(null, stream));
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
      room.publish(dataStream);
      room.addEventListener('quality-level', (qualityEvt) => {
        console.log(`New Quality Event, connection quality: ${qualityEvt.message}`);
      });
      createVideoTile();
    });

    room.addEventListener('tile-added', (roomEvent) => {
      console.log('Tile added');
      const streams = roomEvent.streams;
      const stream = streams[0];
      const div = document.createElement('div');
      div.setAttribute('style', 'width: 320px; height: 240px;float:left;');
      div.setAttribute('id', `test${stream.id}`);

      const player = document.createElement('div');
      player.setAttribute('style', 'width: 100%; height: 100%; position: relative; background-color: black; overflow: hidden;');
      player.setAttribute('id', `player${stream.id}`);

      const video = document.createElement('video');
      video.setAttribute('id', `stream${stream.id}`);
      video.setAttribute('class', 'licode_stream');
      video.setAttribute('style', 'width: 100%; height: 100%; position: absolute; object-fit: cover');
      video.setAttribute('autoplay', 'autoplay');
      video.setAttribute('playsinline', 'playsinline');
      video.srcObject = new MediaStream(stream.getVideoTracks());

      const audio = document.createElement('audio');
      audio.setAttribute('id', `stream_audio_${stream.id}`);
      audio.setAttribute('class', 'licode_stream');
      audio.setAttribute('style', 'width: 0%; height: 0%; position: absolute; object-fit: cover');
      audio.setAttribute('autoplay', 'autoplay');
      audio.setAttribute('playsinline', 'playsinline');
      audio.srcObject = new MediaStream(stream.getAudioTracks());

      div.appendChild(player);
      player.appendChild(video);
      player.appendChild(audio);

      document.getElementById('videoTileContainer').appendChild(div);
    });

    room.addEventListener('stream-subscribed', (streamEvent) => {
      const stream = streamEvent.stream;
      if (stream.hasMedia()) {
        createSubscriberContainer(stream);
      }
    });

    room.addEventListener('stream-unsubscribed', (streamEvent) => {
      const stream = streamEvent.stream;
      const unsubscribeBtn = document.getElementById(`subscribe_btn_${stream.getID()}`);
      if (unsubscribeBtn) {
        unsubscribeBtn.disabled = false;
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
