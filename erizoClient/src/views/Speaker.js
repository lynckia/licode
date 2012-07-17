var Speaker = function(spec) {
    var that = EventDispatcher({});
    that.elementID = spec.elementID;
    that.video = spec.video;
    that.id = spec.id;
    that.div = document.createElement('div');
    that.div.setAttribute('style', 'width: 32px; height: 32px; position: absolute; bottom: 0; right: 0;');
    that.icon = document.createElement('img');
    
    that.icon.setAttribute('id', 'volume_' + that.id);
    that.icon.setAttribute('src', 'http://hpcm.dit.upm.es/assets/mute48.png');
    that.icon.setAttribute('style', 'width: 32px; height: 32px; position: absolute;');
    that.div.appendChild(that.icon);


    that.picker = document.createElement('input');
    that.picker.setAttribute('id', 'picker_' + that.id);
    that.picker.type = "range";
    that.picker.min = 0;
    that.picker.max = 100;
    that.picker.step = 10;
    that.picker.value = 50;
    that.div.appendChild(that.picker);
    that.video = that.picker.value;

    that.picker.oninput = function(evt) {
        if (that.picker.value > 0) {
            that.icon.setAttribute('src', 'http://hpcm.dit.upm.es/assets/sound48.png');
        } else {
            that.icon.setAttribute('src', 'http://hpcm.dit.upm.es/assets/mute48.png');
        }
        that.video = that.picker.value;
    };

    var show = function(displaying) {
        that.picker.setAttribute('style', 'width: 32px; height: 100px; position: absolute; bottom: 30px; right: 0px; -webkit-appearance: slider-vertical; display: '+displaying);
    };

    show('none');

    that.div.onmouseover = function(evt) {
        show('block');
    };

    that.div.onmouseout = function(evt) {
        show('none');
    };

    document.getElementById(that.elementID).appendChild(that.div);
    return that;
}