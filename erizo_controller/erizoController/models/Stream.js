/*global exports*/
'use strict';
exports.Stream = function (spec) {
    var that = {},
        dataSubscribers = [];

    that.getID = function () {
        return spec.id;
    };

    that.getClient = function () {
        return spec.client;
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

    // Retrieves attributes from stream
    that.getAttributes = function () {
        return spec.attributes;
    };

    // Set attributes for this stream
    that.setAttributes = function (attrs) {
        spec.attributes = attrs;
    };

    // Retrieves subscribers to data
    that.getDataSubscribers = function () {
        return dataSubscribers;
    };

    // Add a new data subscriber to this stream
    that.addDataSubscriber = function (id) {
        if (dataSubscribers.indexOf(id) === -1) {
            dataSubscribers.push(id);
        }
    };

    // Removes a data subscriber from this stream
    that.removeDataSubscriber = function (id) {
        var index = dataSubscribers.indexOf(id);
        if (index !== -1) {
            dataSubscribers.splice(index, 1);
        }
    };

    // Returns the public specification of this stream
    that.getPublicStream = function () {
        return {id: spec.id,
                audio: spec.audio,
                video: spec.video,
                data: spec.data,
                screen: spec.screen,
                attributes: spec.attributes};
    };

    return that;
};
