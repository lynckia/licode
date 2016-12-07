/*global L, document*/
'use strict';
/*
 * Class Stream represents a local or a remote Stream in the Room. It will handle the WebRTC stream
 * and identify the stream and where it should be drawn.
 */
var Erizo = Erizo || {};
Erizo.Stream = function (spec) {
    var that = Erizo.EventDispatcher(spec),
        getFrame;

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
    that.extensionId = spec.extensionId;

    if (that.videoSize !== undefined &&
        (!(that.videoSize instanceof Array) ||
           that.videoSize.length !== 4)) {
        throw Error('Invalid Video Size');
    }
    if (spec.local === undefined || spec.local === true) {
        that.local = true;
    }

    // Public functions

    that.getID = function () {
        var id;
        // Unpublished local streams don't yet have an ID.
        if (that.local && !spec.streamID) {
            id = 'local';
        }
        else {
            id = spec.streamID;
        }
        return id;
    };

    // Get attributes of this stream.
    that.getAttributes = function () {
        return spec.attributes;
    };

    // Changes the attributes of this stream in the room.
    that.setAttributes = function() {
        L.Logger.error('Failed to set attributes data. This Stream object has not been published.');
    };

    that.updateLocalAttributes = function(attrs) {
        spec.attributes = attrs;
    };

    // Indicates if the stream has audio activated
    that.hasAudio = function () {
        return spec.audio;
    };

    // Indicates if the stream has video activated
    that.hasVideo = function () {
        return spec.video;
    };

    // Indicates if the stream has data activated
    that.hasData = function () {
        return spec.data;
    };

    // Indicates if the stream has screen activated
    that.hasScreen = function () {
        return spec.screen;
    };

    // Sends data through this stream.
    that.sendData = function () {
        L.Logger.error('Failed to send data. This Stream object has not that channel enabled.');
    };

    // Initializes the stream and tries to retrieve a stream from local video and audio
    // We need to call this method before we can publish it in the room.
    that.init = function () {
      var streamEvent;
      try {
        if ((spec.audio || spec.video || spec.screen) && spec.url === undefined) {
          L.Logger.info('Requested access to local media');
          var videoOpt = spec.video;
          if ((videoOpt === true || spec.screen === true) &&
              that.videoSize !== undefined) {
            videoOpt = {mandatory: {minWidth: that.videoSize[0],
                                    minHeight: that.videoSize[1],
                                    maxWidth: that.videoSize[2],
                                    maxHeight: that.videoSize[3]}};
          } else if (spec.screen === true && videoOpt === undefined) {
            videoOpt = true;
          }
          var opt = {video: videoOpt,
                     audio: spec.audio,
                     fake: spec.fake,
                     screen: spec.screen,
                     extensionId:that.extensionId};
          L.Logger.debug(opt);
          Erizo.GetUserMedia(opt, function (stream) {
            //navigator.webkitGetUserMedia("audio, video", function (stream) {

            L.Logger.info('User has granted access to local media.');
            that.stream = stream;

            streamEvent = Erizo.StreamEvent({type: 'access-accepted'});
            that.dispatchEvent(streamEvent);

          }, function (error) {
            L.Logger.error('Failed to get access to local media. Error code was ' +
                           error.code + '.');
            var streamEvent = Erizo.StreamEvent({type: 'access-denied', msg:error});
            that.dispatchEvent(streamEvent);
          });
          } else {
            streamEvent = Erizo.StreamEvent({type: 'access-accepted'});
            that.dispatchEvent(streamEvent);
          }
          } catch(e) {
            L.Logger.error('Failed to get access to local media. Error was ' + e + '.');
            streamEvent = Erizo.StreamEvent({type: 'access-denied', msg: e});
            that.dispatchEvent(streamEvent);
          }
      };


     that.close = function () {
        if (that.local) {
            if (that.room !== undefined) {
                that.room.unpublish(that);
            }
            // Remove HTML element
            that.hide();
            if (that.stream !== undefined) {
                that.stream.getTracks().forEach(function (track) {
                    track.stop();
                });
            }
            that.stream = undefined;
        }
    };

    that.play = function (elementID, options) {
        options = options || {};
        that.elementID = elementID;
        var player;
        if (that.hasVideo() || this.hasScreen()) {
            // Draw on HTML
            if (elementID !== undefined) {
                player = new Erizo.VideoPlayer({id: that.getID(),
                                                    stream: that,
                                                    elementID: elementID,
                                                    options: options});
                that.player = player;
                that.showing = true;
            }
        } else if (that.hasAudio) {
            player = new Erizo.AudioPlayer({id: that.getID(),
                                                stream: that,
                                                elementID: elementID,
                                                options: options});
            that.player = player;
            that.showing = true;
        }
    };

    that.stop = function () {
        if (that.showing) {
            if (that.player !== undefined) {
                that.player.destroy();
                that.showing = false;
            }
        }
    };

    that.show = that.play;
    that.hide = that.stop;

    getFrame = function () {
        if (that.player !== undefined && that.stream !== undefined) {
            var video = that.player.video,

                style = document.defaultView.getComputedStyle(video),
                width = parseInt(style.getPropertyValue('width'), 10),
                height = parseInt(style.getPropertyValue('height'), 10),
                left = parseInt(style.getPropertyValue('left'), 10),
                top = parseInt(style.getPropertyValue('top'), 10);

            var div;
            if (typeof that.elementID === 'object' &&
              typeof that.elementID.appendChild === 'function') {
                div = that.elementID;
            }
            else {
                div = document.getElementById(that.elementID);
            }

            var divStyle = document.defaultView.getComputedStyle(div),
                divWidth = parseInt(divStyle.getPropertyValue('width'), 10),
                divHeight = parseInt(divStyle.getPropertyValue('height'), 10),

                canvas = document.createElement('canvas'),
                context;

            canvas.id = 'testing';
            canvas.width = divWidth;
            canvas.height = divHeight;
            canvas.setAttribute('style', 'display: none');
            //document.body.appendChild(canvas);
            context = canvas.getContext('2d');

            context.drawImage(video, left, top, width, height);

            return canvas;
        } else {
            return null;
        }
    };

    that.getVideoFrameURL = function (format) {
        var canvas = getFrame();
        if (canvas !== null) {
            if (format) {
                return canvas.toDataURL(format);
            } else {
                return canvas.toDataURL();
            }
        } else {
            return null;
        }
    };

    that.getVideoFrame = function () {
        var canvas = getFrame();
        if (canvas !== null) {
            return canvas.getContext('2d').getImageData(0, 0, canvas.width, canvas.height);
        } else {
            return null;
        }
    };

    that.checkOptions = function (config, isUpdate){
        //TODO: Check for any incompatible options
        if (isUpdate === true){  // We are updating the stream
            if (config.video || config.audio || config.screen){
                L.Logger.warning('Cannot update type of subscription');
                config.video = undefined;
                config.audio = undefined;
                config.screen = undefined;
            }
        }else{  // on publish or subscribe
            if(that.local === false){ // check what we can subscribe to
                if (config.video === true && that.hasVideo() === false){
                    L.Logger.warning('Trying to subscribe to video when there is no ' +
                                     'video, won\'t subscribe to video');
                    config.video = false;
                }
                if (config.audio === true && that.hasAudio() === false){
                    L.Logger.warning('Trying to subscribe to audio when there is no ' +
                                     'audio, won\'t subscribe to audio');
                    config.audio = false;
                }
            }
        }
        if(that.local === false){
            if (!that.hasVideo() && (config.slideShowMode === true)){
                L.Logger.warning('Cannot enable slideShowMode if it is not a video ' +
                                 'subscription, please check your parameters');
                config.slideShowMode = false;
            }
        }
    };

    that.muteAudio = function (isMuted, callback) {
        if (that.room && that.room.p2p){
            L.Logger.warning('muteAudio is not implemented in p2p streams');
            callback ('error');
            return;
        }
        if (that.local) {
            L.Logger.warning('muteAudio can only be used in remote streams');
            callback('Error');
            return;
        }
        var config = {muteStream : {audio : isMuted}};
        that.checkOptions(config, true);
        that.pc.updateSpec(config, callback);
    };

    that.updateConfiguration = function (config, callback) {
        if (config === undefined)
            return;
        if (that.pc) {
            that.checkOptions(config, true);
            if (that.local) {
                if(that.room.p2p) {
                    for (var index in that.pc){
                        that.pc[index].updateSpec(config, callback);
                    }
                } else {
                    that.pc.updateSpec(config, callback);
                }

            } else {
                that.pc.updateSpec(config, callback);
            }
        } else {
            callback('This stream has no peerConnection attached, ignoring');
        }
    };

    return that;
};
