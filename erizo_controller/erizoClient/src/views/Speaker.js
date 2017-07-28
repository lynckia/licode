/* global document */

import View from './View';

/*
 * Speaker represents the volume icon that will be shown in the mediaPlayer, for example.
 * It manages the volume level of the media tag given in the constructor.
 * Every Speaker is a View.
 * Ex.: var speaker = Speaker({elementID: element, media: mediaTag, id: id});
 */
const Speaker = (spec) => {
  const that = View({});
  let lastVolume = 50;

  const mute = () => {
    that.media.muted = true;
    that.icon.setAttribute('src', `${that.url}/assets/mute48.png`);
    if (that.stream.local) {
      that.stream.stream.getAudioTracks()[0].enabled = false;
    } else {
      lastVolume = that.picker.value;
      that.picker.value = 0;
      that.media.volume = 0;
    }
  };

  const unmute = () => {
    that.media.muted = false;
    that.icon.setAttribute('src', `${that.url}/assets/sound48.png`);
    if (that.stream.local) {
      that.stream.stream.getAudioTracks()[0].enabled = true;
    } else {
      that.picker.value = lastVolume;
      that.media.volume = that.picker.value / 100;
    }
  };

  // Variables

  // DOM element in which the Speaker will be appended
  that.elementID = spec.elementID;

  // media tag
  that.media = spec.media;

  // Speaker id
  that.id = spec.id;

  // MediaStream
  that.stream = spec.stream;

  // Container
  that.div = document.createElement('div');
  that.div.setAttribute('style', 'width: 40%; height: 100%; max-width: 32px; ' +
                                   'position: absolute; right: 0;z-index:0;');

  // Volume icon
  that.icon = document.createElement('img');
  that.icon.setAttribute('id', `volume_${that.id}`);
  that.icon.setAttribute('src', `${that.url}/assets/sound48.png`);
  that.icon.setAttribute('style', 'width: 80%; height: 100%; position: absolute;');
  that.div.appendChild(that.icon);

  that.icon.onclick = () => {
    if (that.media.muted) {
      unmute();
    } else {
      mute();
    }
  };

  if (!that.stream.local) {
    // Volume bar
    that.picker = document.createElement('input');
    that.picker.setAttribute('id', `picker_${that.id}`);
    that.picker.type = 'range';
    that.picker.min = 0;
    that.picker.max = 100;
    that.picker.step = 10;
    that.picker.value = lastVolume;
    //  FireFox supports range sliders as of version 23
    that.picker.setAttribute('orient', 'vertical');
    that.div.appendChild(that.picker);
    that.media.volume = that.picker.value / 100;
    that.media.muted = false;

    that.picker.oninput = () => {
      if (that.picker.value > 0) {
        that.media.muted = false;
        that.icon.setAttribute('src', `${that.url}/assets/sound48.png`);
      } else {
        that.media.muted = true;
        that.icon.setAttribute('src', `${that.url}/assets/mute48.png`);
      }
      that.media.volume = that.picker.value / 100;
    };

    // Private functions
    const show = (displaying) => {
      that.picker.setAttribute('style', `background: transparent; width: 32px;
                                         height: 100px; position: absolute; bottom: 90%;
                                         z-index: 1; right: 0px; -webkit-appearance: slider-vertical;
                                         bottom: ${that.div.offsetHeight}px; display: ${displaying}`);
    };

    // Public functions
    that.div.onmouseover = () => {
      show('block');
    };

    that.div.onmouseout = () => {
      show('none');
    };

    show('none');
  }

  document.getElementById(that.elementID).appendChild(that.div);
  return that;
};

export default Speaker;
