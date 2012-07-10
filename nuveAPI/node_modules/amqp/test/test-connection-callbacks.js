testLog = function(name, message) { console.log("Test case: "+name+":", message); };
assert =  require('assert');
amqp = require('../amqp');

var options = global.options || {};
if (process.argv[2]) {
  var server = process.argv[2].split(':');
  if (server[0]) options.host = server[0];
  if (server[1]) options.port = parseInt(server[1]);
}

var implOpts = {
  defaultExchangeName: 'amq.topic'
};

var callbackCalled = false;

var testCases = [
    { name: "OK readyCallback",                   callback: readyCallback },
    { name: "no readyCallback",                       callback: undefined  },
    { name: "null readyCallback",                     callback: null },
    { name: "non-function readyCallback",       callback: "Not a function" },

];

function cycleThroughTestCases(index) {
    index = index || 0;
    if (index >= testCases.length) {
        testLog("", "All done!");
        return;
    }
    
    var testCase = testCases[index];  
    var connection;
    
    testLog(testCase.name,"Starting...");
    
    callbackCalled = false;
    
    if (testCase.callback === undefined) {
        connection = amqp.createConnection(options, implOpts);
    } 
    else {
        connection = amqp.createConnection(options, implOpts, testCase.callback);
    }
    connection.on('ready', function() {
        testLog(testCase.name,"Connection ready. Checking results...");
        if (typeof testCase.callback === 'function') {
            assert(callbackCalled, "readyCallback was not called");
        }
        else {
            assert(!callbackCalled, "readyCallback was unexpectedly called");
        }
        testLog(testCase.name,"Success! Closing connection.");
        connection.destroy();
    });
    connection.on('error', function(e) {
        testLog(testCase.name,"Connection error. Abandoning test cycle.");
        throw(e);
    });
    connection.on('close', function(e) {
        testLog(testCase.name,"Connection closed. Starting next test case.");
				cycleThroughTestCases(index+1);
    });
    
};

function readyCallback(connection) {
    callbackCalled = true;
};

cycleThroughTestCases();

