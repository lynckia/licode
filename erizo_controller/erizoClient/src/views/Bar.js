/*global window, document, clearTimeout, setTimeout */

/*
 * Bar represents the bottom menu bar of every VideoPlayer.
 * It contains a Speaker and an icon.
 * Every Bar is a View.
 * Ex.: var bar = Bar({elementID: element, id: id});
 */
var Erizo = Erizo || {};
Erizo.Bar = function (spec) {
    "use strict";
    var that = Erizo.View({}),
        waiting,
        show;

    // Variables

    // DOM element in which the Bar will be appended
    that.elementID = spec.elementID;

    // Bar ID
    that.id = spec.id;

    // Container
    that.div = document.createElement('div');
    that.div.setAttribute('id', 'bar_' + that.id);

    // Bottom bar
    that.bar = document.createElement('div');
    that.bar.setAttribute('style', 'width: 100%; height: 15%; max-height: 30px; position: absolute; bottom: 0; right: 0; background-color: rgba(255,255,255,0.62)');
    that.bar.setAttribute('id', 'subbar_' + that.id);

    // Lynckia icon
    that.link = document.createElement('a');
    that.link.setAttribute('href', 'http://www.lynckia.com/');
    that.link.setAttribute('target', '_blank');

    that.logo = document.createElement('img');
    that.logo.setAttribute('style', 'width: 10%; height: 100%; max-width: 30px; position: absolute; top: 0; left: 2px;');
    that.logo.setAttribute('alt', 'Lynckia');
    that.logo.setAttribute('src', that.url + '/assets/star.svg');

    // Private functions
    show = function (displaying) {
        if (displaying !== 'block') {
            displaying = 'none';
        } else {
            clearTimeout(waiting);
        }

        that.div.setAttribute('style', 'width: 100%; height: 100%; position: relative; bottom: 0; right: 0; display:' + displaying);
    };

    // Public functions

    that.display = function () {
        show('block');
    };

    that.hide = function () {
        waiting = setTimeout(show, 1000);
    };

    document.getElementById(that.elementID).appendChild(that.div);
    that.div.appendChild(that.bar);
    that.bar.appendChild(that.link);
    that.link.appendChild(that.logo);

    // Speaker component
    if (spec.options === undefined || spec.options.speaker === undefined || spec.options.speaker === true) {
        that.speaker = new Erizo.Speaker({elementID: 'subbar_' + that.id, id: that.id, video: spec.video});
    }

    that.display();
    that.hide();

    return that;
};