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

Note that, if you use a Stream this way, the client that will share its sreen must access to your web app using a secure connection (with https protocol) and use a screensharing plugin as explained <a href="http://lynckia.com/licode/plugin.html" target="_blank">here</a>.

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

| Function                                                                | Description                                                             |
|-------------------------------------------------------------------------|-------------------------------------------------------------------------|
| [hasAudio()](#check-if-the-stream-has-audio-video-andor-data-active)    | Indicates if the stream has audio activated.                            |
| [hasVideo()](#check-if-the-stream-has-audio-video-andor-data-active)    | Indicates if the stream has video activated.                            |
| [hasData()](#check-if-the-stream-has-audio-video-andor-data-active)     | Indicates if the stream has data activated.                             |
| [init()](#initialize-the-stream)                                        | Initializes the local stream.                                           |
| [close()](#close-a-local-stream)                                        | Closes the local stream.                                                |
| [play(elementID, options)](#play-a-local-stream-in-the-html)            | Draws the video or starts playing the audio in the HTML.                |
| [stop()](#remove-the-local-stream-from-the-html)                        | Removes the video from the HTML.                                        |
| [muteAudio(isMuted, callback)](#mute-the-audio-track-of-a-remote-stream)| Mutes the audio track of a remote stream.                               |
| [sendData(msg)](#send-data-through-the-stream)                          | It sends data through the Stream to clients that are subscribed.        |
| [getAttributes()](#get-the-attributes-object)                           | Gets the attributes variable stored when you created the stream.        |
| [setAttributes()](#set-the-attributes-object)                           | It sets new attributes to the local stream that are spread to the room. |
| [getVideoFrame()](#set-the-attributes-object)                           | It gets a Bitmap from the video.                                        |
| [getVideoFrameURL()](#get-the-url-of-a-frame-from-the-video)            | It gets the URL of a Bitmap from the video.                             |
| [updateConfiguration(config, callback)](#get-the-url-of-a-frame-from-the-video) | Updates the spec of a stream.                                   |

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

## Mute the audio track of a remote stream

It stops receiving audio from a remote stream. The publisher of the stream will keep sending audio information to Licode but this particular subscriber won't receive it.

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

It updates the audio and video maximum bandwidth for a publisher.

It can also be used in remote streams to toggle `slideShowMode`

<example>
You can update the maximun bandwidth of video and audio. These values are defined in the object passed to the function. You can also pass a callback function to get the final result.
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

# Room

# Events

# Examples

# Node.js Client