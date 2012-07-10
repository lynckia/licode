var mongo = require('mongodb');
var common = require('common');
var url = require('url');

var noop = function() {};

var createObjectId = function() {
	if (!mongo.BSONNative || !mongo.BSONNative.ObjectID) {
	  return function(id) {
		return mongo.BSONPure.ObjectID.createFromHexString(id);
	  };
	}
	return function(id) {
		return new mongo.BSONNative.ObjectID(id);
	};
}();
var parseURL = function(options) {
	if (typeof options === 'object') {
		options.host = options.host || '127.0.0.1';
		options.port = parseInt(options.port || 27017, 10);
		return options;
	}
	if (/^[^:\/]+$/.test(options)) {
		options = 'mongodb://127.0.0.1:27017/'+options;
	}

	var parsed = url.parse('mongodb://'+options.replace('mongodb://', ''));
	var result = {};

	result.username = parsed.auth && parsed.auth.split(':')[0];
	result.password = parsed.auth && parsed.auth.split(':')[1];
	result.db = (parsed.path || '').substr(1);
	result.host = parsed.hostname;
	result.port = parsed.port;

	return parse(result);
};
var parse = function(options) {
	options = parseURL(options);

	if (options.replSet && Array.isArray(options.replSet)) {
		options.replSet = {members:options.replSet};
	}
	if (options.replSet) {
		if (!options.replSet.members) {
			throw new Error('replSet.members required');
		}
		options.replSet.members = options.replSet.members.map(parseURL);
	}

	return options;
};
var shouldExtend = function(that, proto, name) {
	if (name[0] === '_') return false;
	return !that[name] && !proto.__lookupGetter__(name) && typeof proto[name] === 'function';
};

// basicly just a proxy prototype
var Cursor = function(oncursor) {
	this._oncursor = oncursor;
};

Cursor.prototype.toArray = function() {
	this._exec('toArray', arguments);
};
Cursor.prototype.next = function() {
	this._exec('nextObject', arguments);
};
Cursor.prototype.forEach = function() {
	this._exec('each', arguments);
};
Cursor.prototype.count = function() {
	this._exec('count', arguments);
};
Cursor.prototype.sort = function() {
	return this._config('sort', arguments);
};
Cursor.prototype.limit = function(a) {
	return this._config('limit', arguments);
};
Cursor.prototype.skip = function() {
	return this._config('skip', arguments);
};

Cursor.prototype._config = function(name, args) {
	if (typeof args[args.length-1] === 'function') {
		args = Array.prototype.slice.call(args);

		var callback = args.pop();

		this._exec(name, args).toArray(callback);
		return;
	}
	return this._exec(name, args);
};
Cursor.prototype._exec = function(name, args) {
	var callback = typeof args[args.length-1] === 'function' ? args[args.length-1] : noop;

	this._oncursor.get(common.fork(callback, function(cur) {
		cur[name].apply(cur, args);
	}));
	return this;
};

var Collection = function(oncollection) {
	this._oncollection = oncollection;
};

Collection.prototype.find = function() {
	var args = Array.prototype.slice.call(arguments);
	var oncursor = common.future();
	var oncollection = this._oncollection;

	// we provide sugar for doing find(query, callback) -> find(query).toArray(callback);
	if (typeof args[args.length-1] === 'function') {
		var callback = args.pop();

		oncursor.get(common.fork(callback, function(cur) {
			cur.toArray(callback);
		}));
	}

	common.step([
		function(next) {
			oncollection.get(next);
		},
		function(col, next) {
			args.push(next);
			col.find.apply(col, args);
		},
		function(cur) {
			oncursor.put(null, cur);
		}
	], oncursor.put);

	return new Cursor(oncursor);
};
Collection.prototype.findOne = function() { // see http://www.mongodb.org/display/DOCS/Queries+and+Cursors
	var args = Array.prototype.slice.call(arguments);
	var callback = args.pop();

	this.find.apply(this, args).limit(1).next(callback);
};
Collection.prototype.findAndModify = function(options, callback) {
	this._exec('findAndModify', [options.query, options.sort || [], options.update || {}, {
		new:!!options.new,
		remove:!!options.remove,
		upsert:!!options.upsert,
		fields:options.fields
	}, callback]);
};
Collection.prototype.remove = function() {
	this._exec('remove', arguments.length === 0 ? [{}] : arguments); // driver has a small issue with zero-arguments in remove
};

Collection.prototype._exec = function(name, args) {
	var callback = typeof args[args.length-1] === 'function' ? args[args.length-1] : noop;

	this._oncollection.get(common.fork(callback, function(col) {
		var old = col.opts.safe;

		if (callback !== noop) {
			col.opts.safe = true;
		}
		col[name].apply(col, args);
		col.opts.safe = old;
	}));
};

Collection.prototype.disconnect = function() {
	this.close();
};

Object.keys(mongo.Collection.prototype).forEach(function(name) { // we just wanna proxy any remaining methods on collections
	if (shouldExtend(Collection.prototype, mongo.Collection.prototype, name)) {
		Collection.prototype[name] = function() {
			this._exec(name, arguments);
		};
	}
});

exports.ObjectId = createObjectId;

exports.connect = function(url, collections) {
	url = parse(url);
	collections = collections || url.collections;

	var that = {};
	var ondb = common.future();

	common.step([
		function(next) {
			var replSet = url.replSet && new mongo.ReplSetServers(url.replSet.members.map(function(member) {
				return new mongo.Server(member.host, member.port, {auto_reconnect:true});
			}), {
				read_secondary:url.replSet.slaveOk,
				rs_name:url.replSet.name
			});

			var client = new mongo.Db(url.db, replSet || new mongo.Server(url.host, url.port, {auto_reconnect:true}));

			that.client = client;
			that.bson = {
				Long:      client.bson_serializer.Long,
				ObjectID:  client.bson_serializer.ObjectID,
				Timestamp: client.bson_serializer.Timestamp,
				DBRef:     client.bson_serializer.DBRef,
				Binary:    client.bson_serializer.Binary,
				Code:      client.bson_serializer.Code
			};

			client.open(next);
		},
		function(db, next) {
			this.db = db;

			if (url.username) {
				db.authenticate(url.username, url.password, next);
			} else {
				next(null, true);
			}
		},
		function(success) {
			if (!success) {
				ondb.put(new Error('invalid username or password'));
				return;
			}
			ondb.put(null, this.db);
		}
	], ondb.put);

	that.ObjectId = createObjectId;
	that.collection = function(name) {
		var oncollection = common.future();

		common.step([
			function(next) {
				ondb.get(next);
			},
			function(db, next) {
				db.collection(name, next);
			},
			function(col) {
				oncollection.put(null, col);
			}
		], oncollection.put);

		return new Collection(oncollection);
	};

	Object.keys(mongo.Db.prototype).forEach(function(name) {
		if (shouldExtend(that, mongo.Db.prototype, name)) {
			that[name] = function() {
				var args = arguments;
				var callback = args[args.length-1] || noop;

				ondb.get(common.fork(callback, function(db) {
					db[name].apply(db, args);
				}));
			};
		}
	});

	if (collections) {
		collections.forEach(function(col) {
			that[col] = that.collection(col);
		});
	}
	if (typeof Proxy !== 'undefined') {
		return Proxy.create({
			get: function(proxy, name) {
				if (!that[name]) {
					that[name] = that.collection(name);
				}
				return that[name];
			}
		});
	}

	return that;
};

