# Overview

You will use this API to handle connections to rooms and streams in your web applications.

This API is designed to be executed in the browsers of your users, so it is provided as a JavaScript file you can reference in your web applications.

Typical usage consists of: connection to the desired room, using the token retreived in the backend (explained at Server API), management of local audio and video, client event handling, and so on.

Here you can see the main classes of the library:

| Class                   | Description                                                                     |
|-------------------------|---------------------------------------------------------------------------------|
| [Erizo.Stream](#stream) | It represents a generic Event in the library, which is inherited by the others. |
| [Erizo.Room](#room)     | It represents an events related to Room connection.                             |
| [Erizo.Events](#events) | It represents an event related to streams within a room.                        |

# Stream

It will handle the WebRTC (audio, video and/or data) stream, identify it, and where it should be drawn.

They could be local or remote. Licode internally creates remote streams through the Erizo.Room APIs.

It will identify each local and remote streams with `stream.getID()` and it controls where it should be drawn in the HTML page according to `stream.play()` (just in case they are video/audio streams).

You need to initialize the Stream before you can use it in the Room. Typical initializacion of Stream is:

```
var stream = Erizo.Stream({audio:true, video:false, data: true, attributes: {name:'myStream'}});
```

`audio`, `video` and `data` variables has to be set to true if you want to publish them in the room. While `attributes` variable can store additional information related to the stream. This information will be shared between clients in the room.

If you want to use a stream to share your screen you should initialize it this way:

```
var stream = Erizo.Stream({screen: true, data: true, attributes: {name:'myStream'}});
```

Note that, if you use a Stream this way, the client that will share its sreen must access to your web app using a secure connection (with https protocol).

The Stream API is currently using the MediaDevices.getDisplayMedia() method. The MediaDevices.getUserMedia() method can be still used for sharing the screen by specifying an extensionId or desktopStreamId. Instructions for developing Chrome extensions can be found <a href="http://lynckia.com/licode/plugin.html" target="_blank">here</a>. Additionally, in Chrome, you can use your own extension outside of Licode and directly pass the `chromeMediaSourceId` as a parameter:

```
var stream = Erizo.Stream({screen: true, data: true, attributes: {name:'myStream'}, desktopStreamId:'ID_PROVIDED_BY_YOUR_EXTENSION'});
```

You can also specify some constraints about the video size when creating a stream. In order to do this you need to include a `videoSize` parameter that is an array with the following format: `[minWidth, minHeight, maxWidth, maxHeight]`

```
var stream = Erizo.Stream({video: true, audio: true, videoSize: [320, 240, 640, 480]});
```

Also, you can create a stream to publish an external source to the Licode session. At this point, RTSP and files are supported, depending on the Codec (only H.264 for RTSP and VP8 + (OPUS or PCMU8) for files. You can create an external stream by using the `url` variable. These streams can be then published just as if they were a local stream.

For instance, to create a stream from a rtsp source:

```
var stream = Erizo.Stream({video: true, audio: false, url:"rtsp://user:pass@the_server_url:port"});
```

Or, to create a stream from a .mkv file:

```
var stream = Erizo.Stream({video: true, audio: false, url:"file:///path_to_file/movie.mkv"});
```

You can access the next variables in a stream object:

* `stream.showing` to check whether the stream is locally playing our video/audio.
* `stream.room` to know with which room it is connected.
* `stream.local` to know if the given stream is local or remote.

In the next table we can see the functions of this class:

| Function                                                                         | Description                                                             |
|----------------------------------------------------------------------------------|-------------------------------------------------------------------------|
| [hasAudio()](#check-if-the-stream-has-audio-video-andor-data-active)             | Indicates if the stream has audio activated.                            |
| [hasVideo()](#check-if-the-stream-has-audio-video-andor-data-active)             | Indicates if the stream has video activated.                            |
| [hasData()](#check-if-the-stream-has-audio-video-andor-data-active)              | Indicates if the stream has data activated.                             |
| [init()](#initialize-the-stream)                                                 | Initializes the local stream.                                           |
| [close()](#close-a-local-stream)                                                 | Closes the local stream.                                                |
| [play(elementID, options)](#play-a-local-stream-in-the-html)                     | Draws the video or starts playing the audio in the HTML.                |
| [stop()](#remove-the-local-stream-from-the-html)                                 | Removes the video from the HTML.                                        |
| [muteAudio(isMuted, callback)](#mute-the-audio-or-video-track-of-a-remote-stream)| Mutes the audio track of a remote stream.                               |
| [muteVideo(isMuted, callback)](#mute-the-audio-or-video-track-of-a-remote-stream)| Mutes the video track of a remote stream.                               |
| [sendData(msg)](#send-data-through-the-stream)                                   | It sends data through the Stream to clients that are subscribed.        |
| [getAttributes()](#get-the-attributes-object)                                    | Gets the attributes variable stored when you created the stream.        |
| [setAttributes()](#set-the-attributes-object)                                    | It sets new attributes to the local stream that are spread to the room. |
| [getVideoFrame()](#set-the-attributes-object)                                    | It gets a Bitmap from the video.                                        |
| [getVideoFrameURL()](#get-the-url-of-a-frame-from-the-video)                     | It gets the URL of a Bitmap from the video.                             |
| [updateConfiguration(config, callback)](#update-the-spec-of-a-stream)            | Updates the spec of a stream.                                           |
| [updateSimulcastLayersBitrate(config)](#update-simulcast-layers-bitrate)         | Updates the bitrates for each simulcast layer.                          |
| [updateSimulcastActiveLayers(config)](#update-simulcast-active-layers)           | Updates if each simulcast layer is active.                              |

## Check if the stream has audio, video and/or data active

You can check if the stream has audio, video and/or data activated by these functions.

<example>
The function in line 1 checks the audio while the function in line 2 checks the video and the function in line 3 checks the data.
</example>

```
stream.hasAudio();
stream.hasVideo();
stream.hasData();
```

## Initialize the Stream

It initializes the stream and tries to retrieve a stream from local video and audio (in case you've activated them).

You need to call this method before you can publish the stream in the room or play it locally.

<example>
When you first create the stream, you need to initialize it (line 1). Then you need to add an event listener to know when the user has granted access to the webcam and/or microphone (line 2). You will receive an access-accepted or and access-denied event.
</example>

```
stream.init();
stream.addEventListener('access-accepted', function(event) {
  console.log("Access to webcam and/or microphone granted");
});
stream.addEventListener('access-denied', function(event) {
  console.log("Access to webcam and/or microphone rejected");
});
```

## Close a local stream

It closes a local stream.

If the stream has audio and/or video this method also stops the capture of your camera and microphone.

If the stream is published in a room this method also unpublish it.

If the stream is playing in a div this method also stops it.

<example>
You can close a local stream previously initialized with this code.
</example>

```
stream.close();
```

## Play a local stream in the HTML

It starts playing the video/audio at once at the given `elementID`. It automatically creates an audio/video HTML tag.

<example>
You can play the video/audio only when you have already received the access-accepted event. And you need to pass the `elementID` as argument. You can optionally hide the volume slider with the `options` argument.
</example>

```
stream.play(elementID, {speaker: false});
```

<example>
By default video streams are automatically adapted to the div that contains them. But it sometimes implies that the video will be croped. If you don't want to crop the video you can do like this:
</example>

```
stream.play(elementID, {crop: false});
```

## Remove the local stream from the HTML

It stops playing the video/audio.

<example>
You can stop to play the video/audio in the HTML with this code.
</example>

```
stream.stop();
```

## Mute the audio or video track of a remote stream

It stops receiving audio/video from a remote stream. The publisher of the stream will keep sending audio/video information to Licode but this particular subscriber won't receive it.

If we call it in the publisher's side, Licode will stop sending audio/video data to all its subscribers.

**Note:** it won't work on local streams, in p2p rooms or when the stream does not have an audio track.

<example>
You can mute the audio track (and stop receiving audio bytes) from the remote stream.
</example>

```
stream.muteAudio(true, function (result) {
  if (result === 'error') {
    console.log("There was an error muting the steram")
  }
});
```

<example>
You can also unmute the audio track.
</example>

```
stream.muteAudio(false, function (result) {
  if (result === 'error') {
    console.log("There was an error unmuting the steram")
  }
});
```

Besides, you could also mute/unmute the video track by calling `muteVideo()` with the same parameters.

## Send Data through the stream

It sends a message through this stream. All users that are already subscribed to it will received the message.

<example>
The client can send any JSON serializable message through this function, and it will be received through a `stream-data` event sent from the stream. So all clients should be previously subscribed to this stream.
</example>

```
stream.sendData({text:'Hello', timestamp:12321312});
```

## Get the attributes object

Gets the attributes object that the user stored or updated in the stream.

<example>
The attributes variable is usually a JSON object.
</example>

```
var stream = Erizo.Stream({audio:true, video:false, data: true, attributes: {name:'myStream', type:'public'}});

var attributes = stream.getAttributes();
if(attributes.type === 'public') {
  console.log(attributes.name);
}
stream.addEventListener("stream-attributes-update", function(evt) {
  var stream = evt.stream;
  // Handle stream attribute event.
});
```

## Set the attributes object

Updates attributes object in the stream. It can only be used in local streams and the changes will be propagated to the users who are subscribed to such stream. Other users will be notified through a special event called stream-attributes-update.

<example>
The attributes variable is usually a JSON object.
</example>

```
var stream = Erizo.Stream({audio:true, video:false, data: true, attributes: {name:'myStream', type:'public'}});

var attributes = stream.setAttributes({name: 'myStreamUpdated', type: 'private'});
```

## Get a frame from the video

It gets an image frame from the video.

<example>
The client can get the bitmap raw data from the video streams. The Stream returns the Bitmap data. You can put it in a canvas element. If you want to get the image periodically you can set an interval.
</example>

```
var bitmap;
var canvas = document.createElement('canvas');
var context = canvas.getContext('2d');

canvas.id = "testCanvas";
document.body.appendChild(canvas);

setInterval(function() {

  bitmap = stream.getVideoFrame();

  canvas.width = bitmap.width;
  canvas.height = bitmap.height;

  context.putImageData(bitmap, 0, 0);

}, 100);
```

## Get the URL of a frame from the video

It gets the URL of an image frame from the video.

## Update the spec of a stream

It updates the audio and video maximum bandwidth for a publisher or a subscriber (it will affect other subscribers when Simulcast is not used).

It can also be used in remote streams to toggle `slideShowMode`

<example>
You can update the maximum bandwidth of video and audio. These values are defined in the object passed to the function. You can also pass a callback function to get the final result.
</example>

```
var config = {maxVideoBW: 300, maxAudioBW: 300};

localstream.updateConfiguration(config, function(result) {
  console.log(result);
});

// We can update options also on a remote stream

remoteStream.updateConfiguration({slideShowMode:true}, function (result){
console.log(result);
});
```

## Update Simulcast Layers Bitrate

It allows us to change the max bitrate assigned for each spatial layer in Simulcast. It can only be applied to publishers.
```
localStream.updateSimulcastLayersBitrate({0: 80000, 1: 430000});
```

In this example we are configuring 2 spatial layers bitrates, limiting the lower layer to 80 Kbps and the higher to 430 Kbps.

## Update Simulcast Active Layers

We can decide which Simulcast layers are active. It can only be applied to publishers.
```
localStream.updateSimulcastActiveLayers({0: true, 1: false});
```

In this example we are disabling layer 1 while the other layer (0) is active.

# Room

It represents a Licode Room. It will handle the connection, local stream publication and remote stream subscription.

Typical Room initialization would be:

```
var room = Erizo.Room({token:'213h8012hwduahd-321ueiwqewq'});
```

It will create the room object by passing the token this users have previously received from your service. This token is has to be retreived using the [Server API](/server_api), because it is a user access token. But you need to call to the connect function we will see later in order to connect to the room.

You can access some variables like:

- `room.localStreams` to retrieve the current list of local streams available in the room.
- `room.remoteStreams` to retrieve the current list of remote streams available in the room.
- `room.roomID` to know the identifier of this room.
- `room.state` to access the current state of the room. States can be 0 if it is disconnected, 1 if it is connecting, and 2 if it is connected.

In the next table we can see the functions of this class:

| Function                               | Description                                                                                         |
|----------------------------------------|-----------------------------------------------------------------------------------------------------|
| [connect()](#open-a-connection-to-room)                  | It stablishes a connection to the room.                                           |
| [publish(stream)](#publish-the-local-stream)             | It publishes the `stream`.                                                        |
| [subscribe(stream)](#subscribe-to-a-remote-stream)       | It subscribes to a remote `stream`.                                               |
| [unsubscribe(stream)](#unsubscribe-from-a-remote-stream) | It unsubscribes from the `stream`.                                                |
| [unpublish(stream)](#unpublish-a-local-stream)           | It unpublishes the local `stream`.                                                |
| [disconnect()](#disconnect-from-room)                    | It disconnects from the `room`.                                                   |
| [startRecording(stream)](#start-recording)               | Starts recording the `stream`.                                                    |
| [stopRecording(recordingId)](#stop-recording)            | Stops a recording identified by its `recordingId`.                                |
| [getStreamsByAttribute(name, value)](#get-streams-by-attribute) | It returns a list of the remote streams that have the attribute specified by name - value strings.  |


## Open a connection to Room

It establishes a connection to the room. This function is asynchronous so we need to add an event listener to know when we are finally connected to the room. It will throw a room-connected event when it occurs.

<example>
The `room` has to be previously created with a valid token. Then we connect to the room (line 1), and we add an event listener to wait for the connection to be established (line 2).
Please, take into account that you can't do anything else with the room until you have received this event!!.
</example>

```
room.connect();
room.addEventListener("room-connected", function(event) {
  console.log("Connected!");
});
```

## Publish the local stream

It publishes the local stream given by an argument called `stream`.

Your client should be connected to the `room` and the stream should be first initialized.

When you call this function it starts to send video or audio to your Licode Room. Once the stream is ready all clients in the room will receive a stream-added event with the information about your stream and they could subscribe to it.

<example>
For publishing you first need to create and initialize a stream and connect to the room. Then you give the stream to the room in order to publish the streams it handles (line 1).
It throws a stream-added [Event](#events) when the stream is finally published in the room, beacuse it will be added. So you have to add and event listener to handle it (line 2), and then check if the stream is the one you have just published (line 3).
</example>

```
room.publish(localStream);
room.addEventListener("stream-added", function(event) {
  if (localStream.getID() === event.stream.getID()) {
    console.log("Published!!!");
  }
});
```

`room.publish` also allows to set a bandwidth limit for the localStream. We do this by passing the `maxVideoBW` variable as an option. The BW is expressed in Kbps. Keep in mind that the field `config.erizoController.maxVideoBW` in the server configuration has higher priority than this one and that `config.erizoController.defaultVideoBW` will be used by default if this option is not passed.

```
room.publish(localStream, {maxVideoBW:300});
```

We can also force the client to use a TURN server when publishing by setting the next parameter:

```
room.publish(localStream, {forceTurn: true});
```

There are two options that allow advance control of video bitrate in Chrome:
- `startVideoBW`: Configures Chrome to start sending video at the specified bitrate instead of the default one.
- `hardMinVideoBW`: Configures a hard limit for the minimum video bitrate.

```
room.publish(localStream, {startVideoBW: 1000, hardMinVideoBW:500});
```

In `room.publish` you can include a callback with two parameters, `id` and `error`. If the stream has been published, `id` contains the id of that stream. On the other hand, if there has been any kind of error, `id` is `undefined` and the error is described in `error`.

<example>
Using the callback to catch possible problems
</example>

```
room.publish(localStream, {maxVideoBW:300}, function(id, error){
  if (id === undefined){
    console.log("Error publishing stream", error);
  } else {
    console.log("Published stream", id);
  }
});
```

### Publish Simulcast Streams

You can enable Simulcast in the publisher by adding the next option:

```
room.publish(localStream, {simulcast: {numSpatialLayers: 2}});
```

Being `numSpatialLayers` the max number of spatial layers the publisher will send.

You can also limit the bitrate for each layer:

```
room.publish(localStream, {simulcast: {numSpatialLayers: 2, spatialLayerBitrates: {0: 80000, 1: 430000}}});
```

In that example we are configuring 2 spatial layers, limiting the lower layer to 80 Kbps and the higher to 430 Kbps.

**Note:** Simulcast only works with Google Chrome and it's not compatible with Licode's recording yet.

## Subscribe to a remote stream

It subscribes to a remote stream in the room.

For subscribing to a stream you first need to connect to the room and know which streams are available in the room. So as we have already seen we first need to add an event listener to receive stream-added events.

In this case you will need an object of the [Stream](#stream) class, that we receive with that event.

<example>
You first need to connect to a room, and receive an event with the stream you want to subscribe.
NOTE: You can subscribe to the stream you are publishing, but you will receive it with delay. We recommend you to use the [Stream](#stream) API to play directly your local streams.
</example>

```
room.addEventListener("stream-subscribed", function(streamEvt) {
  console.log("Stream subscribed");
});
room.subscribe(stream);
```

You can choose which components (audio/video) of the stream you want to subscribe to using the second parameter of `subscribe` method.

<example>
Here we are going to subscribe to the audio but not to the video.
</example>

```
room.addEventListener("stream-subscribed", function(streamEvt) {
  console.log("Stream subscribed");
});
room.subscribe(stream, {audio: true, video: false});
```

`room.subscribe` also allows to set a bandwidth limit for a subscription. We do this by passing the `maxVideoBW` variable as an option. The BW is expressed in Kbps. Keep in mind that the field `config.erizoController.maxVideoBW` in the server configuration has higher priority than this one and that `config.erizoController.defaultVideoBW` will be used by default if this option is not passed. More importantly, if the publisher is not using simulcast, the lowest `maxVideoBW` set to a subscriber will limit the max bandwidth used by that particular stream for the publisher and all the subscribers.

```
room.subscribe(stream, {maxVideoBW:300});
```

We can also force the client to use a TURN server when subscribing by setting the next parameter:

```
room.subscribe(localStream, {forceTurn: true});
```

In `room.subscribe` you can include a callback with two parameters, `result` and `error`. If the stream has been subscribed, `result` is true. On the other hand, if there has been any kind of error, `result` is `undefined` and the error is described in `error`.

<example>
Using the callback to catch possible problems
</example>

```
room.subscribe(stream, {audio: true, video: false}, function(result, error){
  if (result === undefined){
    console.log("Error subscribing to stream", error);
  } else {
    console.log("Stream subscribed!");
  }
});
```

When subscribing you can also configure the receiver to receive a lower bitrate Video stream from the publisher. We call this feature slideShowMode. By enabling it, this subscriber will receive one frame every two seconds from the publisher and won't affect the quality of other subscribers.

```
room.subscribe(stream, {audio: true, video: true, slideShowMode:true}, function(result, error){
  if (result === undefined){
    console.log("Error subscribing to stream", error);
  } else {
    console.log("Stream subscribed!");
  }
});
```

`SlideShowMode` can also be toggled on or off using `stream.updateConfiguration`. Keep in mind this will only work on remote streams (subscriptions).

When we enable Simulcast it is also interesting to specify whether the subscriber should not receive a resolution or frame rate beyond a maximum. We
can configure it like in the example below:

```
room.subscribe(stream, {video: {height: {max: 480}, width: {max: 640}, frameRate: {max:20}}}, function(result, error){
  if (result === undefined){
    console.log("Error subscribing to stream", error);
  } else {
    console.log("Stream subscribed!");
  }
});
```

It would help us not wasting CPU or bandwidth if, for instance, we will not render videos in a &lt;video&gt; element bigger than 640x480.

## Unsubscribe from a remote stream

You can unsubscribe from a stream you are currently subscribed.

Here apply the same requirements seen in the subscription.

<example>
You first need to be subscribed to the stream. When you unsubscribe from a stream the HTML element will be empty.
</example>

```
room.unsubscribe(stream);
```

In `room.unsubscribe` you can include a callback with two parameters, `result` and `error`. If the stream has been unsubscribed, `result` is true. On the other hand, if there has been any kind of error, `result` is `undefined` and the error is described in `error`.

<example>
Using the callback to catch possible problems
</example>

```
room.unsubscribe(stream, function(result, error){
  if (result === undefined){
    console.log("Error unsubscribing", error);
  } else {
    console.log("Stream unsubscribed!");
  }
});
```

We can listen to an event called `stream-unsubscribed` that is triggered when the unsubscription is completed (the stream no longer lives in the client nor the server). This event is interesting in cases where we might subscribe again to the stream, to avoid having conflicts or setting artificial delays to wait for resubscriptions.

## Unpublish a local stream

You can unpublish your stream given in `stream` when your are connected to a room and you are currently publishing it.

Here apply the same requirements seen in the publish function.

The room will throw a stream-removed event to all the users when your streams are not published.

<example>
You first need to be publishing the local stream. Then you can unpublish your streams at any time (line 1). You can be sure your streams are not published when you receive a stream-removed event (lines 2-3).
</example>

```
room.unpublish(localStream);
room.addEventListener("stream-removed", function(event) {
  if (localStream.getID() === event.stream.getID()) {
    console.log("Unpublished!!!");
  }
});
```

In `room.unpublish` you can include a callback with two parameters, `result` and `error`. If the stream has been unpublished, `result` is true. On the other hand, if there has been any kind of error, `result` is `undefined` and the error is described in `error`.

<example>
Using the callback to catch possible problems
</example>

```
room.unpublish(localStream, function(result, error){
  if (result === undefined){
    console.log("Error unpublishing", error);
  } else {
    console.log("Stream unpublished!");
  }
});
```

## Disconnect from Room

You can disconnect from the room when you want.

The room will throw a room-disconnected event.

<example>
You can disconnect from the room whenever you are previously connected.
</example>

```
room.disconnect();
```

## Managing Quality Adaptation

By default, Licode will try to adapt the video quality of each publisher to the worst subscriber, that way, we ensure that all participants can partake in the conference. However, that means that video quality can be degraded for all the participants in a room if just one of them does not have enough bandwidth available.

To solve this, Licode gives you a flexible way to configure the behaviour of the adaptation per publisher. You specify a `minVideoBW` when you publish a stream. This will set the minimum video bitrate, you can use this when you want to control the minimum video quality for a given stream. On the subscriber, an estimate of the available bandwidth is calculated for a particular stream. Licode will react to a subscriber that is under the `minVideoBW` set by the publisher, based on the specified [adaptation scheme](#schemes). If no `scheme` is specified, Licode will only notify subscribers via a `streamEvent`.

```
room.publish(localStream, {maxVideoBW:2000, minVideoBW: 1000});
```

### Schemes

An adaptation scheme is a behaviour patter that Licode will use on the subscribers that report having less available bandwidth than what was configured via `minVideoBW`.

Currently Licode implements three different adaptations schemes, one built on top of the other.

- `notify`: Licode will only notify periodically subscribers that are below `minVideoBW`. The possible messages, included in `streamEvent.msg` are:
	- *insufficient*: Indicates the subscriber is not reporting enough bandwidth

- `notify-break`: Licode will notify subscribers, stop adapting the publisher to problematic subscribers and relegate them to audio-only mode. The possible messages, included in `streamEvent.msg` are:

	- *insufficient*: Indicates the subscriber is not reporting enough bandwidth
	- *audio-only*: Licode will now send only audio to this subscriber and the publisher will not try to adapt Video quality.

- `notify-break-recover`: Same as notify-break but Licode will periodically try to recover the subscriber's video.The possible messages, included in `streamEvent.msg` are:
	- *insufficient*: Indicates the subscriber is not reporting enough bandwidth, Licode will stop sending Video to this subscriber and the publisher won't try to adapt.
	- *recovered*: This stream has successfully recovered, reports more that minVideoBW and Licode will treat it as a stream with no bandwidth problems
	- *audio-only*: Licode will stop trying to recover and send only audio to this subscriber and the publisher will not try to adapt Video quality.

You will have to set your desired scheme per publisher:

```
room.publish(localStream, {maxVideoBW:2000, minVideoBW: 1000 scheme:"notify-break"});
```

The default scheme is `notify`.

Licode uses `streamEvents` to notify clients, so you will need to listen to the following events to properly take advantage of the schemes in your application. These events are:

- `notify`: Licode will only notify periodically subscribers that are below `minVideoBW`.
- `notify-break`: Licode will notify subscribers, stop adapting the publisher to problematic subscribers and relegate them to audio-only mode.
- `notify-break-recover`: Same as notify-break but Licode will periodically try to recover the subscriber's video.

Keep in mind you can use this in combination with `SlideShowMode` allowing for a wide variety of configurations.

## Start Recording

Start the recording of a stream in the server in the path specified in licode_config.js

The recording will stored in a .mkv file using VP8 codec for video and PCMU or OPUS for audio, depending on the server configuration. This file can be played directly or streamed into a Licode room.

Licode will keep recording until stopRecording is called or the stream is removed from the room.

<example>
Start the recording of the local stream.
</example>

```
room.startRecording(localStream, function(recordingId, error) {
  if (recordingId === undefined){
    console.log("Error", error);
  } else {
    console.log("Recording started, the id of the recording is ", recordingId);
  }
});
```

## Stop Recording

Stop the recording of a stream. You must provide the identificator of the recording: `recordingId`.

<example>
Stop the recording.
</example>

```
room.stopRecording(recordingId, function(result, error){
  if (result === undefined){
    console.log("Error", error);
  } else {
    console.log("Stopped recording!");
  }
});
```

## Playing recorded streams

There are two ways of playing a recorded stream. Both of them involve the creation of an external stream. You can either set the `url` variable to the path of the recorded file or use the `recordingId` as a variable.

<example>
Play a recording from full path (url) or by using its `recordingId`.
</example>

```
var stream = Erizo.Stream({video: true, audio: false, url:"file:///path_to_file/previousRecording.mkv"});
room.publish(stream);
var stream2 = Erizo.Stream({audio:true, video:true, recording: 'asda2131231'});
room.publish(stream2);
```

## Get Streams by attribute

You can search remote streams by attribute. A remote stream is a room stream you have previously subscribed to.

<example>
If you want to get the streams of type 'public':
</example>

```
var streams = room.getStreamsByAttribute('type', 'public');
// streams is an array that contains the stream objects.
```
## Event Handling

Room inherits EventDispatcher (see [events](#events) for further information), for handling RoomEvents and StreamEvents. For example:

- Event 'room-connected' points out that the user has been successfully connected to the room.
- Event 'room-error' indicates that there has been an error and it hasn't been possible to connect to the room.
- Event 'room-disconnected' shows that the user has been already disconnected.
- Event 'stream-added' indicates that there is a new stream available in the room.
- Event 'stream-removed' shows that a previous available stream has been removed from the room.

The client could receive any event when it is connected.

<example>
Despite we have shown examples of event listeners during the documentation, we strongly recommend to add event listeners before you do any action in the room. The previous examples only illustrated the behaviour of Room's functions.
</example>

```
var room = Room({token:"..."});
room.addEventListener("room-connected", function(evt){...});
room.addEventListener("room-error", function(evt){...});
room.addEventListener("room-disconnected", function(evt){...});
room.addEventListener("stream-added", function(evt){...});
room.addEventListener("stream-removed", function(evt){...});
room.connect();
```

# Events

Licode provides a set of event classes to handle changes during client connection.

[Room](#room) and [Stream](#stream) classes throw different Licode events (they are EventDispatchers) and it's always a good idea to listen to them. Here we will explain the different kinds of events in Licode. In the next table you can see a brief description.

| Class                               | Description                                                                                         |
|-------------------------------------|-----------------------------------------------------------------------------------------------------|
| [Licode Event](#licode-event)       | It represents a generic Event in the library, which is inherited by the others.                     |
| [Room Event](#room-event)           |	It represents an events related to Room connection.                                                 |
| [Stream Event](#stream-event)       | It represents an event related to streams within a room.                                            |

## Licode Event

It handles the type of event, that is important when adding event listeners to EventDispatchers and dispatching new events.

<example>
A LicodeEvent can be initialized like this, but it is usually created by Client API.
</example>

```
var event = Erizo.LicodeEvent({type: "room-connected"});
```

## Room Event

It represents connect and disconnect events.

You can access the list of streams connected to the room by accessing `roomEvent.streams`.

These are the type of Room events thrown:

- room-connected: points out that the user has been successfully connected to the room. This message also provides the list of streams that are currently published in the room.
- room-error: indicates that it hasn't been possible a succesufully connection to the room.
- room-disconnected: shows that the user has been already disconnected.

They are all dispatched by Room objects

<example>
A RoomEvent can be initialized like this, but it is usually created by Room objects.
</example>

```
var roomEvent = Erizo.RoomEvent({type:"room-connected", streams:[stream1, stream2]});
```

<example>
They all are dispatched by Room objects, so you need to add event listeners to them.
</example>
```
room.addEventListener("room-connected", function(evt){...});
```
## Stream Event

It represents an event related to a stream.

You can access the related stream by `streamEvent.stream`.

Some of them have a more detailed message in `streamEvent.msg`.

For `stream-failed` events there is another field `streamEvent.origin` to give us more info about the reason of the failure.

There are the different types of Stream events:

- *access-accepted*: indicates that the user has accepted to share his camera and microphone.
- *access-denied*: indicates that the user has denied to share his camera and microphone.
- *stream-added*: indicates that there is a new stream available in the room.
- *stream-removed*: shows that a previous available stream has been removed from the room.
- *stream-data*: thrown by the stream it indicates new data received in the stream.
- *stream-attributes-update*: notifies when the owner of the given stream updates its attributes
- *bandwidth-alert*: thrown when a subscriber stream is reporting less than the `minVideoBW` specified in the publisher. The event has three parts:
	- `streamEvent.stream` is the problematic subscribe stream.
	- `streamEvent.bandwidth` is the available bandwidth reported by that stream.
	- `streamEvent.msg` the status of that stream, depends on the adaptation [scheme](#schemes).
- *stream-failed*: A stream has failed, either in the connection establishment or during the communication.
- *stream-ended*: A track of the stream (specified in the msg. es.audio/video) is ended, probably caused by an hardware disconnection. Emitted only once

They all are dispatched by Room objects.

<example>
A StreamEvent can be initialized like this, but it is usually created by Client API.
</example>

```
var streamEvent = Erizo.StreamEvent({type:"stream-added", stream:stream1});
```

<example>
`stream-added`, `stream-removed` and `stream-failed` are dispatched by Room objects, so you need to add event listeners to them.
</example>

```
room.addEventListener("stream-removed", function(evt){...});
```

<example>
`access-accepted`, `access-denied`, `stream-data`, `stream-attributes-update` and `bandwidth-alert` are dispatched by Stream objects, so you need to add event listeners to them.
</example>

```
stream.addEventListener("access-accepted", function(evt){...});

stream.addEventListener("stream-data", function(evt){
  console.log('Received data ', evt.msg, 'from stream ', evt.stream.getAttributes().name);
});

room.addEventListener("stream-attributes-update", function(evt){...});
```

## Connection Event

It represents an event related to an internal connection.

There is currently only one possible event type called `connection-failed` that is triggered when the signaling negotiation inside a connection times out.

They are dispatched by Room objects.

<example>
A ConnectionEvent can be initialized like this, but it is usually created by Client API.
</example>

```
var connectionEvent = Erizo.ConnectionEvent({type:"connection-failed"});
```

<example>
`connection-failed` is dispatched by Room objects, so you need to add event listeners to them.
</example>

```
room.addEventListener("connection-failed", function(evt) {...});
```

# Examples

Here we show some examples and code snippets for typical use cases in this API.

In this section we will see a set of examples to use almost every part of this API.

<example>
In this example we will make a basic videoconference application. Every client than connects to the application will publish his video and audio and will subscribe to all the videos and audios of the other clients. This example uses an unique room.
</example>

```
<html>
  <head>
    <title>Licode Basic Example</title>
    <script type="text/javascript" src="erizo.js"></script>
    <script type="text/javascript">

      window.onload = function () {

          var localStream = Erizo.Stream({audio: true, video: true, data: true});
          var room = Erizo.Room({token: "af54/=gopknosdvmgiufhgadf=="});

          localStream.addEventListener("access-accepted", function () {

              var subscribeToStreams = function (streams) {
                  for (var index in streams) {
                    var stream = streams[index];
                    if (localStream.getID() !== stream.getID()) {
                        room.subscribe(stream);
                    }
                  }
              };

              room.addEventListener("room-connected", function (roomEvent) {

                  room.publish(localStream);
                  subscribeToStreams(roomEvent.streams);
              });

              room.addEventListener("stream-subscribed", function(streamEvent) {
                  var stream = streamEvent.stream;
                  var div = document.createElement('div');
                  div.setAttribute("style", "width: 320px; height: 240px;");
                  div.setAttribute("id", "test" + stream.getID());

                  document.body.appendChild(div);
                  stream.play("test" + stream.getID());
              });

              room.addEventListener("stream-added", function (streamEvent) {
                  var streams = [];
                  streams.push(streamEvent.stream);
                  subscribeToStreams(streams);
              });

              room.addEventListener("stream-removed", function (streamEvent) {
                  // Remove stream from DOM
                  var stream = streamEvent.stream;
                  if (stream.elementID !== undefined) {
                      var element = document.getElementById(stream.elementID);
                      document.body.removeChild(element);
                  }
              });

              room.connect();
              localStream.play("myVideo");
          });
          localStream.init();
      };
    </script>
  </head>

  <body>
    <div id="myVideo" style="width:320px; height: 240px;">
    </div>
  </body>
</html>
```
# Customize Logging

You can configure and customize the way ErizoClient:

| Function                               | Description                                                                                         |
|----------------------------------------|-----------------------------------------------------------------------------------------------------|
| `L.Logger.setLogLevel(LogLevel)`         | Sets the log level from 0 (DEBUG) to 5(NONE) with decreasing level of detail               |
| `L.Logger.setLogPrefix(logPrefix)`       | You can pass a string as a prefix for every log ErizoClient log message              |
| `L.Logger.setOutputFunction(outputFunction)`      | You can pass a function that takes an array forming the log message to further customize the output|

# Node.js Client

Running erizo clients in your node.js applications.

You can also run erizo clients in your node.js applications with the same API explained here. You can connect to rooms, publish and subscribe to streams and manage events. You need only to import the node module **erizofc.js**. This adds a new dependency that you will need to install: ` npm install socket.io-client`

```
var newIo = require('socket.io-client');
var Erizo = require('.erizofc');
```

The lines to initialize a room and a stream change slightly. So, once you have a token:
```
var room = Erizo.Room(newIo, undefined, undefined, {token:'theToken'});
```

And for creating a Stream:

```
var stream = Erizo.Stream(undefined, {stream: {audio:true, video:true, data: true}});
```

The parameters set to `undefined` can be used for defining helper functions for getting the user media, the type of browser, etc. You can see examples of these helpers in *erizoClient* or *spine* code.

After instantiating a room and a stream you can use the API like explained for the browser case, calling `Erizo.Room`, `Erizo.Stream` and `Erizo.Events`. Note that you can not publish/subscribe streams with video and/or audio. We are working on this feature in order to develop another way of distribute video/audio streams.

You can also use Erizo Client Logger for managing log levels, etc.
```
Erizo.Logger.setLogLevel(Erizo.Logger.ERROR);
```
