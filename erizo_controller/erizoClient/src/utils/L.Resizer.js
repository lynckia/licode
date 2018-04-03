/* globals $$, jQuery, Elements, document, window, L */

/**
* Copyright 2013 Marc J. Schmidt. See the LICENSE file at the top-level
* directory of this distribution and at
* https://github.com/marcj/css-element-queries/blob/master/LICENSE.
*/
this.L = this.L || {};

/**
 * @param {HTMLElement} element
 * @param {String}      prop
 * @returns {String|Number}
 */
L.GetComputedStyle = (computedElement, prop) => {
  if (computedElement.currentStyle) {
    return computedElement.currentStyle[prop];
  } else if (window.getComputedStyle) {
    return window.getComputedStyle(computedElement, null).getPropertyValue(prop);
  }
  return computedElement.style[prop];
};

  /**
   *
   * @type {Function}
   * @constructor
   */
L.ElementQueries = function ElementQueries() {
      /**
       *
       * @param element
       * @returns {Number}
       */
  function getEmSize(element = document.documentElement) {
    const fontSize = L.GetComputedStyle(element, 'fontSize');
    return parseFloat(fontSize) || 16;
  }

      /**
       *
       * @copyright https://github.com/Mr0grog/element-query/blob/master/LICENSE
       *
       * @param element
       * @param value
       * @param units
       * @returns {*}
       */
  function convertToPx(element, originalValue) {
    let vh;
    let vw;
    let chooser;
    const units = originalValue.replace(/[0-9]*/, '');
    const value = parseFloat(originalValue);
    switch (units) {
      case 'px':
        return value;
      case 'em':
        return value * getEmSize(element);
      case 'rem':
        return value * getEmSize();
              // Viewport units!
              // According to http://quirksmode.org/mobile/tableViewport.html
              // documentElement.clientWidth/Height gets us the most reliable info
      case 'vw':
        return (value * document.documentElement.clientWidth) / 100;
      case 'vh':
        return (value * document.documentElement.clientHeight) / 100;
      case 'vmin':
      case 'vmax':
        vw = document.documentElement.clientWidth / 100;
        vh = document.documentElement.clientHeight / 100;
        chooser = Math[units === 'vmin' ? 'min' : 'max'];
        return value * chooser(vw, vh);
      default:
        return value;
              // for now, not supporting physical units (since they are just a set number of px)
              // or ex/ch (getting accurate measurements is hard)
    }
  }

      /**
       *
       * @param {HTMLElement} element
       * @constructor
       */
  function SetupInformation(element) {
    this.element = element;
    this.options = [];
    let i;
    let j;
    let option;
    let width = 0;
    let height = 0;
    let value;
    let actualValue;
    let attrValues;
    let attrValue;
    let attrName;

          /**
           * @param option {mode: 'min|max', property: 'width|height', value: '123px'}
           */
    this.addOption = (newOption) => {
      this.options.push(newOption);
    };

    const attributes = ['min-width', 'min-height', 'max-width', 'max-height'];

          /**
           * Extracts the computed width/height and sets to min/max- attribute.
           */
    this.call = () => {
              // extract current dimensions
      width = this.element.offsetWidth;
      height = this.element.offsetHeight;

      attrValues = {};

      for (i = 0, j = this.options.length; i < j; i += 1) {
        option = this.options[i];
        value = convertToPx(this.element, option.value);

        actualValue = option.property === 'width' ? width : height;
        attrName = `${option.mode}-${option.property}`;
        attrValue = '';

        if (option.mode === 'min' && actualValue >= value) {
          attrValue += option.value;
        }

        if (option.mode === 'max' && actualValue <= value) {
          attrValue += option.value;
        }

        if (!attrValues[attrName]) attrValues[attrName] = '';
        if (attrValue && (` ${attrValues[attrName]} `)
                                            .indexOf(` ${attrValue} `) === -1) {
          attrValues[attrName] += ` ${attrValue}`;
        }
      }

      for (let k = 0; k < attributes.length; k += 1) {
        if (attrValues[attributes[k]]) {
          this.element.setAttribute(attributes[k],
                                                attrValues[attributes[k]].substr(1));
        } else {
          this.element.removeAttribute(attributes[k]);
        }
      }
    };
  }

      /**
       * @param {HTMLElement} element
       * @param {Object}      options
       */
  function setupElement(originalElement, options) {
    const element = originalElement;
    if (element.elementQueriesSetupInformation) {
      element.elementQueriesSetupInformation.addOption(options);
    } else {
      element.elementQueriesSetupInformation = new SetupInformation(element);
      element.elementQueriesSetupInformation.addOption(options);
      element.sensor = new L.ResizeSensor(element, () => {
        element.elementQueriesSetupInformation.call();
      });
    }
    element.elementQueriesSetupInformation.call();
    return element;
  }

      /**
       * @param {String} selector
       * @param {String} mode min|max
       * @param {String} property width|height
       * @param {String} value
       */
  function queueQuery(selector, mode, property, value) {
    let query;
    if (document.querySelectorAll) query = document.querySelectorAll.bind(document);
    if (!query && typeof $$ !== 'undefined') query = $$;
    if (!query && typeof jQuery !== 'undefined') query = jQuery;

    if (!query) {
      throw new Error('No document.querySelectorAll, jQuery or Mootools\'s $$ found.');
    }

    const elements = query(selector) || [];
    for (let i = 0, j = elements.length; i < j; i += 1) {
      elements[i] = setupElement(elements[i], {
        mode,
        property,
        value,
      });
    }
  }

  const regex = /,?([^,\n]*)\[[\s\t]*(min|max)-(width|height)[\s\t]*[~$^]?=[\s\t]*"([^"]*)"[\s\t]*]([^\n\s{]*)/mgi;  // jshint ignore:line

      /**
       * @param {String} css
       */
  function extractQuery(originalCss) {
    let match;
    const css = originalCss.replace(/'/g, '"');
    while ((match = regex.exec(css)) !== null) {
      if (match.length > 5) {
        queueQuery(match[1] || match[5], match[2], match[3], match[4]);
      }
    }
  }

      /**
       * @param {CssRule[]|String} rules
       */
  function readRules(originalRules) {
    if (!originalRules) {
      return;
    }
    let selector = '';
    let rules = originalRules;
    if (typeof originalRules === 'string') {
      rules = originalRules.toLowerCase();
      if (rules.indexOf('min-width') !== -1 || rules.indexOf('max-width') !== -1) {
        extractQuery(rules);
      }
    } else {
      for (let i = 0, j = rules.length; i < j; i += 1) {
        if (rules[i].type === 1) {
          selector = rules[i].selectorText || rules[i].cssText;
          if (selector.indexOf('min-height') !== -1 ||
                          selector.indexOf('max-height') !== -1) {
            extractQuery(selector);
          } else if (selector.indexOf('min-width') !== -1 ||
                                 selector.indexOf('max-width') !== -1) {
            extractQuery(selector);
          }
        } else if (rules[i].type === 4) {
          readRules(rules[i].cssRules || rules[i].rules);
        }
      }
    }
  }

      /**
       * Searches all css rules and setups the event listener
       * to all elements with element query rules..
       */
  this.init = () => {
    const styleSheets = document.styleSheets || [];
    for (let i = 0, j = styleSheets.length; i < j; i += 1) {
      if (Object.prototype.hasOwnProperty.call(styleSheets[i], 'cssText')) {
        readRules(styleSheets[i].cssText);
      } else if (Object.prototype.hasOwnProperty.call(styleSheets[i], 'cssRules')) {
        readRules(styleSheets[i].cssRules);
      } else if (Object.prototype.hasOwnProperty.call(styleSheets[i], 'rules')) {
        readRules(styleSheets[i].rules);
      }
    }
  };
};

