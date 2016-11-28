# Single Peer Connection - Feature Review
Single Peer Connection means that we can send/receive all streams in the same PeerConnection, sharing UDP ports, ICE credentials, DTLS certificates, etc. It usually means that publishers/subscribers use a single UDP "connection", with some benefits: reduce the number of ports used by the clients and server and shorter times to publish/subscribe new streams.

![comparison](https://docs.google.com/drawings/d/1EMW43_6BX0mDJ2v4kLWrzDobz6dRjjqhctSkLM869io/pub?w=960&h=721)

## Architecture
Single Peer Connection will carry changes in both client and server sides.

This new feature will be optional because we'd like to keep backwards compatibility with older systems and be sure we can disable it if some browser does not support it.

## API Changes
Client: All changes will be hidden inside code.

Server: We will probably propose new policies for Erizo Controller Cloud Handler.

## Details
This features carries changes in:

- SDP: there will be a single SDP per connection, with information about all the streams that take part of it.

- Renegotiation: Offer/Answer requests could add/remove more streams to the SDP. A new offer does not mean that we will negotiate a new ICE, or handshake new DTLS keys. It will only affect to the number of streams that are added/removed in the SDP. There will be several new scenarios for renegotiating with Offer/Answer requests:

  - When publishing or subscribing to the first stream, the client will create a Peer Connection, and Erizo will send an Offer/SDP with the stream info and ICE candidates. It will be exactly the same way we're doing now. And the client will send a proper Answer.

  - When a client wants to publish/subscribe to a new Stream, Erizo will send a new Offer/SDP with the new Stream info. And the client will send a proper Answer.

  - When a client unpublish/unsubscribe from the latest stream in the SDP, Erizo and the client will close the connection as we're doing now.

  - When the publisher wants to unpublish one of the Streams, Erizo will send an Offer/SDP  with the stream removed from the info.

  - When a subscribed Stream is removed, Erizo will send a new Offer/SDP removing the info of the corresponding Stream.

Both Erizo Client and Erizo Controler might decide not to use Single PC for the next reasons:

  - The browser does not support single peer connection.

  - Erizo Controller decides that for scalability purposes it is preferable to use another ErizoJS and so, different Peer Connections.

In terms of message flow, current solution with Multiple Peer Connections is as follows:

![Multiple Peer Connections](http://g.gravizo.com/g?
@startuml;
actor User;
participant "Erizo Client" as Client;
participant "Erizo Controller" as EC;
User->Client: publish stream1;
Client->EC: publish stream1;
Client<-EC: offer;
Client->EC: answer;
Client<->EC: ICE Negotiation;
Client<->EC: DTLS Negotiation;
User->Client: subscribe stream2;
Client->EC: publish stream2;
Client<-EC: offer;
Client->EC: answer;
Client<->EC: ICE Negotiation;
Client<->EC: DTLS Negotiation;
User->Client: subscribe stream3;
Client->EC: publish stream3;
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
Client->EC: answer;
Client<->EC: ICE Negotiation;
Client<->EC: DTLS Negotiation;
User->Client: subscribe stream2;
Client->EC: publish stream2;
Client<-EC: offer;
Client->EC: answer;
User->Client: subscribe stream3;
Client->EC: publish stream3;
Client<-EC: offer;
Client->EC: answer;
@enduml
)

### How does it affect Erizo Client?
Streams will be added to existing Peer Connections, and will need to keep track of the existing Peer Connections to decide whenever a user wants to publish/subscribe to a new Stream if we need to create a new Peer Connection or update an existing one. It will also depends on the offer received from Erizo.

Erizo Client will be told to use Single Peer Connection or Multiple Peer Connections.

So, if Erizo Client detects that the browser supports Single PC it will send a publish/subscribe message to Erizo Controller with a Single PC request.

### How does it affect Nuve?
It does not affect Nuve.

### How does it affect Erizo Controller / Erizo Agent / Erizo JS?
We will create a new policy for Erizo Controller Cloud Handler, to make sure we reuse ErizoJS in the same rooms.
Erizo Controller will receive a publish/subscribe message with the Single PC request. Based on the policy used it will finally accept Single PC or not and will send the corresponding offer. It will also have to tell Erizo JS to apply Single PC or Multiple PC solution, so the request to ErizoJS will be updated with the previous result.

It affects Erizo Agent because it needs to choose the same ErizoJS for streams in the same room, according to the policy.

It affects Erizo JS because there will be a new object called Streams, and we will add Streams to WebRtcConnections (see next section).

![Erizo Controller Logic](http://g.gravizo.com/g?
@startuml;
%28*%29 --> if "Single PC request from client?" then;
  -->[true] "Check Cloud Handler";
  if "Can  we reuse a ErizoJS?" then;
    -down->[true] ===SEND_SPC===;
  else;
    -right-->[false] "Create another ErizoJS" as create_erizojs;
    --> ===SEND_SPC===;
  endif;
  --> "Send a single PC request to ErizoJS" as create_spc;
  --> if "Does ErizoJS contain a PC yet?" then;
    ->[true] ===ADD_STREAM===;
  else;
    ->[false] "Create WebRtcConnection";
    --> ===ADD_STREAM===;
  endif;
  --> "Add stream to WebRtcConnection";
else;
  ->[false] "Send a multiple PC request to ErizoJS" as create_mpc;
endif;
create_mpc --> "Create WebRtcConnection and add stream" as mpc;
--> answer;
@enduml
)

### How does it affect ErizoAPI / Erizo?
Much functionality inside WebRtcConnection will be moved to Stream. And WebRtcConnection will handle Streams directly. The new architecture should be compatible with Single and Multiple PC.

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
Stream<--WebRtcConnection;
Stream<--OneToManyProcessor;
WebRtcConnection<--OneToManyProcessor;
DtlsTransport<--WebRtcConnection;
WebRtcConnection : -StreamList streams;
WebRtcConnection : -DtlsTransport rtp;
WebRtcConnection : +addStream%28%29;
WebRtcConnection : +removeStream%28%29;
OneToManyProcessor : +MediaSource publisher;
OneToManyProcessor : +MediaSink subscribers;
Stream : -Worker worker;
Stream : -Pipeline pipeline;
Stream : +onPacketReceived%28%29;
@enduml;
)

In summary, WebRtcConnection will be the object to gather all Streams that receive/send data from/to the same DtlsTransport (connection). In Multiple Peer Connection cases there will be just one Stream per WebRtcConnection. Otherwise, there will be multiple Streams. WebRtcConnection will then control DtlsTransport, to start/stop it whenever it has 1+ or 0 running streams. Finally, each Stream will have its own Worker to separate processing time and scale better to the number of streams per each connection.

All the logic to create/delete streams/webrtcconnections/onetomanyprocessors will still be placed in ErizoJS, so we need to add this logic there.

## Additional considerations
This will change the way we currently assign ErizoJS to Streams.
