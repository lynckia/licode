/* global document */

import View from './View';
import Bar from './Bar';

/*
 * AudioPlayer represents a Licode Audio component that shows either a local or a remote Audio.
 * Ex.: var player = AudioPlayer({id: id, stream: stream, elementID: elementID});
 * A AudioPlayer is also a View component.
 */

const AudioPlayer = (spec) => {
  const that = View({});
  let onmouseover;
  let onmouseout;

  // Variables

  // AudioPlayer ID
  that.id = spec.id;

  // Stream that the AudioPlayer will play
  that.stream = spec.stream.stream;

  // DOM element in which the AudioPlayer will be appended
  that.elementID = spec.elementID;


  // Audio tag
  that.audio = document.createElement('audio');
  that.audio.setAttribute('id', `stream${that.id}`);
  that.audio.setAttribute('class', 'licode_stream');
  that.audio.setAttribute('style', 'width: 100%; height: 100%; position: absolute');
  that.audio.setAttribute('autoplay', 'autoplay');

  if (spec.stream.local) { that.audio.volume = 0; }

  if (that.elementID !== undefined) {
    // It will stop the AudioPlayer and remove it from the HTML
    that.destroy = () => {
      that.audio.pause();
      that.parentNode.removeChild(that.div);
    };

    onmouseover = () => {
      that.bar.display();
    };

    onmouseout = () => {
      that.bar.hide();
    };

    // Container
    that.div = document.createElement('div');
    that.div.setAttribute('id', `player_${that.id}`);
    that.div.setAttribute('class', 'licode_player');
    that.div.setAttribute('style', 'width: 100%; height: 100%; position: relative; ' +
                              'overflow: hidden;');

    // Check for a passed DOM node.
    if (typeof that.elementID === 'object' &&
          typeof that.elementID.appendChild === 'function') {
      that.container = that.elementID;
    } else {
      that.container = document.getElementById(that.elementID);
    }
    that.container.appendChild(that.div);

    that.parentNode = that.div.parentNode;

    that.div.appendChild(that.audio);

    // Bottom Bar
    if (spec.options.bar !== false) {
      that.bar = Bar({ elementID: `player_${that.id}`,
        id: that.id,
        stream: spec.stream,
        media: that.audio,
        options: spec.options });

      that.div.onmouseover = onmouseover;
      that.div.onmouseout = onmouseout;
    } else {
      // Expose a consistent object to manipulate the media.
      that.media = that.audio;
    }
  } else {
    // It will stop the AudioPlayer and remove it from the HTML
    that.destroy = () => {
      that.audio.pause();
      that.parentNode.removeChild(that.audio);
    };

    document.body.appendChild(that.audio);
    that.parentNode = document.body;
  }

  that.audio.srcObject = that.stream;

  return that;
};

export default AudioPlayer;
