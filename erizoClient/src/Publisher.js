/*
 * Class Publisher is used to configure local video and audio that can be 
 * published in the Room. We need to initialize the Publisher before we can
 * use it in the Room.
 * Typical initializacion of Publisher is:
 * var publisher = Publisher({audio:true, video:false, elementID:'divId'});
 * Audio and video variables has to be set to true if we want to publish them
 * in the room.
 * elementID variable indicates the DOM element's ID.
 * It dispatches events like 'access-accepted' when the user has allowed the 
 * use of his camera and/or microphone:
 * publisher.addEventListener('access-accepted', onAccepted); 
 */
var Publisher = function (spec) {
    "use strict";
    var that = EventDispatcher(spec);
    that.room = undefined;
    that.elementID = undefined;

    // Public functions

    // Indicates if the publisher has audio activated
    that.hasAudio = function () {
        return spec.audio;
    };

    // Indicates if the publisher has video activated
    that.hasVideo = function () {
        return spec.video;
    };

    // Initializes the publisher and tries to retrieve a stream from local video and audio
    // We need to call this method before we can publish it in the room.
    that.init = function () {
        try {
            navigator.webkitGetUserMedia({video: spec.video, audio: spec.audio}, function (stream) {
                
                L.Logger.info("User has granted access to local media.");

                that.stream = Stream({streamID: 0, stream: stream});

                // Draw on HTML
                if (spec.elementID !== undefined) {
                    var player = new VideoPlayer({id: that.stream.streamID, stream: that.stream.stream, elementID: spec.elementID});
                    that.player = player;
                }

                var publisherEvent = PublisherEvent({type: "access-accepted"});
                that.dispatchEvent(publisherEvent);
            }, function (error) {
                L.Logger.error("Failed to get access to local media. Error code was " + error.code + ".");
            });
            L.Logger.debug("Requested access to local media");
        } catch (e) {
            L.Logger.error("Error accessing to local media");
        }
    };

    return that;
};