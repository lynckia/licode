var L = L || {};

L.HTML = (function (L) {
    "use strict";
    var init, addVideoToElement, removeVideo;

    init = function () {
        window.location.href.replace(/[?&]+([^=&]+)=([^&]*)/gi, function (m, key, value) {
            document.getElementById(key).value = unescape(value);
        });
    };

    addVideoToElement = function(id, stream, elementID) {
        var url, video;
        L.Logger.debug("Creating URL from stream " + stream);
        url = webkitURL.createObjectURL(stream);

        video = document.createElement('video');
        video.setAttribute("id", "stream" + id);
        video.setAttribute("style", "width: 100%; height: 100%");
        video.setAttribute("autoplay", "autoplay");

        if (elementID !== undefined) {
            document.getElementById(elementID).appendChild(video);
        } else {
            document.body.appendChild(video);
        }

        video.src = url;
    };

    removeVideo = function (id) {
        var video = document.getElementById("stream" + id);
        video.close();
        video.getParentNode().removeChild(node);
    };

    return {
        init: init,
        addVideoToElement: addVideoToElement
    };
}(L));