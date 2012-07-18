/*
 * VideoPlayer represents a Lynckia video component that shows either a local or a remote video.
 * Ex.: var player = VideoPlayer({id: id, stream: stream, elementID: elementID});
 * A VideoPlayer is also a View component.
 */
var VideoPlayer = function(spec) {
    var that = View({});

    // Variables

    // VideoPlayer ID
    that.id = spec.id;

    // Stream that the VideoPlayer will play
    that.stream = spec.stream;

    // DOM element in which the VideoPlayer will be appended
    that.elementID = spec.elementID;

    // Private functions
    var onmouseover = function(evt) {
        that.bar.display();
    };

    var onmouseout = function(evt) {
        that.bar.hide();
    };

    // Public functions

    // It will stop the VideoPlayer and remove it from the HTML
    that.destroy = function() {
        that.video.pause();
        that.parentNode.removeChild(that.div);
    };
  
    window.location.href.replace(/[?&]+([^=&]+)=([^&]*)/gi, function (m, key, value) {
        document.getElementById(key).value = unescape(value);
    });

    L.Logger.debug('Creating URL from stream ' + that.stream);
    that.stream_url = webkitURL.createObjectURL(that.stream);

    // Container
    that.div = document.createElement('div');
    that.div.setAttribute('id', 'player_' + that.id);
    that.div.setAttribute('style', 'width: 100%; height: 100%; position: relative; background-color: black');

    // Loader icon
    that.loader = document.createElement('img');
    that.loader.setAttribute('style', 'width: 16px; height: 16px; position: absolute; top: 50%; left: 50%; margin-top: -8px; margin-left: -8px');
    that.loader.setAttribute('id', 'back_' + that.id);
    that.loader.setAttribute('src', that.url + '/assets/loader.gif');

    // Video tag
    that.video = document.createElement('video');
    that.video.setAttribute('id', 'stream' + that.id);
    that.video.setAttribute('style', 'width: 100%; height: 100%; position: absolute');
    that.video.setAttribute('autoplay', 'autoplay');

    if (that.elementID !== undefined) {
        document.getElementById(that.elementID).appendChild(that.div);
    } else {
        document.body.appendChild(that.div);
    }

    that.parentNode = that.div.parentNode;

    that.div.appendChild(that.loader);
    that.div.appendChild(that.video);

    // Bottom Bar
    that.bar = new Bar({elementID: 'player_' + that.id, id: that.id, video: that.video});

    that.div.onmouseover = onmouseover;
    that.div.onmouseout = onmouseout;

    that.video.src = that.stream_url;

    return that;
};