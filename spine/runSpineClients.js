'use strict';
var Getopt = require('node-getopt');
var efc = require ('./simpleNativeConnection');

var getopt = new Getopt([
  ['s' , 'stream-config=ARG'             , 'file containing the stream config JSON'],
  ['h' , 'help'                          , 'display this help']
]);

var opt = getopt.parse(process.argv.slice(2));

var streamConfig;

for (var prop in opt.options) {
    if (opt.options.hasOwnProperty(prop)) {
        var value = opt.options[prop];
        switch (prop) {
            case 'help':
                getopt.showHelp();
                process.exit(0);
                break;
            case 'stream-config':
                streamConfig = value;
                break;
            default:
                console.log('Default');
                break;
        }
    }
}

if (!streamConfig){
    streamConfig = 'spineClientsConfig.json';
}

console.log('Loading stream config file', streamConfig);
streamConfig = require('./' + streamConfig);

if (streamConfig.publishConfig){
    var streamPublishConfig = {
        publishConfig: streamConfig.publishConfig,
        serverUrl: streamConfig.basicExampleUrl
    };

    if (streamConfig.publishersAreSubscribers){
        streamPublishConfig.subscribeConfig = streamConfig.subscribeConfig;
    }
}

if (streamConfig.subscribeConfig){
    var streamSubscribeConfig = {
        subscribeConfig : streamConfig.subscribeConfig,
        serverUrl : streamConfig.basicExampleUrl
    };
    console.log('StreamSubscribe', streamSubscribeConfig);
}



var startStreams = function(stConf, num, time){
    var started = 0;
    var interval = setInterval(function(){
        if(++started>=num){
            console.log('All streams have been started');
            clearInterval(interval);
        }
        console.log('Will start stream with config', stConf);
        efc.ErizoSimpleNativeConnection (stConf, function(msg){
            console.log('Getting Callback', msg);
        }, function(msg){
            console.error('Error message', msg);
        });
    }, time);
};

console.log('Starting ', streamConfig.numSubscribers, 'subscriber streams',
        'and', streamConfig.numPublishers, 'publisherStreams');

if (streamSubscribeConfig && streamConfig.numSubscribers)
    startStreams(streamSubscribeConfig,
                 streamConfig.numSubscribers,
                 streamConfig.connectionCreationInterval);
if (streamPublishConfig && streamConfig.numPublishers)
    startStreams(streamPublishConfig,
                 streamConfig.numPublishers,
                 streamConfig.connectionCreationInterval);
