require('./harness');
var assert = require('assert');
var cbcnt = 0;

connection.on('ready', function(){
    var exchange1 = connection.exchange('test-exchange-1');
    var exchange2 = connection.exchange('test-exchange-2');

    var queue = connection.queue('test-queue', function() {

        assert.equal(2, Object.keys(connection.exchanges).length);
        assert.equal(4, Object.keys(connection.channels).length);

        var messages = 0;

        exchange1.on('close', function(){
            cbcnt++;
            assert.equal('closed', exchange1.state);
            assert.equal(1, Object.keys(connection.exchanges).length);
            assert.equal(3, Object.keys(connection.channels).length);
            exchange2.publish('','test3');
            exchange2.destroy();
            exchange2.close();
        });

        exchange2.on('close', function(){
            cbcnt++;
            assert.equal('closed', exchange2.state);
            assert.equal(2, Object.keys(connection.channels).length);
            assert.equal("3", Object.keys(connection.channels)[1]);
            assert.equal(2, messages);
            connection.destroy();
        });

        queue.bind(exchange2, '');

        queue.subscribe(function(message){
            messages++;
        }).addCallback(function(){
            exchange1.publish('','test1');
            exchange2.publish('','test2');
            exchange1.destroy();
            exchange1.close();
            assert.equal('closing', exchange1.state);
        });
    });
});
process.addListener('exit', function () {
  assert.equal(cbcnt, 2);
});
