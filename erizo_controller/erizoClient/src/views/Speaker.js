/*global window, console, document */
/*
 * Speaker represents the volume icon that will be shown in the VideoPlayer, for example.
 * It manages the volume level of the video tag given in the constructor.
 * Every Speaker is a View.
 * Ex.: var speaker = Speaker({elementID: element, video: videoTag, id: id});
 */
var Erizo = Erizo || {};
Erizo.Speaker = function (spec) {
    "use strict";
    var that = Erizo.View({}),
        show,
        mute,
        unmute,
        lastVolume = 50,
        muted = false;

    // Variables

    // DOM element in which the Speaker will be appended
    that.elementID = spec.elementID;

    // Video tag
    that.video = spec.video;

    // Speaker id
    that.id = spec.id;

    // MediaStream
    that.stream = spec.stream;

    // Container
    that.div = document.createElement('div');
    that.div.setAttribute('style', 'width: 40%; height: 130%; max-width: 32px; position: absolute; bottom: 0; right: 0;');

    // Volume icon 
    that.icon = document.createElement('img');
    that.icon.setAttribute('id', 'volume_' + that.id);
    that.icon.setAttribute('src', that.url + '/assets/sound48.png');
    that.icon.setAttribute('style', 'width: 80%; height: 70%; bottom: 0px; position: absolute;');
    that.div.appendChild(that.icon);


    if (!that.stream.local) {

        // Volume bar
        that.picker = document.createElement('input');
        that.picker.setAttribute('id', 'picker_' + that.id);
        that.picker.type = "range";
        that.picker.min = 0;
        that.picker.max = 100;
        that.picker.step = 10;
        that.picker.value = lastVolume;
        that.div.appendChild(that.picker);
        that.video.volume = that.picker.value / 100;

        that.picker.oninput = function (evt) {
            if (that.picker.value > 0) {
                muted = false;
                that.icon.setAttribute('src', that.url + '/assets/sound48.png');
            } else {
                muted = true;
                that.icon.setAttribute('src', that.url + '/assets/mute48.png');
            }
            that.video.volume = that.picker.value / 100;
        };

        // Private functions
        show = function (displaying) {
            that.picker.setAttribute('style', 'width: 32px; height: 100px; position: absolute; bottom: ' + that.div.offsetHeight + 'px; right: 0px; -webkit-appearance: slider-vertical; display: ' + displaying);
        };

        mute = function () {
            muted = true;
            that.icon.setAttribute('src', that.url + '/assets/mute48.png');
            lastVolume = that.picker.value;
            that.picker.value = 0;
            that.video.volume = 0;
        };

        unmute = function () {
            muted = false;
            that.icon.setAttribute('src', that.url + '/assets/sound48.png');
            that.picker.value = lastVolume;
            that.video.volume = that.picker.value / 100;
        };

        that.icon.onclick = function (evt) {
            if (muted) {
                unmute();
            } else {
                mute();
            }
        }

        // Public functions
        that.div.onmouseover = function (evt) {
            show('block');
        };

        that.div.onmouseout = function (evt) {
            show('none');
        };

        show('none');

    } else {

        mute = function () {
            muted = true;
            that.icon.setAttribute('src', that.url + '/assets/mute48.png');
            that.stream.stream.getAudioTracks()[0].enabled = false;
        };

        unmute = function () {
            muted = false;
            that.icon.setAttribute('src', that.url + '/assets/sound48.png');
            that.stream.stream.getAudioTracks()[0].enabled = true;
        };

        that.icon.onclick = function (evt) {
            if (muted) {
                unmute();
            } else {
                mute();
            }
        }
    }
  

    document.getElementById(that.elementID).appendChild(that.div);
    return that;
};