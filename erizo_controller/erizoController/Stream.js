/*global exports*/
exports.Stream = function (spec) {
    "use strict";

    var that = {},
        dataSubscribers = [];

    that.getID = function () {
        return spec.id;
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

    that.getAttributes = function () {
        return spec.attributes;
    };

    that.getDataSubscribers = function () {
        return dataSubscribers;
    };

    that.addDataSubscriber = function (id) {
        if (dataSubscribers.indexOf(id) === -1) {
            dataSubscribers.push(id);
        }
    };

    that.removeDataSubscriber = function (id) {
        var index = dataSubscribers.indexOf(id);
        if (index !== -1) {
            dataSubscribers.splice(index, 1);
        }
    };

    that.getPublicStream = function () {
        return {id: spec.id, audio: spec.audio, video: spec.video, data: spec.data, screen: spec.screen, attributes: spec.attributes};
    };

    return that;
};