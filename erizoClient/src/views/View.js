/*
 * View class represents a HTML component
 * Every view is an EventDispatcher.
 */

var View = function(spec) {
    var that = EventDispatcher({});

    // Variables

    // URL where it will look for icons and assets
    that.url = "http://chotis2.dit.upm.es/";
    return that;
};