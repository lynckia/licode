# Common
A utility module for both node.js and the browser.  
It is available through npm:

	npm install common

Or as minified js file for the browser:

	<script src='common.min.js'></script>

This module among other things contains a fork of [step](https://github.com/creationix/step) that also provides error handling

``` js
common.step([
	function(next) { // next is the last argument, except in the last handler
		fs.readFile(__filename, 'utf-8', next);
	},
	function(file) {
		console.log(file);
	}
], function(err) {
	// any error received in a callback will be forwarded here
});
```

It also contains a shortcut to the `EventEmitter` prototype and a compatible implementation of this for the browser.

``` js
var MyEmitter = common.emitter(function() {
	this.foo = 42;
});

var me = new MyEmitter();

me.emit('foo', me.foo); // emits 'foo',42
```

There is also a more general method for extending prototypes called `extend`:

``` js
// this prototype is the same as above
var MyEmitter = common.extend(events.EventEmitter, function() {
	this.foo = 42;
});
```

If you want to use futures you can use the `future` function to create a future:

``` js
var fut = common.future();

fut.get(function(val) {
	console.log(val);
});
setTimeout(function() {
	fut.put(42); // results in the previous .get being called and all future .get's will be called synchroniously
}, 1000)
```

To do string formatting you can use `format`:

``` js
// you can parse the arguments to a pattern one by one
common.format('define {0} here', 'pattern'); // returns 'define pattern here'

// or as a map or array
common.format('define {foo} here', {foo:'pattern'}); // same as above
```
There is a `log` method that just accepts the does the same as `format` except it prints out the result using `console.log` if available

To generate a simple weak symbols (often used when generating keys for a map) use `gensym`

``` js
common.gensym() // returns 's0'
common.gensym() // returns 's1'
```

If you instead of a weak symbol need a strong one use `uuid`:

``` js
common.uuid(); // returns a strong id, ex: ngDl6IdovME9JKvIxgED0FK1kzURxfZaCq48-0
```

Common can also encode integers into alphanumerical notation using `encode`:

``` js
common.encode(1000); // returns G8
```

To ensure that a method cannot be called more than once you can use the `once` function:

``` js
var fn = common.once(function() {
	console.log('hello');
});

fn(); // prints hello
fn(); // does nothing
```

Besides the above common implements two of the utilities mentioned in The Good Parts, `memoizer` and `curry`.  
