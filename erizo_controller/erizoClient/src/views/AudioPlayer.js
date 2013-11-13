/*global window, console, clearInterval, setInterval, document, unescape, L, webkitURL*/
/*
 * AudioPlayer represents a Licode Audio component that shows either a local or a remote Audio.
 * Ex.: var player = AudioPlayer({id: id, stream: stream, elementID: elementID});
 * A AudioPlayer is also a View component.
 */
var Erizo = Erizo || {};
Erizo.AudioPlayer = function (spec) {
    "use strict";

    var that = Erizo.View({}),
        onmouseover,
        onmouseout;

    // Variables

    // AudioPlayer ID
    that.id = spec.id;

    // Stream that the AudioPlayer will play
    that.stream = spec.stream.stream;

    // DOM element in which the AudioPlayer will be appended
    that.elementID = spec.elementID;


    L.Logger.debug('Creating URL from stream ' + that.stream);
    var myURL = window.URL || webkitURL;
    that.stream_url = myURL.createObjectURL(that.stream);

    // Audio tag
    that.audio = document.createElement('audio');
    that.audio.setAttribute('id', 'stream' + that.id);
    that.audio.setAttribute('style', 'width: 100%; height: 100%; position: absolute');
    that.audio.setAttribute('autoplay', 'autoplay');

    if(spec.stream.local) 
        that.audio.volume = 0;

    if(spec.stream.local) 
        that.audio.volume = 0;


    if (that.elementID !== undefined) {

        // It will stop the AudioPlayer and remove it from the HTML
        that.destroy = function () {
            that.audio.pause();
            //clearInterval(that.resize);
            that.parentNode.removeChild(that.div);
        };

        onmouseover = function (evt) {
            that.bar.display();
        };

        onmouseout = function (evt) {
            that.bar.hide();
        };

        // Container
        that.div = document.createElement('div');
        that.div.setAttribute('id', 'player_' + that.id);
        that.div.setAttribute('style', 'width: 100%; height: 100%; position: relative; overflow: hidden;');

        document.getElementById(that.elementID).appendChild(that.div);
        that.container = document.getElementById(that.elementID);

        that.parentNode = that.div.parentNode;

        that.div.appendChild(that.audio);

        // Bottom Bar
        that.bar = new Erizo.Bar({elementID: 'player_' + that.id, id: that.id, stream: spec.stream, media: that.audio, options: spec.options});

        that.div.onmouseover = onmouseover;
        that.div.onmouseout = onmouseout;

    } else {
        // It will stop the AudioPlayer and remove it from the HTML
        that.destroy = function () {
            that.audio.pause();
            //clearInterval(that.resize);
            that.parentNode.removeChild(that.audio);
        };

        document.body.appendChild(that.audio);
        that.parentNode = document.body;
    }

    that.audio.src = that.stream_url;

    return that;
};