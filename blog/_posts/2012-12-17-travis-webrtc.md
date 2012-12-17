--- 
layout: post 
title: Automated testing in Travis CI for WebRTC
--- 


Intro
----

We've been working on developing tests for checking WebRTC connections 
in Lynckia and Google Chrome (stable and unstable versions). We started 
using Travis for the server tests some weeks ago, but at this time, we
wanted to go beyond simple tests of Nuve and basic room's functionality.

Our idea is to test publishng, subscribing and further features of our 
client library in Chrome and in future versions of Firefox. During these
tests we need to automatically start Chrome, accept the browser's 
GetUserMedia request, and to send a fake video stream to check whether 
streams are sent or not.

Testing environment
-----------------

For this puporse we used three different and compatible frameworks:

1. [*Travis CI*](https://travis-ci.org/): We continued using it for running 
tests in a standalone machine. It allows to setup experimental environments 
based on a Linux machine that is almost fully configurable.

 ![Travis Logo](http://i.imgur.com/0qPjt.png?1 "Travis-CI Logo")

2. [*Testacular*](http://vojtajina.github.com/testacular/): This is a Test 
Runner for JavaScript that allows to test JS applications directly on the 
browser. It supports different providers, such as Google Chrome, Firefox, 
and so on. And it also supports custom browser launchers. 

 Testacular also allows you to use several testing frameworks, but it comes 
 with built-in Jasmine and Mocha integration.

3. [*Jasmine*](http://pivotal.github.com/jasmine/): It is a behavior-driven 
development framework for testing JavaScript code. It does not depend on any 
other JavaScript frameworks. It does not require a DOM. And it has a clean, 
obvious syntax so that you can easily write tests.
 
 We've also tested it against Mocha in our experiments, so you can choose 
 your own framework.

Setting up Travis and Testacular
---------------------------

We first need to install Chrome in the Travis machine for every test. Travis 
helps us to do it by the <code>before_install</code> command:

    language: node_js
    node_js:
      - 0.8
    before_install:
      - ./.travis/scripts/install_chrome.sh
      - export DISPLAY=:99.0
      - sh -e /etc/init.d/xvfb start

In this case you have to create the <code>install_chrome.sh</code> script to 
start chrome before the tests. You can access this script [here]
(https://github.com/ging/lynckia/blob/master/.travis/scripts/install_chrome.sh).



Since Travis runs <code>npm test</code> for running tests we also need to 
create a <code>package.json</code> with info about the tests. Here we should 
add the next lines to our code:

    "scripts": {
        "test": "./node_modules/coffee-script/bin/cake test"
    } 

And we'll also add the corresponding Cakefile:

    exec = require('child_process').exec
    task 'test', 'run all tests suites', ->
        console.log 'Running front-end tests'
        chrome_bin = "DISPLAY=:99"
        testacular = "#{__dirname}/node_modules/testacular/bin/testacular"
        browsers = '.travis/chrome-start.sh'
        options = "--single-run --browsers=#{browsers}"
        exec "#{chrome_bin} #{testacular} start #{__dirname}/.travis/testacular.conf.js", (err, stdout, stderr) ->
            console.error err if err
            console.log stdout

In the 
[chrome-start.sh](https://github.com/ging/lynckia/blob/master/.travis/chrome-start.sh) 
file we have the corresponding starting scripts. With this script we setup 
Chrome with some default Chrome 
[Preferences](https://github.com/ging/lynckia/blob/master/.travis/Preferences): 

* It allows the user to start Chrome and use Webrtc from a local URL 
(http://localhost:9876/) without asking permissions. This is configured in the 
Preferences file.

* It also starts Chrome by using a fake video device, that is used to send 
synthetic video packets between clients. This is achieved in the starting script, 
with the option: <code>--use-fake-device-for-media-stream</code>. Below you can see the resulting video.

 ![Fake Video on Google Chrome](http://i.imgur.com/Q0ReP.png?1)

Testing
------

For testing we used Jasmine and its asynchronous functionalities. We use it to 
test Lynckia but it can also be used to test WebRTC applications. Below we show 
one of our examples in the 
[test cases](https://github.com/ging/lynckia/blob/master/test/nuve-test.js):

    it('should get access to user media', function () {
        var callback = jasmine.createSpy("getusermedia");

        localStream = Erizo.Stream({audio: true, video: true, data: true});

        localStream.addEventListener("access-accepted", callback);

        localStream.init();

        waitsFor(function () {
            return callback.callCount > 0;
        });

        runs(function () {

            expect(callback).toHaveBeenCalled();
        });
    });

In this example we call the GetUserMedia command, and it automatically accepts on 
behalf of the user given the previous configuration. Once it is accept we check 
whether the Jasmine's Spy has been called.