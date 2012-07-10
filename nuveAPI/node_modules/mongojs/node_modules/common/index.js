var noop = function() {};

var Emitter;

if (!module.browser) {
	Emitter = require('events').EventEmitter; // node-only
} else {
	Emitter = function() {
		this._events = {};
	};

	Emitter.prototype.on = Emitter.prototype.addListener = function(name, listener) {
		this.emit('newListener', name, listener);
		(this._events[name] = this._events[name] || []).push(listener);
	};
	Emitter.prototype.once = function(name, listener) {
		var self = this;

		var onevent = function() {
			self.removeListener(name, listener);
			listener.apply(this, arguments);
		};

		onevent.listener = listener;	
		this.on(name, onevent);
	};
	Emitter.prototype.emit = function(name) {
		var listeners = this._events[name];

		if (!listeners) {
			return;
		}
		var args = Array.prototype.slice.call(arguments, 1);

		listeners = listeners.slice();

		for (var i = 0; i < listeners.length; i++) {
			listeners[i].apply(null, args);
		}
	};
	Emitter.prototype.removeListener = function(name, listener) {
		var listeners = this._events[name];

		if (!listeners) {
			return;
		}
		for (var i = 0; i < listeners.length; i++) {
			if (listeners[i] === listener || (listeners[i].listener === listener)) {
				listeners.splice(i, 1);
				break;
			}
		}
		if (!listeners.length) {
			delete this._events[name];
		}
	};
	Emitter.prototype.removeAllListeners = function(name) {
		if (!arguments.length) {
			this._events = {};
			return;
		}
		delete this._events[name];
	};
	Emitter.prototype.listeners = function(name) {
		return this._events[name] || [];
	};	
}

Object.create = Object.create || function(proto) {
	var C = function() {};
	
	C.prototype = proto;
	
	return new C();
};

exports.extend = function(proto, fn) {
	var C = function() {
		proto.call(this);
		fn.apply(this, arguments);
	};
	C.prototype = Object.create(proto.prototype);

	return C;		
};

exports.createEmitter = function() {
	return new Emitter();
};

exports.emitter = function(fn) {
	return exports.extend(Emitter, fn);
};

// functional patterns below

exports.fork = function(a,b) {
	return function(err, value) {
		if (err) {
			a(err);
			return;
		}
		b(value);
	};
};

exports.step = function(funcs, onerror) {
	var counter = 0;
	var completed = 0;
	var pointer = 0;
	var ended = false;
	var state = {};
	var values = null;
	var complete = false;

	var check = function() {
		return complete && completed >= counter;
	};
	var next = function(err, value) {
		if (err && !ended) {
			ended = true;
			(onerror || noop).apply(state, [err]);
			return;
		}
		if (ended || (counter && !check())) {
			return;
		}

		var fn = funcs[pointer++];
		var args = (fn.length === 1 ? [next] : [value, next]);

		counter = completed = 0;
		values = [];
		complete = false;
		fn.apply(state, pointer < funcs.length ? args : [value, next]);
		complete = true;

		if (counter && check()) {
			next(null, values);
		}
	};
	next.parallel = function() {
		var index = counter++;

		if (complete) {
			throw new Error('next.parallel must not be called async');
		}
		return function(err, value) {
			completed++;
			values[index] = value;
			next(err, values);
		};
	};

	next();
};

exports.memoizer = function(fn) {
	var cache = {};
	
	var stringify = function(obj) {
		var type = typeof obj;

		if (type !== 'object') {
			return type + ': ' + obj;
		}
		var keys = [];
		
		for (var i in obj) {
			keys.push(stringify(obj[i]));
		}
		return keys.sort().join('\n');
	};
	
	return function() {
		var key = '';
		
		for (var i = 0; i < arguments.length; i++) {
			key += stringify(arguments[i]) + '\n';
		}
		
		cache[key] = cache[key] || fn.apply(null, arguments);

		return cache[key];
	};
};

exports.curry = function(fn) {
	var args = Array.prototype.slice.call(arguments, 1);

	return function() {
		return fn.apply(this, args.concat(Array.prototype.slice.call(arguments)));
	};
};

exports.once = function(fn) {
	var once = true;

	return function() {
		if (once) {
			once = false;
			(fn || noop).apply(null, arguments);
			return true;
		}
		return false;
	};
};

exports.future = function() {
	var that = {};
	var stack = [];
	
	that.get = function(fn) {
		stack.push(fn);
	};
	that.put = function(a,b) {
		that.get = function(fn) {
			fn(a,b);
		};
		
		while (stack.length) {
			stack.shift()(a,b);
		}
	};
	return that;
};

// utilities below

exports.encode = function(num) {
	var ALPHA = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz';

	return function(i) {
		return i < ALPHA.length ? ALPHA.charAt(i) : exports.encode(Math.floor(i / ALPHA.length)) + ALPHA.charAt(i % ALPHA.length);
	};
}();

exports.uuid = function() {
	var inc = 0;		

	return function() {
		var uuid = '';

		for (var i = 0; i < 36; i++) {
			uuid += exports.encode(Math.floor(Math.random() * 62));
		}
		return uuid + '-' + exports.encode(inc++);			
	};
}();

exports.gensym = function() {
	var s = 0;
	
	return function() {
		return 's'+(s++);
	};
}();

exports.join = function() {
	var result = {};
	
	for (var i = 0; i < arguments.length; i++) {
		var a = arguments[i];
		
		for (var j in a) {
			result[j] = a[j];
		}
	}
	return result;
};

exports.format = function (str, col) {
	col = typeof col === 'object' ? col : Array.prototype.slice.call(arguments, 1);

	return str.replace(/\{([^{}]+)\}/gm, function () {
		return col[arguments[1]] === undefined ? arguments[0] : col[arguments[1]];
	});
};

exports.log = function(str) {
	if (typeof window !== 'undefined' && !window.console) {
		return;
	}
	console.log(str);
};
