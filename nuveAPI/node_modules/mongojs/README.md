# mongojs
A [node.js](http://nodejs.org) module for mongodb, that emulates [the official mongodb API](http://www.mongodb.org/display/DOCS/Home) as much as possible. It wraps [mongodb-native](https://github.com/christkv/node-mongodb-native/).  
It is available through npm:

	npm install mongojs

## Usage

mongojs is very simple to use:

``` js
var db = require('mongojs').connect(databaseURL, [collections]);
```

Some examples of this could be:

``` js
// simple usage for a local db
var db = require('mongojs').connect('mydb', ['mycollection']);

// the db is on a remote server (the port default to mongo)
var db = require('mongojs').connect('example.com/mydb', ['mycollection']);

// we can also provide some credentials
var db = require('mongojs').connect('username:password@example.com/mydb', ['mycollection']);

// connect now, and worry about collections later
var db = require('mongojs').connect('mydb');
var mycollection = db.collection('mycollection');
```

After we connected we can query or update the database just how we would using the mongo API with the exception that we use a callback  
The format for callbacks is always `callback(error, value)` where error is null if no exception has occured.

``` js
// find everything
db.mycollection.find(function(err, docs) {
	// docs is an array of all the documents in mycollection
});

// find everything, but sort by name
db.mycollection.find().sort({name:1}, function(err, docs) {
	// docs is now a sorted array
});

// iterate over all whose level is greater than 90.
db.mycollection.find({level:{$gt:90}}).forEach(function(err, doc) {
	if (!doc) {
		// we visited all docs in the collection
		return;
	}
	// doc is a document in the collection
});

// find all named 'mathias' and increment their level
db.mycollection.update({name:'mathias'}, {$inc:{level:1}}, {multi:true}, function(err) {
	// the update is complete
});

// use the save function to just save a document (the callback is optional for all writes)
db.mycollection.save({created:'just now'});
```

For more detailed information about the different usages of update and quering see [the mongo docs](http://www.mongodb.org/display/DOCS/Manual)

## Replication Sets

Mongojs can also connect to a mongo replication set

``` js
var db = require('mongojs').connect({
	db: 'mydb',                   // the name of our database
	collections: ['mycollection'], // we can pass the collections here also
	replSet: {
		name: 'myReplSetName',    // the name of the replication set
		slaveOk: true,            // is it ok to read from secondary? defaults to false
		members: ['myserver:myport', 'myotherserver', 'mythirdserver']
	}
});
```

For more detailed information about replica sets see [the mongo replication docs](http://www.mongodb.org/display/DOCS/Replica+Sets)
