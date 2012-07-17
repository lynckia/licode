var VideoPlayer = function(spec) {
    var that = View({});

    window.location.href.replace(/[?&]+([^=&]+)=([^&]*)/gi, function (m, key, value) {
        document.getElementById(key).value = unescape(value);
    });

    var onmouseover = function(evt) {
        that.bar.display();
    };

    var onmouseout = function(evt) {
        that.bar.hide();
    };

    that.destroy = function() {
        that.video.pause();
        that.parentNode.removeChild(that.div);
    };
    
    that.id = spec.id;
    that.stream = spec.stream;
    that.elementID = spec.elementID;

    L.Logger.debug('Creating URL from stream ' + that.stream);
    that.url = webkitURL.createObjectURL(that.stream);

    that.div = document.createElement('div');
    that.div.setAttribute('id', 'player_' + that.id);
    that.div.setAttribute('style', 'width: 100%; height: 100%; position: relative; background-color: black');

    that.loader = document.createElement('img');
    that.loader.setAttribute('style', 'width: 16px; height: 16px; position: absolute; top: 50%; left: 50%; margin-top: -8px; margin-left: -8px');
    that.loader.setAttribute('id', 'back_' + that.id);
    that.loader.setAttribute('src', that.url + '/assets/loader.gif');

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
    that.bar = new Bar({elementID: 'player_' + that.id, id: that.id, video: that.video});

    that.div.onmouseover = onmouseover;
    that.div.onmouseout = onmouseout;

    that.video.src = that.url;

    return that;
};