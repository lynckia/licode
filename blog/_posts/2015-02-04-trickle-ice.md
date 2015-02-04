--- 
layout: post 
title: New master branch, now with 100% more trickle 
--- 

We have just updated the master branch with the changes in trickle_ice.
The most obvious new feature is the support of [Trickle Ice](https://webrtchacks.com/trickle-ice/). It will make the delay until you start receiving or sending media via Licode a lot smaller.

However, this has a drawback, this new implementation modifies the protocol used to communicate erizoController with erizoClients.

Basically, if you modified erizoClient, erizoJSController or erizoController you will find you have to work a little to merge the new changes, we promise it is worth it :)

As always, we created a new [release/tag] (https://github.com/ging/licode/releases/tag/v1.1.0) to the previous version so you guys can find easily the exact commit we made the change. Furthermore, the [legacy branch](https://github.com/ging/licode/tree/legacy) also points to the previous state of master.

But enough about the past, let's summarize the new additions:

Main new features:

* Trickle ICE support
* Initial [Openwebrtc/Bowser](http://www.openwebrtc.io) support: There are still some things to improve but we'll get there soon
* Support for Ubuntu 14.04: Licode should now work with the latest LTS release out of the box! I'd advise to start moving to 14.04 ;)

Fixes:

* Fixes to Video/only audio/only streams in Firefox
* Fixes to erizoJS management 
* Fixed the infamous 'Cannot call method 'hasAudio' of undefined
* A lot of smaller fixes

We hope all of you switch to this branch as soon as possible and, as always, report your problems so we can keep fixing them.
Thanks to everyone that contributed with code or reports!
