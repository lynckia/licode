# Automatic Subscriptions - Feature Review

Automatic subscriptions is a new way for clients to subscribe to streams whose properties meet some criteria, without having to call `room.subscribe()` for each of those streams.

Clients will be able to use selectors, which will be based on stream properties, like `id`, `attributes`, etc. to decide which existing or future streams they want to subscribe in the room.

## Changes in the Client API

### Auto Subscriptions by Selector

```js
const selectors = {
  '/id': '23',
  '/attributes/group': '23',
  '/attributes/kind': 'professor',
  '/attributes/externalId': '10'
};
const options = {audio: true, video: false, forceTurn: true};
room.autoSubscribe(selectors, options);

// When stream meets one or more of the selectors and is finally subscribed.
room.on('stream-subscribed', stream => {

});

// When a stream does not meet any selector anymore and is finally unsubscribed.
room.on('stream-unsubscribed', stream => {

});
```

### Update Stream attributes

It already exists

## Changes in the Server API

### Update Stream attributes (2nd step)

```js
const roomId = '30121g51113e74fff3115502';
const streamAttributes = [
  {
    selector: {
      '/attributes/externalId': '10',
    },
    patch: [  // JSON Patch (http://jsonpatch.com/)
      { op: 'remove', path: '/attributes/group'},
      { op: 'add', path: '/attributes/kind', value: 'professor'},
    ],
  }
];

N.API.updateRoomStreamAttributes(roomId, streamAttributes, (resp) => {
}, errorCallback);
```

## Architecture

### Changes in ErizoController

It handles requests to subscribe with selectors with the AutoSubscriptionHandler. AutoSubscriptionHandler will perform subscribe/unsubscribe actions based on the given selectors.

Actions will be triggered in any of the following cases:
- New 'stream-published' events inside ErizoController.
- Changes in the stream properties (including attributes).
- Changes in the selectors themselves (calls to autoSubscribe function).

#### Actions:

- Subscription:

Triggered whenever a stream is published, or any selector meets at least one of the stream properties.
It sends a new 'subscribe' message to the ErizoJS where the stream was published.

- Unsubscription:

Triggered if no selector meets any of the stream properties. Note that if a stream is unpublished Erizo automatically cancels any subscription yet.
It sends a new 'unsubscribe' message to the ErizoJS where the stream is published.

### Changes in ErizoJS

We need to create Offers from Subscribers, so we now need to pass media information from the Publisher to the Subscriber in Erizo, and that will help us to create the SDP, use RTP extensions, etc.

Also, ErizoJS may detect that there is a suitable WebRtcConnection or not, and if they will use Single Peer Connection, so in some cases it should start an ICE negotiation and in others it just need to do a renegotiation, by adding/removing the stream.

### Changes in Erizo client

There will be a new API, as we saw before. Clients will now create subscriptions by receiving directly the Offer from ErizoJS, so they'll need to check if the should create a PeerConnection, and then apply the remote description, create an answer and send it back.

### Changes in Nuve

There is an optional change in Nuve if we want to implement the optional server API. It will allow us to do bulk and secure changes in the streams attributes as an 'atomic' operation, which will cause that all subscriptions/unsubscriptions will happen almost at the same time. With this API Nuve will send changes to the corresponding ErizoController, which will update the AutoSubscriptionHandlers for each client belonging to the room.

### Additional notes

SDP negotiation usually involves the exchange of an offer and an answer between peers. But in this case we might not need to send the answer back from clients to Erizo (which will save bandwidth and CPU in the server in some cases). As a first step we can send the answer and then investigate whether it makes sense to drop it in the client or not.
