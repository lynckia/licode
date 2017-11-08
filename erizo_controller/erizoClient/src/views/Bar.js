/* global document, clearTimeout, setTimeout */

import View from './View';
import Speaker from './Speaker';

/*
 * Bar represents the bottom menu bar of every mediaPlayer.
 * It contains a Speaker and an icon.
 * Every Bar is a View.
 * Ex.: var bar = Bar({elementID: element, id: id});
 */
const Bar = (spec) => {
  const that = View({});
  let waiting;

  // Variables

  // DOM element in which the Bar will be appended
  that.elementID = spec.elementID;

  // Bar ID
  that.id = spec.id;

  // Container
  that.div = document.createElement('div');
  that.div.setAttribute('id', `bar_${that.id}`);
  that.div.setAttribute('class', 'licode_bar');

  // Bottom bar
  that.bar = document.createElement('div');
  that.bar.setAttribute('style', 'width: 100%; height: 15%; max-height: 30px; ' +
                                 'position: absolute; bottom: 0; right: 0; ' +
                                 'background-color: rgba(255,255,255,0.62)');
  that.bar.setAttribute('id', `subbar_${that.id}`);
  that.bar.setAttribute('class', 'licode_subbar');

  // Lynckia icon
  that.link = document.createElement('a');
  that.link.setAttribute('href', 'http://www.lynckia.com/');
  that.link.setAttribute('class', 'licode_link');
  that.link.setAttribute('target', '_blank');

  that.logo = document.createElement('img');
  that.logo.setAttribute('style', 'width: 100%; height: 100%; max-width: 30px; ' +
                                  'position: absolute; top: 0; left: 2px;');
  that.logo.setAttribute('class', 'licode_logo');
  that.logo.setAttribute('alt', 'Lynckia');
  that.logo.setAttribute('src', `${that.url}/assets/star.svg`);

  // Private functions
  const show = (displaying) => {
    let action = displaying;
    if (displaying !== 'block') {
      action = 'none';
    } else {
      clearTimeout(waiting);
    }

    that.div.setAttribute('style',
      `width: 100%; height: 100%; position: relative; bottom: 0; right: 0; display: ${action}`);
  };

  // Public functions
  that.display = () => {
    show('block');
  };

  that.hide = () => {
    waiting = setTimeout(show, 1000);
  };

  document.getElementById(that.elementID).appendChild(that.div);
  that.div.appendChild(that.bar);
  that.bar.appendChild(that.link);
  that.link.appendChild(that.logo);

  // Speaker component
  if (!spec.stream.screen && (spec.options === undefined ||
                              spec.options.speaker === undefined ||
                              spec.options.speaker === true)) {
    that.speaker = Speaker({ elementID: `subbar_${that.id}`,
      id: that.id,
      stream: spec.stream,
      media: spec.media });
  }

  that.display();
  that.hide();

  return that;
};

export default Bar;
