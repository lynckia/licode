/*
 * Class Stream represents a local or a remote Stream in the Room. It will handle the WebRTC stream
 * and identify the stream and where it should be drawn.
 */
var Stream = function (spec) {
    "use strict";
    var that = {};
    that.elementID = undefined;
    that.streamID = spec.streamID;
    that.stream = spec.stream;
    return that;
};