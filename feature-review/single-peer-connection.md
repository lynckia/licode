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
Erizo Controller will receive a publish/subscribe message with the Single PC request. Based on the policy used it will finally accept Single PC or not and will send the corresponding offer.

It affects Erizo Agent because it needs to choose the same ErizoJS for streams in the same room, according to the policy.

It affects Erizo JS because there will be a new object called Streams, and we will add Streams to WebRtcConnections.

### How does it affect ErizoAPI / Erizo?
Much functionality inside WebRtcConnection will be moved to Stream. And WebRtcConnection will handle Streams directly. The new architecture should be compatible with Single and Multiple PC.

## Additional considerations
This will change the way we currently assign ErizoJS to Streams.