function init() {
  (new L.ElementQueries()).init();
}

if (window.addEventListener) {
  window.addEventListener('load', init, false);
} else {
  window.attachEvent('onload', init);
}

  /**
   * Iterate over each of the provided element(s).
   *
   * @param {HTMLElement|HTMLElement[]} elements
   * @param {Function}                  callback
   */
function forEachElement(elements, callback = () => {}) {
  const elementsType = Object.prototype.toString.call(elements);
  const isCollectionTyped = (elementsType === '[object Array]' ||
          (elementsType === '[object NodeList]') ||
          (elementsType === '[object HTMLCollection]') ||
          (typeof jQuery !== 'undefined' && elements instanceof jQuery) || // jquery
          (typeof Elements !== 'undefined' && elements instanceof Elements) // mootools
      );
  let i = 0;
  const j = elements.length;
  if (isCollectionTyped) {
    for (; i < j; i += 1) {
      callback(elements[i]);
    }
  } else {
    callback(elements);
  }
}
  /**
   * Class for dimension change detection.
   *
   * @param {Element|Element[]|Elements|jQuery} element
   * @param {Function} callback
   *
   * @constructor
   */
L.ResizeSensor = function ResizeSensor(element, callback = () => {}) {
      /**
       *
       * @constructor
       */
  function EventQueue() {
    let q = [];
    this.add = (ev) => {
      q.push(ev);
    };

    let i;
    let j;
    this.call = () => {
      for (i = 0, j = q.length; i < j; i += 1) {
        q[i].call();
      }
    };

    this.remove = (ev) => {
      const newQueue = [];
      for (i = 0, j = q.length; i < j; i += 1) {
        if (q[i] !== ev) newQueue.push(q[i]);
      }
      q = newQueue;
    };

    this.length = () => q.length;
  }

      /**
       *
       * @param {HTMLElement} element
       * @param {Function}    resized
       */
  function attachResizeEvent(htmlElement, resized) {
    // Only used for the dirty checking, so the event callback count is limted
    //  to max 1 call per fps per sensor.
    // In combination with the event based resize sensor this saves cpu time,
    // because the sensor is too fast and
    // would generate too many unnecessary events.
    const customRequestAnimationFrame = window.requestAnimationFrame ||
    window.mozRequestAnimationFrame ||
    window.webkitRequestAnimationFrame ||
    function delay(fn) {
      return window.setTimeout(fn, 20);
    };

    const newElement = htmlElement;
    if (!newElement.resizedAttached) {
      newElement.resizedAttached = new EventQueue();
      newElement.resizedAttached.add(resized);
    } else if (newElement.resizedAttached) {
      newElement.resizedAttached.add(resized);
      return;
    }

    newElement.resizeSensor = document.createElement('div');
    newElement.resizeSensor.className = 'resize-sensor';
    const style = 'position: absolute; left: 0; top: 0; right: 0; bottom: 0; ' +
                      'overflow: hidden; z-index: -1; visibility: hidden;';
    const styleChild = 'position: absolute; left: 0; top: 0; transition: 0s;';

    newElement.resizeSensor.style.cssText = style;
    newElement.resizeSensor.innerHTML =
              `<div class="resize-sensor-expand" style="${style}">` +
                  `<div style="${styleChild}"></div>` +
              '</div>' +
              `<div class="resize-sensor-shrink" style="${style}">` +
                  `<div style="${styleChild} width: 200%; height: 200%"></div>` +
              '</div>';
    newElement.appendChild(newElement.resizeSensor);

    if (L.GetComputedStyle(newElement, 'position') === 'static') {
      newElement.style.position = 'relative';
    }

    const expand = newElement.resizeSensor.childNodes[0];
    const expandChild = expand.childNodes[0];
    const shrink = newElement.resizeSensor.childNodes[1];

    const reset = () => {
      expandChild.style.width = `${100000}px`;
      expandChild.style.height = `${100000}px`;

      expand.scrollLeft = 100000;
      expand.scrollTop = 100000;

      shrink.scrollLeft = 100000;
      shrink.scrollTop = 100000;
    };

    reset();
    let dirty = false;

    const dirtyChecking = () => {
      if (!newElement.resizedAttached) return;

      if (dirty) {
        newElement.resizedAttached.call();
        dirty = false;
      }

      customRequestAnimationFrame(dirtyChecking);
    };

    customRequestAnimationFrame(dirtyChecking);
    let lastWidth;
    let lastHeight;
    let cachedWidth;
    let cachedHeight; // useful to not query offsetWidth twice

    const onScroll = () => {
      if ((cachedWidth = newElement.offsetWidth) !== lastWidth ||
                (cachedHeight = newElement.offsetHeight) !== lastHeight) {
        dirty = true;

        lastWidth = cachedWidth;
        lastHeight = cachedHeight;
      }
      reset();
    };

    const addEvent = (el, name, cb) => {
      if (el.attachEvent) {
        el.attachEvent(`on${name}`, cb);
      } else {
        el.addEventListener(name, cb);
      }
    };

    addEvent(expand, 'scroll', onScroll);
    addEvent(shrink, 'scroll', onScroll);
  }

  forEachElement(element, (elem) => {
    attachResizeEvent(elem, callback);
  });

  this.detach = (ev) => {
    L.ResizeSensor.detach(element, ev);
  };
};

L.ResizeSensor.detach = (element, ev) => {
  forEachElement(element, (elem) => {
    const elementItem = elem;
    if (elementItem.resizedAttached && typeof ev === 'function') {
      elementItem.resizedAttached.remove(ev);
      if (elementItem.resizedAttached.length()) return;
    }
    if (elementItem.resizeSensor) {
      if (elementItem.contains(elementItem.resizeSensor)) {
        elementItem.removeChild(elementItem.resizeSensor);
      }
      delete elementItem.resizeSensor;
      delete elementItem.resizedAttached;
    }
  });
};
