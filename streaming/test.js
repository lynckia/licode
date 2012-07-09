var addon = require('./build/Release/addon');

var wr = new addon.WebRtcConnection();
wr.init();

var o = new addon.OneToManyProcessor();

//o.setPublisher(wr);

if(o->me.publisher == NULL) {
	console.log('noooo');
}

wr.setAudioReceiver(o);

//console.log('Local SDP\n: ', wr.getLocalSdp());
