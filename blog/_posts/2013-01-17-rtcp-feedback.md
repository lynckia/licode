--- 
layout: post 
title: Introducing RTCP feedback forwarding in Lynckia.  
--- 

One of the most important problems to be faced when working with real
time communications is how to react to varying network conditions. Even
if a bandwidth and general quality is agreed during the signalling or
negotiation phase, any change in one of the participants' connection
will degrade the communication. There are many studies and implemented
mechanisms to help solve this problem. These solutions are heavily
dependant on the protocols and technologies used in each application.

Lynckia lives in the WebRTC world and has to communicate with the
current implementations, being [Google Chrome](http://google.com/chrome) the
most important today. Chrome has been advancing in the implementation of
the [SAVPF RTP Profile](http://tools.ietf.org/html/rfc5124) for some
months that, among other things, defines feedback RTCP packets to adapt
the communication between the peers. 

Today we announce we have implemented the forwarding of RTCP packets via
Lynckia in the master branch. In our tests it has greatly improved the communication in
environments with high packet losses and latency. While this is only the
first step, it is very important for providing a high quality service.

We encourage all of you to test it and tell us what you think of it. Please remember to uninstall libsrtp
and use the one included in the repository now. All the scripts have
been fixed so installing everything again should work just fine.

We also have updated our [demo page](http://chotis2.dit.upm.es) and we
also encourage you to try it :)
