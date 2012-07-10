# node-amqp

IMPORTANT: This module only works with node v0.4.0 and later.

This is a client for RabbitMQ (and maybe other servers?). It partially
implements the 0.9.1 version of the AMQP protocol.

## Installation

    npm install amqp

## Synopsis

An example of connecting to a server and listening on a queue.

```javascript
var amqp = require('amqp');

var connection = amqp.createConnection({ host: 'dev.rabbitmq.com' });

// Wait for connection to become established.
connection.on('ready', function () {
  // Create a queue and bind to all messages.
  // Use the default 'amq.topic' exchange
  connection.queue('my-queue', function(q){
      // Catch all messages
      q.bind('#');
    
      // Receive messages
      q.subscribe(function (message) {
        // Print messages to stdout
        console.log(message);
      });
  });
});
```

## Connection

`new amqp.Connection()` Instantiates a new connection. Use
`connection.connect()` to connect to a server.

`amqp.createConnection()` returns an instance of `amqp.Connection`, which is
a subclass of `net.Stream`. All the event and methods which work on
`net.Stream` can also be used on an `amqp.Connection` instance. (e.g., the
events `'connected'` and `'closed'`.)

### Connection options and URL

`amqp.createConnection([options, [implOptions]])` takes two options
objects as parameters.  The first options object has these defaults:

    { host: 'localhost'
    , port: 5672
    , login: 'guest'
    , password: 'guest'
    , vhost: '/'
    }

All of these can be passed in a single URL of the form

    amqp[s]://[user:password@]hostname[:port][/vhost]

Note that the vhost must be URL-encoded and appear as the only segment
of the path, i.e., the only unencoded slash is that leading; leaving
the path entirely empty indicates that the vhost `/`, as
above, should be used (it could also be supplied as the path `/%2f`).

This URL is supplied as the field `url` in the options; for example

```javascript
var conn =
  amqp.createConnection({url: "amqp://guest:guest@localhost:5672"});

```


Options provided as individual fields will override values given in
the URL.

The second options are specific to the node AMQP implementation. It has
the default values:

    { defaultExchangeName: '' }

The defaultExchangeName is the default exchange to which
connection.publish will publish. In the past, the default exchange was
`amq.topic`, which is not ideal.  To emulate this behaviour, one can
create a connection like:

```javascript
var conn =
  amqp.createConnection({url: "amqp://guest:guest@localhost:5672"},
                        {defaultExchangeName: "amq.topic"});
```


After a connection is established the `'connect'` event is fired as it is
with any `net.Connection` instance. AMQP requires a 7-way handshake which
must be completed before any communication can begin. `net.Connection` does
the handshake automatically and emits the `'ready'` event when the handshaking
is complete.

For backward compatability, two additional options are available. Older
versions of this library placed the routingKey and deliveryTag for incoming
messages into the JSON payload received. This module was changed to
leave inbound JSON payloads pristine.  Some applications may need the
old behaviour. If the key 'routingKeyInPayload' is set to true in the
connection options, the messages resulting from a subscribe call will
include a 'routingKey' key in the JSON payload.  If the key
'devliryTagInPayload' is set to true in the connection options, the
deliveryTag of the incoming message will be placed in the JSON payload.


### connection.publish(queueName, body)

Publishes a message to the default exchange; if the defaultExchange is
left as `''`, this effectively publishes the message to the queue
named.

### connection.end()

`amqp.Connection` is derived from `net.Stream` and has all the same methods.
So use `connection.end()` to terminate a connection gracefully.




## Queue

Events: A queue will call the callback given to the `connection.queue()`
method once it is usable. For example:

```javascript
var q = connection.queue('my-queue', function (queue) {
  console.log('Queue ' + queue.name + ' is open');
});
```


Declaring a queue with an empty name will make the server generate a
random name.

### connection.queue(name, options, openCallback)

Returns a reference to a queue. The options are

- `passive`: boolean, default false.
    If set, the server will not create the queue.  The client can use
    this to check whether a queue exists without modifying the server
    state.
- `durable`: boolean, default false.
    Durable queues remain active when a server restarts.
    Non-durable queues (transient queues) are purged if/when a
    server restarts.  Note that durable queues do not necessarily
    hold persistent messages, although it does not make sense to
    send persistent messages to a transient queue.
- `exclusive`: boolean, default false.
    Exclusive queues may only be consumed from by the current connection.
    Setting the 'exclusive' flag always implies 'autoDelete'.
- `autoDelete`: boolean, default true.
    If set, the queue is deleted when all consumers have finished
    using it. Last consumer can be cancelled either explicitly or because
    its channel is closed. If there was no consumer ever on the queue, it
    won't be deleted.

### queue.subscribe([options,] listener)

An easy subscription command. It works like this

```javascript
q.subscribe(function (message, headers, deliveryInfo) {
  console.log('Got a message with routing key ' + deliveryInfo.routingKey);
});
    
```

It will automatically acknowledge receipt of each message.

There are several options available.  Setting the options argument to
`{ ack: true }` (which defaults to false) will make it so that the AMQP
server only delivers a single message at a time. When you want the next
message, call `q.shift()`. When `ack` is false then you will receive
messages as fast as they come in. 

