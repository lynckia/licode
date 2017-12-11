# Single Peer Connection - Feature Review
Single Peer Connection means that we can send/receive all streams in the same PeerConnection, sharing UDP ports, ICE credentials, DTLS certificates, etc. It usually means that publishers/subscribers use a single UDP "connection", with some benefits: reduce the number of ports used by the clients and server and shorter times to publish/subscribe new streams.

![comparison](https://docs.google.com/drawings/d/1EMW43_6BX0mDJ2v4kLWrzDobz6dRjjqhctSkLM869io/pub?w=960&h=721)

## Architecture
Single Peer Connection involves changes in both client and server sides.

This new feature will be optional because we'd like to keep backwards compatibility with older systems and be sure we can disable it if some browser does not support it.

## API Changes
Client: All changes will be hidden inside code.

Server: The feature will be activated/deactivated via `licode_config.js`. The policies that control ecCloudHandler will be modified to allow more fine-grained control over the ErizoJS chosen for any given connection or client.

## Details
This features involves changes in:

- SDP: Full SDP will only be used in the Clients. We will transition to an asynchronous exchange of JSON messages between ErizoClient and ErizoJS. ErizoClient will be in charge of assembling the SDP and passing it to the PeerConnection. There will be a single SDP per connection, with information about all the streams that take part of it.

- Renegotiation: JSON messages could add/remove more streams in the SDP. A new media JSON message does not mean that we will negotiate a new ICE, or handshake new DTLS. It will only affect to the number of streams that are added/removed in the SDP. There will be several new scenarios for renegotiating with Offer/Answer requests:

  - When publishing or subscribing to the first stream, the client will create a Peer Connection, and ErizoClient or Erizo will send a message with the stream info and ICE candidates.

  - When a client wants to publish/subscribe to a new Stream, only media information will be exchanged.

  - When a client unpublish/unsubscribe from the last stream in the SDP, Erizo and the client will close the connection as we're doing now.

  - When the publisher wants to unpublish one of the Streams, Erizo will send a message with the stream removed from the list.

  - When a subscribed Stream is removed, Erizo will send a new message removing the info of the corresponding Stream.

Both Erizo Client and Erizo Controler might decide not to use Single PC for the next reasons:

  - The browser does not support single peer connection.

  - A policy in ErizoController decides that for scalability purposes it is preferable to use another ErizoJS and so, different Peer Connections.

In terms of message flow, current solution with Multiple Peer Connections is as follows:

![Multiple Peer Connections](http://g.gravizo.com/g?
@startuml;
actor User;
participant "Erizo Client" as Client;
participant "Erizo Controller" as EC;
User->Client: publish stream1;
Client->EC: publish stream1;
Client->EC: offer;
Client<-EC: answer;
Client<->EC: ICE Negotiation;
Client<->EC: DTLS Negotiation;
User->Client: subscribe stream2;
Client->EC: subscribe stream2;
Client<-EC: offer;
Client->EC: answer;
Client<->EC: ICE Negotiation;
Client<->EC: DTLS Negotiation;
User->Client: subscribe stream3;
Client->EC: subscribe stream3;
Client<-EC: offer;
Client->EC: answer;
Client<->EC: ICE Negotiation;
Client<->EC: DTLS Negotiation;
@enduml
)

And with the new solution with Single Peer Connection it will be like the next figure:

![Single Peer Connection](http://g.gravizo.com/g?
@startuml;
actor User;
participant "Erizo Client" as Client;
participant "Erizo Controller" as EC;
User->Client: publish stream1;
Client->EC: publish stream1;
Client<-EC: offer;
Client->EC: JSON data;
Client<->EC: ICE Negotiation;
Client<->EC: DTLS Negotiation;
User->Client: subscribe stream2;
Client->EC: subscribe stream2;
Client<-EC: Media Info;
Client->EC: answer;
User->Client: subscribe stream3;
Client->EC: subscribe stream3;
Client<-EC: Media Info;
Client->EC: answer;
@enduml
)

### How does it affect ErizoClient?
Streams will be added to existing PeerConnections, and will need to keep track of the existing PeerConnections to decide whenever a user wants to publish/subscribe to a new Stream if we need to create a new Peer Connection or update an existing one.

ErizoClient will be told to use Single PeerConnection or Multiple PeerConnections.

So, if ErizoClient detects that the browser supports Single PC it will send a publish/subscribe message to Erizo Controller indicating Single PC is supported.

It will also be in charge of receiving JSON messages with media or connection information from Erizo and assemble them into a SDP supported by the browser.
This includes absorbing differences in implementation or standards. For instance, there is Plan B (Chrome) and Unified Plan (standard, Firefox),

### How does it affect Nuve?
It does not affect Nuve.

### How does it affect Erizo Controller / Erizo Agent / Erizo JS?
We will create a new policy for ErizoController CloudHandler, to limit and customize the amount of ErizoJS processes that are used in a Room.
ErizoController will receive a publish/subscribe message with the Single PC support information. Based on the policy used, it will chose an ErizoJS for that stream and send it's id to the client. It will also have to tell ErizoJS to apply Single PC or Multiple PC solution, so the request to ErizoJS will be updated with the previous result.

It affects Erizo Agent because it needs to choose the same ErizoJS for streams in the same room, according to the policy.

It affects Erizo JS because there will be a new object called MediaStream, and we will add MediaStreams to WebRtcConnections (see next section) and OneToManyProcessors.

![Erizo Controller Logic](http://g.gravizo.com/g?
@startuml;
%28*%29 --> "Check Cloud Handler Policy";
  -> if "Can  we reuse a ErizoJS?" then;
  -->[true] ===SEND_SPC===;
else;
  -->[false] "Create another ErizoJS" as create_erizojs;
  --> ===SEND_SPC===;
endif;
if "Single PC request from client?" then;
  -->[true] "Send a single PC request to ErizoJS" as create_spc;
  --> if "Does ErizoJS contain a PC yet?" then;
    -down->[true] ===ADD_STREAM===;
  else;
    ->[false] "Create WebRtcConnection" as create_pc;
    --> ===ADD_STREAM===;
  endif;
  --> "Add stream to WebRtcConnection";
  --> "Send Message" as answer;
else;
  ->[false] "Send a multiple PC request to ErizoJS";
  --> create_pc;
endif;
@enduml
)


#### Cloud Handler Policy

There should be a new ErizoController Cloud Handler policy that takes into account these new requirements.

Any other similar solution should be considered when implementing this feature.

### How does it affect ErizoAPI / Erizo?
Much functionality inside WebRtcConnection will be moved to MediaStream. And WebRtcConnection will handle MediaStreams directly. The new architecture should be compatible with Single and Multiple PC.

Below I show a summary of the current architecture inside Erizo, with the main building blocks:

![Erizo Current Architecture](http://g.gravizo.com/g?
@startuml;
WebRtcConnection<--OneToManyProcessor;
DtlsTransport<--WebRtcConnection;
WebRtcConnection : -DtlsTransport rtp;
WebRtcConnection : -Worker worker;
WebRtcConnection : -Pipeline pipeline;
WebRtcConnection : +onPacketReceived%28%29;
OneToManyProcessor : +MediaSource publisher;
OneToManyProcessor : +MediaSink subscribers;
@enduml;
)

And here we can see the proposal to change them:

![Erizo Proposed Architecture](http://g.gravizo.com/g?
@startuml;
MediaStream<--WebRtcConnection;
MediaStream<--OneToManyProcessor;
DtlsTransport<--WebRtcConnection;
WebRtcConnection : -StreamList streams;
WebRtcConnection : -DtlsTransport rtp;
WebRtcConnection : -Worker worker;
WebRtcConnection : +addStream%28%29;
WebRtcConnection : +removeStream%28%29;
OneToManyProcessor : +MediaSource publisher;
OneToManyProcessor : +MediaSink subscribers;
MediaStream : -Worker worker;
MediaStream : -Pipeline pipeline;
MediaStream : +onPacketReceived%28%29;
@enduml;
)

In summary, *WebRtcConnection will gather all MediaStreams* that receive/send data from/to the same DtlsTransport (connection). In Multiple Peer Connection cases there will be just one MediaStream per WebRtcConnection. Otherwise, there will be multiple MediaStreams. Finally, each *Stream will have its own Pipeline and Worker* to separate processing time and scale better to the number of streams per each connection.

All the logic to create/delete streams/webrtcconnections/onetomanyprocessors will still be placed in ErizoJS, so we need to add this logic there.

For connection-wide RTCP values (such as REMB), RTCP aggregation may be needed.

Furthermore, WebRtcConnection will no longer handle SDP data. It will create and receive JSON messages containing different parts of the negotiation (DTLS, ICE, etc.) . MediaStreams will also operate with JSON messages with information about the media streams (SSRC, msid, PT, etc.).
