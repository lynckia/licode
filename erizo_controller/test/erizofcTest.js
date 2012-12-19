var Erizo = require('./../erizoClient/dist/erizofc');

var token = 'eyJ0b2tlbklkIjoiNTBhZTM2NDk2OWMxNjYyMjI4MDAwMDZkIiwiaG9zdCI6ImNob3RpczIuZGl0LnVwbS5lczo4MDgwIiwic2lnbmF0dXJlIjoiTkRZeE0yVTJPVEJqT1dRNE9EVXpPR0ZsWkdJeVpXWXpOVFE1TXpreU16UTVPVGs0TldJM053PT0ifQ==';

var room = Erizo.Room({token: token});
var stream = Erizo.Stream({audio:true, video:false, data: true, attributes: {name:'myStream'}});

stream.init();

room.addEventListener("room-connected", function(event) {
  console.log("Connected!");
  room.publish(stream);

  setInterval(function(){
  	stream.sendData({msg:'eeeei'});
  }, 1000);

});

room.addEventListener("stream-added", function(event) {
  console.log('stream added', event.stream.getID());

});

room.connect();