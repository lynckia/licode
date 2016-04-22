var assert = require('chai').assert;
var spine = require ("../spine/simpleNativeConnection");
var testConfigFile = require ("./spineTestConfig.json");

var spineConns = [];


describe('Spine', function() {

    describe('#connectToRoom()', function () {
        it('Should connect to ErizoController', function (done) {
            var testConfig = {};
            testConfig.serverUrl = testConfigFile.basicExampleUrl;
            testConfig.publishConfig = false;
            testConfig.subscribeConfig = false;
            spineConns.push(spine.ErizoSimpleNativeConnection (testConfig, function(msg){
                switch (msg){
                    case "error":
                        throw msg;
                        break;
                    case "room-connected":
                        done();
                        break;
                }
            },
            function (error){
                throw error
            }));
        });
    });

    describe('#publish()', function (){
        this.timeout(5000);
        it('Should publish a video to Licode', function(done){
            var testConfig = {};
            testConfig.serverUrl = testConfigFile.basicExampleUrl;
            testConfig.publishConfig = {
                "video": {"file":"file:///vagrant/test_1000_pli.mkv"}, //TODO: update with test media file
                "audio": true,
                "data" : false
            };
            testConfig.subscribeConfig = false;

            spineConns.push(spine.ErizoSimpleNativeConnection(testConfig, function(msg){
                switch (msg){
                    case "error":
                        throw msg;
                        break;
                    case "stream-added":
                        done();
                        break;
                }
            }, function (error){
                throw error;
            }));

        });
    });

    describe('#publishandsubscribe()', function(){
        this.timeout(15000);
        it('Should publish a video to Licode and subscribe to it', function (done){
            var testConfig = {};
            testConfig.serverUrl = testConfigFile.basicExampleUrl;
            testConfig.publishConfig = {
                "video": {"file":"file:///vagrant/test_1000_pli.mkv"}, //TODO:Update with test media file
                "audio": true,
                "data" : false
            };
            testConfig.subscribeConfig = false;
            var testSubscribeConfig = {};
            testSubscribeConfig.serverUrl = testConfigFile.basicExampleUrl;
            testSubscribeConfig.subscribeConfig = {
                "video": true,
                "audio": true,
                "data": false
            };
            testSubscribeConfig.publishConfig = false;

            spineConns.push(spine.ErizoSimpleNativeConnection (testConfig, function (msg){
                if (msg === "stream-added"){
                    spineConns.push(spine.ErizoSimpleNativeConnection(testSubscribeConfig, 
                                function(subsMsg) {
                                    switch (subsMsg){
                                        case "error":
                                            throw subsMsg;
                                            break;
                                        case "stream-subscribed":
                                            done();
                                            break;
                                    }
                                },
                                function (error){
                                    throw error;
                                }));

                }
            },
            function (error){
                throw error;
            }));

        });
    });

    afterEach(function(){
        while (spineConns.length>0){
            var theConn = spineConns.pop();
            if (theConn.isActive)
                theConn.close();
        }
    });

});
