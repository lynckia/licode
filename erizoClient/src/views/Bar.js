var Bar = function(spec) {
    var that = EventDispatcher({});
    that.elementID = spec.elementID;
    that.id = spec.id;
    var waiting = undefined;

    that.div = document.createElement('div');
    that.div.setAttribute('id', 'bar_'+that.id);

    that.bar = document.createElement('div');
    that.bar.setAttribute('style', 'width: 100%; height: 32px; position: absolute; bottom: 0; right: 0; background-color: rgba(255,255,255,0.62)');
    that.bar.setAttribute('id', 'subbar_'+that.id);

    that.logo = document.createElement('img');
    that.logo.setAttribute('style', 'width: 32px; height: 32px; position: absolute; top: 0; left: 0;');
    that.logo.setAttribute('src', 'http://hpcm.dit.upm.es/assets/star.svg');

    var show = function(displaying) {
        if (displaying !== 'block') {
            displaying = 'none';
        } else {
            clearTimeout(waiting);
        }
        
        that.div.setAttribute('style', 'width: 100%; height: 100%; position: relative; bottom: 0; right: 0; display:'+displaying);
    };

    that.display = function() {
        show('block');
    };

    that.hide = function() {
        waiting = setTimeout(show, 1000);
    };

    console.log(that.elementID);
    document.getElementById(that.elementID).appendChild(that.div);
    that.div.appendChild(that.bar);
    that.bar.appendChild(that.logo);
    that.speaker = new Speaker({elementID: 'bar_'+that.id, id: that.id, video: spec.video});
    that.display();
    that.hide();
    return that;
};