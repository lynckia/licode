/* global document */

import View from './View';
import Bar from './Bar';

/*
 * VideoPlayer represents a Licode video component that shows either a local or a remote video.
 * Ex.: var player = VideoPlayer({id: id, stream: stream, elementID: elementID});
 * A VideoPlayer is also a View component.
 */
const VideoPlayer = (spec) => {
  const that = View({});

  // Variables

  // VideoPlayer ID
  that.id = spec.id;

  // Stream that the VideoPlayer will play
  that.stream = spec.stream.stream;

  // DOM element in which the VideoPlayer will be appended
  that.elementID = spec.elementID;

  // Private functions
  const onmouseover = () => {
    that.bar.display();
  };

  const onmouseout = () => {
    that.bar.hide();
  };

  // Public functions

  // It will stop the VideoPlayer and remove it from the HTML
  that.destroy = () => {
    that.video.pause();
    that.parentNode.removeChild(that.div);
  };

  /* window.location.href.replace(/[?&]+([^=&]+)=([^&]*)/gi, function (m, key, value) {
      document.getElementById(key).value = unescape(value);
  }); */

  // Container
  that.div = document.createElement('div');
  that.div.setAttribute('id', `player_${that.id}`);
  that.div.setAttribute('class', 'licode_player');
  that.div.setAttribute('style', 'width: 100%; height: 100%; position: relative; ' +
                                   'background-color: black; overflow: hidden;');

  // Loader icon
  if (spec.options.loader !== false) {
    that.loader = document.createElement('img');
    that.loader.setAttribute('style', 'width: 16px; height: 16px; position: absolute; ' +
                                        'top: 50%; left: 50%; margin-top: -8px; margin-left: -8px');
    that.loader.setAttribute('id', `back_${that.id}`);
    that.loader.setAttribute('class', 'licode_loader');
    that.loader.setAttribute('src', `${that.url}/assets/loader.gif`);
  }

  // Video tag
  that.video = document.createElement('video');
  that.video.setAttribute('id', `stream${that.id}`);
  that.video.setAttribute('class', 'licode_stream');
  that.video.setAttribute('style', 'width: 100%; height: 100%; position: absolute; object-fit: cover');
  that.video.setAttribute('autoplay', 'autoplay');
  that.video.setAttribute('playsinline', 'playsinline');

  if (spec.stream.local) { that.video.volume = 0; }

  if (that.elementID !== undefined) {
    // Check for a passed DOM node.
    if (typeof that.elementID === 'object' &&
          typeof that.elementID.appendChild === 'function') {
      that.container = that.elementID;
    } else {
      that.container = document.getElementById(that.elementID);
    }
  } else {
    that.container = document.body;
  }
  that.container.appendChild(that.div);

  that.parentNode = that.div.parentNode;

  if (that.loader) {
    that.div.appendChild(that.loader);
  }
  that.div.appendChild(that.video);

  that.containerWidth = 0;
  that.containerHeight = 0;

  // Bottom Bar
  if (spec.options.bar !== false) {
    that.bar = Bar({ elementID: `player_${that.id}`,
      id: that.id,
      stream: spec.stream,
      media: that.video,
      options: spec.options });

    that.div.onmouseover = onmouseover;
    that.div.onmouseout = onmouseout;
  } else {
    // Expose a consistent object to manipulate the media.
    that.media = that.video;
  }

  that.video.srcObject = that.stream;

  return that;
};

export default VideoPlayer;
