--- 
layout: post 
title: Screen sharing with Licode
--- 

We have just implemented a new very interesting functionality in Licode. From today you can include in your Licode applications the feature of share any client screen using the GetUserMedia API of WebRTC.

This is very easy. To allow it in a Stream you only need to create it this way:

var stream = Erizo.Stream({screen: true, data: true, attributes: {name:'myStream'}});

And then publish it in a room in the traditional way. Instructions in [Client API](http://lynckia.com/licode/client-api.html). 

We think this is a very useful feature and, in the tests that we have made, its performance its very amazing. We are also developing a demo service in order to you can try the new functionality and use to share your screen in any scenario.

Note that the clients that will share their screen have to access your web application using a secure connection (https protocol). Furthermore, by the moment, he or she has to allow the access activating a flag in Chrome browser (Enable screen capture support in getUserMedia()).
