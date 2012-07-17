var L = L || {};

/*
 * API to modify the HTML DOM. It allows the addition and removal of <video/> tags in HTML
 */

L.HTML = (function (L) {
    "use strict";
    var init, addVideoToElement, removeVideo;

    // It initializes the component. We only need to call it once
    init = function () {
        window.location.href.replace(/[?&]+([^=&]+)=([^&]*)/gi, function (m, key, value) {
            document.getElementById(key).value = unescape(value);
        });
    };

    // It adds a new video node into the HTML document
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

    // It removes a <video/> tag from the HTML document
    removeVideo = function (id) {
        var video = document.getElementById("stream" + id);
        video.pause();
        video.parentNode.removeChild(video);
    };

    return {
        init: init,
        addVideoToElement: addVideoToElement,
        removeVideo: removeVideo
    };
}(L));