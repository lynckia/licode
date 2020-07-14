/* global document */

import { EventDispatcher, StreamEvent } from './Events';
import ConnectionHelpers from './utils/ConnectionHelpers';
import ErizoMap from './utils/ErizoMap';
import Random from './utils/Random';
import VideoPlayer from './views/VideoPlayer';
import AudioPlayer from './views/AudioPlayer';
import Logger from './utils/Logger';

const log = Logger.module('Stream');

/*
 * Class Stream represents a local or a remote Stream in the Room. It will handle the WebRTC
 * stream and identify the stream and where it should be drawn.
 */
const Stream = (altConnectionHelpers, specInput) => {
  const spec = specInput;
  const that = EventDispatcher(spec);

  that.stream = spec.stream;
  that.url = spec.url;
  that.recording = spec.recording;
  that.room = undefined;
  that.showing = false;
  that.local = false;
  that.video = spec.video;
  that.audio = spec.audio;
  that.screen = spec.screen;
  that.videoSize = spec.videoSize;
  that.videoFrameRate = spec.videoFrameRate;
  that.extensionId = spec.extensionId;
  that.desktopStreamId = spec.desktopStreamId;
  that.audioMuted = false;
  that.videoMuted = false;
  that.unsubscribing = {
    callbackReceived: false,
    pcEventReceived: false,
  };
  that.p2p = false;
  that.ConnectionHelpers =
    altConnectionHelpers === undefined ? ConnectionHelpers : altConnectionHelpers;
  if (that.url !== undefined) {
    spec.label = `ei_${Random.getRandomValue()}`;
  }
  const onStreamAddedToPC = (evt) => {
    if (evt.stream.id === that.getLabel()) {
      that.emit(StreamEvent({ type: 'added', stream: evt.stream }));
    }
  };

  const onStreamRemovedFromPC = (evt) => {
    if (evt.stream.id === that.getLabel()) {
      that.emit(StreamEvent({ type: 'removed', stream: that }));
    }
  };

  const onICEConnectionStateChange = (msg) => {
    that.emit(StreamEvent({ type: 'icestatechanged', msg }));
  };

  if (that.videoSize !== undefined &&
        (!(that.videoSize instanceof Array) ||
           that.videoSize.length !== 4)) {
    throw Error('Invalid Video Size');
  }
  if (spec.local === undefined || spec.local === true) {
    that.local = true;
  }

  // Public functions
  that.getID = () => {
    let id;
    // Unpublished local streams don't yet have an ID.
    if (that.local && !spec.streamID) {
      id = 'local';
    } else {
      id = spec.streamID;
    }
    return id;
  };

  that.getLabel = () => {
    if (that.stream && that.stream.id) {
      return that.stream.id;
    }
    return spec.label;
  };

  // Get attributes of this stream.
  that.getAttributes = () => spec.attributes;

  // Changes the attributes of this stream in the room.
  that.setAttributes = (attrs) => {
    if (that.local) {
      that.emit(StreamEvent({ type: 'internal-set-attributes', stream: that, attrs }));
      return;
    }
    log.error(`message: Failed to set attributes data, reason: Stream has not been published, ${that.toLog()}`);
  };

  that.toLog = () => {
    let info = `streamId: ${that.getID()}, label: ${that.getLabel()}`;
    const attrKeys = Object.keys(spec.attributes);
    attrKeys.forEach((attrKey) => {
      info = `${info}, ${attrKey}: ${spec.attributes[attrKey]}`;
    });
    return info;
  };

  that.updateLocalAttributes = (attrs) => {
    spec.attributes = attrs;
  };

  // Indicates if the stream has audio activated
  that.hasAudio = () => spec.audio !== false && spec.audio !== undefined;

  // Indicates if the stream has video activated
  that.hasVideo = () => spec.video !== false && spec.video !== undefined;

  // Indicates if the stream has data activated
  that.hasData = () => spec.data !== false && spec.data !== undefined;

  // Indicates if the stream has screen activated
  that.hasScreen = () => spec.screen;

  that.hasMedia = () => spec.audio || spec.video || spec.screen;

  that.isExternal = () => that.url !== undefined || that.recording !== undefined;

  that.addPC = (pc, p2pKey = undefined) => {
    if (p2pKey) {
      that.p2p = true;
      if (that.pc === undefined) {
        that.pc = ErizoMap();
      }
      that.pc.add(p2pKey, pc);
      pc.on('ice-state-change', onICEConnectionStateChange);
      return;
    }
    if (that.pc) {
      that.pc.off('add-stream', onStreamAddedToPC);
      that.pc.off('remove-stream', onStreamRemovedFromPC);
      that.pc.off('ice-state-change', onICEConnectionStateChange);
    }
    that.pc = pc;
    that.pc.on('add-stream', onStreamAddedToPC);
    that.pc.on('remove-stream', onStreamRemovedFromPC);
    that.pc.on('ice-state-change', onICEConnectionStateChange);
  };

  // Sends data through this stream.
  that.sendData = (msg) => {
    if (that.local && that.hasData()) {
      that.emit(StreamEvent({ type: 'internal-send-data', stream: that, msg }));
      return;
    }
    log.error(`message: Failed to send data, reason: Stream has not been published, ${that.toLog()}`);
  };

  // Initializes the stream and tries to retrieve a stream from local video and audio
  // We need to call this method before we can publish it in the room.
  that.init = () => {
    try {
      if ((spec.audio || spec.video || spec.screen) && spec.url === undefined) {
        log.debug(`message: Requested access to local media, ${that.toLog()}`);
        let videoOpt = spec.video;
        if (videoOpt === true || spec.screen === true) {
          videoOpt = videoOpt === true || videoOpt === null ? {} : videoOpt;
          if (that.videoSize !== undefined) {
            videoOpt.width = {
              min: that.videoSize[0],
              max: that.videoSize[2],
            };

            videoOpt.height = {
              min: that.videoSize[1],
              max: that.videoSize[3],
            };
          }

          if (that.videoFrameRate !== undefined) {
            videoOpt.frameRate = {
              min: that.videoFrameRate[0],
              max: that.videoFrameRate[1],
            };
          }
        } else if (spec.screen === true && videoOpt === undefined) {
          videoOpt = true;
        }
        const opt = { video: videoOpt,
          audio: spec.audio,
          fake: spec.fake,
          screen: spec.screen,
          extensionId: that.extensionId,
          desktopStreamId: that.desktopStreamId };

        that.ConnectionHelpers.GetUserMedia(opt, (stream) => {
          log.debug(`message: User has granted access to local media, ${that.toLog()}`);
          that.stream = stream;

          that.dispatchEvent(StreamEvent({ type: 'access-accepted' }));

          that.stream.getTracks().forEach((trackInput) => {
            log.debug(`message: getTracks, track: ${trackInput.kind}, ${that.toLog()}`);
            const track = trackInput;
            track.onended = () => {
              that.stream.getTracks().forEach((secondTrackInput) => {
                const secondTrack = secondTrackInput;
                secondTrack.onended = null;
              });
              const streamEvent = StreamEvent({ type: 'stream-ended',
                stream: that,
                msg: track.kind });
              that.dispatchEvent(streamEvent);
            };
          });
        }, (error) => {
          log.error(`message: Failed to get access to local media, ${that.toLog()}, error: ${error.name}, message: ${error.message}`);
          const streamEvent = StreamEvent({ type: 'access-denied', msg: error });
          that.dispatchEvent(streamEvent);
        });
      } else {
        const streamEvent = StreamEvent({ type: 'access-accepted' });
        that.dispatchEvent(streamEvent);
      }
    } catch (e) {
      log.error(`message: Failed to get access to local media, ${that.toLog()}, error: ${e}`);
      const streamEvent = StreamEvent({ type: 'access-denied', msg: e });
      that.dispatchEvent(streamEvent);
    }
  };


  that.close = () => {
    if (that.local) {
      if (that.room !== undefined) {
        that.room.unpublish(that);
      }
      // Remove HTML element
      that.hide();
      if (that.stream !== undefined) {
        that.stream.getTracks().forEach((trackInput) => {
          const track = trackInput;
          track.onended = null;
          track.stop();
        });
      }
      that.stream = undefined;
    }
    if (that.pc && !that.p2p) {
      that.pc.off('add-stream', onStreamAddedToPC);
      that.pc.off('remove-stream', onStreamRemovedFromPC);
      that.pc.off('ice-state-change', onICEConnectionStateChange);
    } else if (that.pc && that.p2p) {
      that.pc.forEach((pc) => {
        pc.off('add-stream', onStreamAddedToPC);
        pc.off('remove-stream', onStreamRemovedFromPC);
        pc.off('ice-state-change', onICEConnectionStateChange);
      });
    }
    that.removeAllListeners();
  };

  that.play = (elementID, optionsInput) => {
    const options = optionsInput || {};
    that.elementID = elementID;
    let player;
    if (that.hasVideo() || that.hasScreen()) {
      // Draw on HTML
      if (elementID !== undefined) {
        player = VideoPlayer({ id: that.getID(),
          stream: that,
          elementID,
          options });
        that.player = player;
        that.showing = true;
      }
    } else if (that.hasAudio()) {
      player = AudioPlayer({ id: that.getID(),
        stream: that,
        elementID,
        options });
      that.player = player;
      that.showing = true;
    }
  };

  that.stop = () => {
    if (that.showing) {
      if (that.player !== undefined) {
        try {
          that.player.destroy();
        } catch (e) {
          log.warning(`message: Exception when destroying Player, error: ${e.message}, ${that.toLog()}`);
        }
        that.showing = false;
      }
    }
  };

  that.show = that.play;
  that.hide = that.stop;

  const getFrame = () => {
    if (that.player !== undefined && that.stream !== undefined) {
      const video = that.player.video;
      const style = document.defaultView.getComputedStyle(video);
      const width = parseInt(style.getPropertyValue('width'), 10);
      const height = parseInt(style.getPropertyValue('height'), 10);
      const left = parseInt(style.getPropertyValue('left'), 10);
      const top = parseInt(style.getPropertyValue('top'), 10);

      let div;
      if (typeof that.elementID === 'object' &&
              typeof that.elementID.appendChild === 'function') {
        div = that.elementID;
      } else {
        div = document.getElementById(that.elementID);
      }

      const divStyle = document.defaultView.getComputedStyle(div);
      const divWidth = parseInt(divStyle.getPropertyValue('width'), 10);
      const divHeight = parseInt(divStyle.getPropertyValue('height'), 10);
      const canvas = document.createElement('canvas');

      canvas.id = 'testing';
      canvas.width = divWidth;
      canvas.height = divHeight;
      canvas.setAttribute('style', 'display: none');
      // document.body.appendChild(canvas);
      const context = canvas.getContext('2d');

      context.drawImage(video, left, top, width, height);

      return canvas;
    }
    return null;
  };

  that.getVideoFrameURL = (format) => {
    const canvas = getFrame();
    if (canvas !== null) {
      if (format) {
        return canvas.toDataURL(format);
      }
      return canvas.toDataURL();
    }
    return null;
  };

  that.getVideoFrame = () => {
    const canvas = getFrame();
    if (canvas !== null) {
      return canvas.getContext('2d').getImageData(0, 0, canvas.width, canvas.height);
    }
    return null;
  };

  that.checkOptions = (configInput, isUpdate) => {
    const config = configInput;
    // TODO: Check for any incompatible options
    if (isUpdate === true) { // We are updating the stream
      if (config.audio || config.screen) {
        log.warning(`message: Cannot update type of subscription, ${that.toLog()}`);
        config.audio = undefined;
        config.screen = undefined;
      }
    } else if (that.local === false) { // check what we can subscribe to
      if (config.video === true && that.hasVideo() === false) {
        log.warning(`message: Trying to subscribe to video when there is no video, ${that.toLog()}`);
        config.video = false;
      }
      if (config.audio === true && that.hasAudio() === false) {
        log.warning(`message: Trying to subscribe to audio when there is no audio, ${that.toLog()}`);
        config.audio = false;
      }
    }
    if (that.local === false) {
      if (!that.hasVideo() && (config.slideShowMode === true)) {
        log.warning(`message: Cannot enable slideShowMode without video, ${that.toLog()}`);
        config.slideShowMode = false;
      }
    }
  };

  const muteStream = (callback = () => {}) => {
    if (that.room && that.room.p2p) {
      log.warning(`message: muteAudio/muteVideo are not implemented in p2p streams, ${that.toLog()}`);
      callback('error');
      return;
    }
    if (!that.stream || !that.pc) {
      log.warning(`message: muteAudio/muteVideo cannot be called until a stream is published or subscribed, ${that.toLog()}`);
      callback('error');
    }
    for (let index = 0; index < that.stream.getVideoTracks().length; index += 1) {
      const track = that.stream.getVideoTracks()[index];
      track.enabled = !that.videoMuted;
    }
    const config = { muteStream: { audio: that.audioMuted, video: that.videoMuted } };
    that.checkOptions(config, true);
    that.pc.updateSpec(config, that.getID(), callback);
  };

  that.muteAudio = (isMuted, callback = () => {}) => {
    that.audioMuted = isMuted;
    muteStream(callback);
  };

  that.muteVideo = (isMuted, callback = () => {}) => {
    that.videoMuted = isMuted;
    muteStream(callback);
  };

  // eslint-disable-next-line no-underscore-dangle
  that._setStaticQualityLayer = (spatialLayer, temporalLayer, callback = () => {}) => {
    if (that.room && that.room.p2p) {
      log.warning(`message: setStaticQualityLayer is not implemented in p2p streams, ${that.toLog()}`);
      callback('error');
      return;
    }
    const config = { qualityLayer: { spatialLayer, temporalLayer } };
    that.checkOptions(config, true);
    that.pc.updateSpec(config, that.getID(), callback);
  };

  // eslint-disable-next-line no-underscore-dangle
  that._setDynamicQualityLayer = (callback) => {
    if (that.room && that.room.p2p) {
      log.warning(`message: setDynamicQualityLayer is not implemented in p2p streams, ${that.toLog()}`);
      callback('error');
      return;
    }
    const config = { qualityLayer: { spatialLayer: -1, temporalLayer: -1 } };
    that.checkOptions(config, true);
    that.pc.updateSpec(config, that.getID(), callback);
  };

  // eslint-disable-next-line no-underscore-dangle
  that._enableSlideShowBelowSpatialLayer = (enabled, spatialLayer = 0, callback = () => {}) => {
    if (that.room && that.room.p2p) {
      log.warning(`message: enableSlideShowBelowSpatialLayer is not implemented in p2p streams, ${that.toLog()}`);
      callback('error');
      return;
    }
    const config = { slideShowBelowLayer: { enabled, spatialLayer } };
    that.checkOptions(config, true);
    log.debug(`message: Calling updateSpec, ${that.toLog()}, config: ${JSON.stringify(config)}`);
    that.pc.updateSpec(config, that.getID(), callback);
  };

  // This is an alias to keep backwards compatibility
  // eslint-disable-next-line no-underscore-dangle
  that._setMinSpatialLayer = that._enableSlideShowBelowSpatialLayer.bind(this, true);

  const controlHandler = (handlersInput, publisherSideInput, enable) => {
    let publisherSide = publisherSideInput;
    let handlers = handlersInput;
    if (publisherSide !== true) {
      publisherSide = false;
    }

    handlers = (typeof handlers === 'string') ? [handlers] : handlers;
    handlers = (handlers instanceof Array) ? handlers : [];

    if (handlers.length > 0) {
      that.room.sendControlMessage(that, 'control', { name: 'controlhandlers',
        enable,
        publisherSide,
        handlers });
    }
  };

  that.disableHandlers = (handlers, publisherSide) => {
    controlHandler(handlers, publisherSide, false);
  };

  that.enableHandlers = (handlers, publisherSide) => {
    controlHandler(handlers, publisherSide, true);
  };

  that.updateSimulcastLayersBitrate = (bitrates) => {
    if (that.pc && that.local) {
      that.pc.updateSimulcastLayersBitrate(bitrates);
    }
  };

  that.updateSimulcastActiveLayers = (layersInfo) => {
    if (that.pc && that.local) {
      that.pc.updateSimulcastActiveLayers(layersInfo);
    }
  };

  that.updateConfiguration = (config, callback = () => {}) => {
    if (config === undefined) { return; }
    if (that.pc) {
      that.checkOptions(config, true);
      if (that.local) {
        if (that.room.p2p) {
          for (let index = 0; index < that.pc.length; index += 1) {
            that.pc[index].updateSpec(config, that.getID(), callback);
          }
        } else {
          that.pc.updateSpec(config, that.getID(), callback);
        }
      } else {
        that.pc.updateSpec(config, that.getID(), callback);
      }
    } else {
      callback('This stream has no peerConnection attached, ignoring');
    }
  };

  return that;
};

export default Stream;