You can also use the prefetchCount option to increase the window of how
many messages the server will send you before you need to ack (quality of service).
'{ ack: true, prefetchCount: 1 }' is the default and will only send you one
message before you ack. Setting prefetchCount to 0 will make that window unlimited.

The 'routingKeyInPayload' and 'deliveryKeyInPayload' options determine
if the reception process will inject the routingKey and deliveryKey,
respectively, into the JSON payload received.  These default to unset
thus adopting the parent connection's values (which default to false).
Setting these to true provide backward compability for older
applications.

This method will emit `'basicQosOk'` when ready.


### queue.subscribeRaw([options,] listener)

Subscribes to a queue. The `listener` argument should be a function which
receives a message. This is a low-level interface - the message that the
listener receives will be a stream of binary data. You probably want to use
`subscribe` instead. For now this low-level interface is left undocumented.
Look at the source code if you need to do this.

This method will emit `'basicConsumeOk'` when ready.

### queue.unsubscribe(consumerTag)

Unsubscribe from a queue, given the consumer tag. The consumer tag is
supplied to the *promise callback* of `Queue.subscribeRaw` or
`Queue.subscribe`:

```javascript
connection.queue('foo', function(queue) {
  var ctag;
  queue.subscribe(function(msg) {...})
    .addCallback(function(ok) { ctag = ok.consumerTag; });
  // ... and in some other callback
  queue.unsubscribe(ctag);
});
```

Note that `Queue.unsubscribe` will not requeue messages that have not
been acknowledged. You need to close the queue or connection for that
to happen. You may also receive messages after calling `unsubscribe`;
you will **not** receive messages from the queue after the unsubscribe
promise callback has been invoked, however.

### queue.shift()

For use with `subscribe({ack: true}, fn)`. Acknowledges the last
message.


### queue.bind([exchange,] routing)

This method binds a queue to an exchange.  Until a queue is
bound it will not receive any messages, unless they are sent through
the unnamed exchange (see `defaultExchangeName` above).

If the `exchange` argument is left out `'amq.topic'` will be used.

This method will emit `'queueBindOk'` when ready.

### queue.bind_headers([exchange,] routing)

This method binds a queue to an exchange.  Until a queue is
bound it will not receive any messages.

This method is to be used on an "headers"-type exchange. The routing
argument must contain the routing keys and the `x-match` value (`all` or `any`).

If the `exchange` argument is left out `'amq.headers'` will be used.

### queue.destroy(options)

Delete the queue. Without options, the queue will be deleted even if it has
pending messages or attached consumers. If +options.ifUnused+ is true, then
the queue will only be deleted if there are no consumers. If
+options.ifEmpty+ is true, the queue will only be deleted if it has no
messages.




## Exchange

Events: An exchange will call the callback given to the `connection.exchange()`
method once it is usable. For example:

```javascript
var exc = connection.exchange('my-exchange', function (exchange) {
  console.log('Exchange ' + exchange.name + ' is open');
});
```

### exchange.on('open', callback)

The open event is emitted when the exchange is declared and ready to
be used. This interface is considered deprecated.


### connection.exchange()
### connection.exchange(name, options={}, openCallback)

An exchange can be created using `connection.exchange()`. The method returns
an `amqp.Exchange` object.

Without any arguments, this method returns the default exchange.
Otherwise a string, `name`, is given as the first argument and an `options`
object for the second. The options are

- `type`: the type of exchange `'direct'`, `'fanout'`, or `'topic'` (default).
- `passive`: boolean, default false.
    If set, the server will not create the exchange.  The client can use
    this to check whether an exchange exists without modifying the server
    state.
- `durable`: boolean, default false.
    If set when creating a new exchange, the exchange will be marked as
    durable.  Durable exchanges remain active when a server restarts.
    Non-durable exchanges (transient exchanges) are purged if/when a
    server restarts.
- `autoDelete`: boolean, default true.
    If set, the exchange is deleted when there are no longer queues
    bound to it.

An exchange will emit the `'open'` event when it is finally declared.


### exchange.publish(routingKey, message, options)

Publishes a message to the exchange. The `routingKey` argument is a string
which helps routing in `topic` and `direct` exchanges. The `message` can be
either a Buffer or Object. A Buffer is used for sending raw bytes; an Object
is converted to JSON.

`options` is an object with any of the following

- `mandatory`: boolean, default false.
    This flag tells the server how to react if the message cannot be
    routed to a queue.  If this flag is set, the server will return an
    unroutable message with a Return method.  If this flag is false, the
    server silently drops the message.
- `immediate`: boolean, default false.
    This flag tells the server how to react if the message cannot be
    routed to a queue consumer immediately.  If this flag is set, the
    server will return an undeliverable message with a Return method.
    If this flag is false, the server will queue the message, but with
    no guarantee that it will ever be consumed.
- `contentType`: default `'application/octet-stream'`
- `contentEncoding`: default null.
- `headers`: default `{}`. Arbitrary application-specific message headers.
- `deliveryMode`: Non-persistent (1) or persistent (2)
- `priority`: The message priority, 0 to 9.
- `replyTo`: Usually used to name a reply queue for a request message.

### exchange.destroy(ifUnused = true)

Deletes an exchange.
If the optional boolean second argument is set, the server will only
delete the exchange if it has no queue bindings. If the exchange has queue
bindings the server does not delete it but raises a channel exception
instead.
