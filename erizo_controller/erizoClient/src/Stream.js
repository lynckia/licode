/*global ErizoGetUserMedia, L, document*/
/*
 * Class Stream represents a local or a remote Stream in the Room. It will handle the WebRTC stream
 * and identify the stream and where it should be drawn.
 */
var Erizo = Erizo || {};
Erizo.Stream = function (spec) {
    "use strict";
    var that = Erizo.EventDispatcher(spec),
        getFrame;
    that.stream = spec.stream;
    that.room = undefined;
    that.showing = false;
    that.local = false;
    if (spec.local === undefined || spec.local === true) {
        that.local = true;
    }

    // Public functions

    that.getID = function () {
        return spec.streamID;
    };

    that.getAttributes = function () {
        return spec.attributes;
    };

    // Indicates if the stream has audio activated
    that.hasAudio = function () {
        return spec.audio;
    };

    // Indicates if the stream has video activated
    that.hasVideo = function () {
        return spec.video;
    };

    // Indicates if the stream has video activated
    that.hasData = function () {
        return spec.data;
    };

    // Sends data through this stream.
    that.sendData = function (msg) {};

    // Initializes the stream and tries to retrieve a stream from local video and audio
    // We need to call this method before we can publish it in the room.
    that.init = function () {
        try {
            if (spec.audio || spec.video) {
                L.Logger.debug("Requested access to local media");
                Erizo.GetUserMedia({video: spec.video, audio: spec.audio}, function (stream) {
                //navigator.webkitGetUserMedia("audio, video", function (stream) {

                    L.Logger.info("User has granted access to local media.");
                    that.stream = stream;

                    var streamEvent = Erizo.StreamEvent({type: "access-accepted"});
                    that.dispatchEvent(streamEvent);

                }, function (error) {
                    L.Logger.error("Failed to get access to local media. Error code was " + error.code + ".");
                    var streamEvent = Erizo.StreamEvent({type: "access-denied"});
                    that.dispatchEvent(streamEvent);
                });
            } else {
                var streamEvent = Erizo.StreamEvent({type: "access-accepted"});
                that.dispatchEvent(streamEvent);
            }
        } catch (e) {
            L.Logger.error("Error accessing to local media");
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
                that.stream.stop();       
            }
            that.stream = undefined; 
        }
    };

    that.show = function (elementID, options) {
        that.elementID = elementID;
        if (that.hasVideo()) {
            // Draw on HTML
            if (elementID !== undefined) {
                var player = new Erizo.VideoPlayer({id: that.getID(), stream: that, elementID: elementID, options: options});
                that.player = player;
                that.showing = true;
            }
        }
    };

    that.hide = function () {
        if (that.showing) {
            if (that.player !== undefined) {
                that.player.destroy();
                that.showing = false;
            }
        }
    };

    getFrame = function () {
        if (that.player !== undefined && that.stream !== undefined) {
            var video = that.player.video,

                style = document.defaultView.getComputedStyle(video),
                width = parseInt(style.getPropertyValue("width"), 10),
                height = parseInt(style.getPropertyValue("height"), 10),
                left = parseInt(style.getPropertyValue("left"), 10),
                top = parseInt(style.getPropertyValue("top"), 10),

                div = document.getElementById(that.elementID),
                divStyle = document.defaultView.getComputedStyle(div),
                divWidth = parseInt(divStyle.getPropertyValue("width"), 10),
                divHeight = parseInt(divStyle.getPropertyValue("height"), 10),

                canvas = document.createElement('canvas'),
                context;

            canvas.id = "testing";
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

    that.getVideoFrameURL = function () {
        var canvas = getFrame();
        if (canvas !== null) {
            return canvas.toDataURL();
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

    return that;
};