# Overview

You will use this API to handle connections to rooms and streams in your web applications.

This API is designed to be executed in the browsers of your users, so it is provided as a JavaScript file you can reference in your web applications.

Typical usage consists of: connection to the desired room, using the token retreived in the backend (explained at Server API), management of local audio and video, client event handling, and so on.

Here you can see the main classes of the library:

| Class        | Description                                                                     |
|--------------|---------------------------------------------------------------------------------|
| Erizo.Stream | It represents a generic Event in the library, which is inherited by the others. |
| Erizo.Room   | It represents an events related to Room connection.                             |
| Erizo.Events | It represents an event related to streams within a room.                        |

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

| Function                              | Description                                                             |
|---------------------------------------|-------------------------------------------------------------------------|
| hasAudio()                            | Indicates if the stream has audio activated.                            |
| hasVideo()                            | Indicates if the stream has video activated.                            |
| hasData()                             | Indicates if the stream has data activated.                             |
| init()                                | Initializes the local stream.                                           |
| close()                               | Closes the local stream.                                                |
| play(elementID, options)              | Draws the video or starts playing the audio in the HTML.                |
| stop()                                | Removes the video from the HTML.                                        |
| muteAudio(isMuted, callback)          | Mutes the audio track of a remote stream.                               |
| sendData(msg)                         | It sends data through the Stream to clients that are subscribed.        |
| getAttributes()                       | Gets the attributes variable stored when you created the stream.        |
| setAttributes()                       | It sets new attributes to the local stream that are spread to the room. |
| getVideoFrame()                       | It gets a Bitmap from the video.                                        |
| getVideoFrameURL()                    | It gets the URL of a Bitmap from the video.                             |
| updateConfiguration(config, callback) | Updates the spec of a stream.                                           |

## Check if the stream has audio, video and/or data active

## Initialize the Stream

## Close a local stream

## Play a local stream in the HTML

## Remove the local stream from the HTML

## Mute the audio track of a remote stream

## Send Data through the stream

## Get the attributes object

## Set the attributes object

## Get a frame from the video

## Get the URL of a frame from the video

## Update the spec of a stream





# Room

# Events

# Examples

# Node.js Client