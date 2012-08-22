# ErizoAPI, a Node.js addon wrapper for erizo, the MCU Library for WebRTC 

Erizo is a project that aims to implement a library able to communicate with WebRTC (http://www.webrtc.org) browser clients in order to provide advanced communication services. With ErizoAPI you can use easily erizo in your Node.js projects.

## Requirements

You must have built erizo in your computer in order to use erizoAPI.<br>
You can find erizo code at <a href="http://github.com/ging/erizo">http://github.com/ging/erizo</a>
 
## Building Instructions

1- Export the paths to get the source code and the library of erizo: 

    - Main path to erizo
      export ERIZO_HOME=path/to/erizo
    - Path to liberizo.so
      export LD\_LIBRARY\_PATH=path/to/erizo/build/erizo

2- Execute build.sh

##Usage

With erizo you can publish and subscribe to WebRTC streams.<br>
First get the erizoAPI module.   

<pre>
    var erizo = require('./path/to/erizoAPI/build/Release/addon');
</pre>

OneToManyProcessor is the muxer that gets the published stream and sends it to the subscribers. With more than one OneToManyProcessor you can make multiconference services.

<pre>
    var muxer = new erizo.OneToManyProcessor();        
</pre>

Also you need a WebRtcConnection for each participant (publisher or subscriber). 

<pre>
    var wrtc = new erizo.WebRtcConnection();
</pre>

Start the ICE negotiation and check periodically its state (the negotiation is asynchronous). When candidates are gathered (state > 0) set the client SDP.

<pre>
    wrtc.init();
                           
    var intervarId = setInterval(function() {

        var state = wrtc.getCurrentState();

        if(state > 0) {

            wrtc.setRemoteSdp(remoteSdp);
   
            var localSdp = wrtc.getLocalSdp();

            //Return localSdp to the client
    
            clearInterval(intervarId);
        }

    }, 100);
</pre>

Finally return the local SDP to the client and the connection is stablished.

Note that the way to interchange the SDPs between the client and erizo is out of the scope of this project. 


###Publishing a stream

To publish a stream add the publisher WebRtcConnection to the OneToManyProcessor

<pre>
    wrtc.setAudioReceiver(muxer);
    wrtc.setVideoReceiver(muxer);
    muxer.setPublisher(wrtc);
</pre>
  


###Subscribing to a stream

To subscribe to a stream add a subscriber WebRtcConnection to the OneToManyProcessor. OneToManyProcessor needs an unique peer identifier for each subscriber.

<pre>
    muxer.addSubscriber(wrtc, peerId);
</pre> 

In order to start correctly the communication send a FIR packet to the publisher. Make sure the ICE state is READY (wrtc.getCurrentState() > 2)

<pre>
    muxer.sendFIR();                    
</pre> 

Also you can remove a determined subscriber from the muxer

<pre>
    muxer.removeSubscriber(peerId);                    
</pre> 


## License

The MIT License

Copyright (C) 2012 Universidad Politecnica de Madrid.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
