require('./harness');

var recvCount = 0;
var body = "Some say the devil is dead";

var later = function(fun) {
    setTimeout(fun, 500);
}
connection.addListener('ready', function () {
    puts("connected to " + connection.serverProperties.product);

    var q = connection.queue('node-simple-queue');
    var exchange = connection.exchange('node-simple-fanout', {type: 'fanout'});

    exchange.on('open', function(){

	q.bind(exchange, "");
	q.subscribe(function(message){recvCount+=1;});

	exchange.publish('', body);

	q.unbind(exchange, "");
	later(function(){
	    exchange.publish('', body);
	    later(function(){connection.end()});
	});
    });
});

process.addListener('exit', function () {
    assert.equal(1, recvCount);
});
