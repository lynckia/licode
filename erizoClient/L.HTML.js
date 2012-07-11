var L = L || {};

L.HTML = function(L) {

    var init = function() {
        window.location.href.replace(/[?&]+([^=&]+)=([^&]*)/gi, function(m, key, value) {
            document.getElementById(key).value = unescape(value);
        });
    };

    var addVideoToElement = function(id, stream, elementID) {
        var url = webkitURL.createObjectURL(stream);

        var video = document.createElement('video');
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

    var removeVideo = function(id) {
        var video = document.getElementById("stream"+id);
    };

    return {
        init: init,
        addVideoToElement: addVideoToElement
    };
}(L);