# Overview

The Licode server-side API provides your server communication with Nuve.

Nuve is a Licode module that manages some resources like the videoconference rooms or the tokens to access to a determined room. Server-side API is available in node.js.

| Class                   | Description                                                                     |
|-------------------------|---------------------------------------------------------------------------------|
| [Room](#rooms)          | Represents a videoconference room.                                              |
| [Token](#tokens)        | The key to add a new participant to a videoconference room.                     |
| [User](#users)          | A user that can publish or subscribe to streams in a videoconference room.      |

# Initialize

Once you compile the API, you need to require it in your node.js server and initialize it with a Service. Call the constructor with the Service Id and the Service Key created in the data base.

```
var N = require('./nuve');
N.API.init(serviceId, serviceKey, nuve_host);
```

With a Service you can create videoconference rooms for your videoconference application and get the neccesary tokens for add participants to them. Also you can ask Nuve about the users connected to a room.

You can also compile Nuve API for python.

# Rooms

A Room object represent a videoconference room. In a room participate users that can interchange their streams. Each participant can publish his stream and/or subscribe to the other streams published in the room.

A Room object has the following properties:

- `Room.name`: the name of the room.
- `Room._id`: a unique identifier for the room.
- `Room.p2p` (optional): boolean that indicates if the room is a peer - to - peer room. In p2p rooms server side is only used for signalling.
- `Room.mediaConfiguration` (optional): a string with the media configuration used for this room.
- `Room.data` (optional): additional metadata for the room.

In your service you can create a room, get a list of rooms that you have created, get the info about a determined room or delete a room when you don't need it.

In all functions you can include an optional error callback ir order to catch possible problems with nuve server.

## Create Room

To create a room you need to specify a name and a callback function. When the room is created Nuve calls this function and returns you the roomId:

```
var roomName = 'myFirstRoom';

N.API.createRoom(roomName, function(room) {
  console.log('Room created with id: ', room._id);
}, errorCallback);
```

You can create peer - to - peer rooms in which users will communicate directly between their browsers using server side only for signalling.

```
var roomName = 'myP2PRoom';

N.API.createRoom(roomName, function(room) {
  console.log('P2P room created with id: ', room._id);
}, errorCallback, {p2p: true});
```

You can include metadata when creating the room. This metadata will be stored in Room.data field of the room.

```
var roomName = 'myRoomWithMetadata';

N.API.createRoom(roomName, function(room) {
  console.log('Room created with id: ', room._id);
}, errorCallback, {data: {room_color: 'red', room_description: 'Room for testing metadata'}});
```

You can also specify which media configuration you want to use in the Room.

```
N.API.createRoom(roomName, function(room) {
  console.log('Room created with id: ', room._id);
}, errorCallback, {mediaConfiguration: 'VP8_AND_OPUS'});
```

## Get Rooms

You can ask Nuve for a list of the rooms in your service:

```
N.API.getRooms(function(roomList) {
  var rooms = JSON.parse(roomList);
  for(var i in rooms) {
    console.log('Room ', i, ':', rooms[i].name);
  }
}, errorCallback);
```

## Get Room

Also you can get the info about a determined room with its roomId:

```
var roomId = '30121g51113e74fff3115502';

N.API.getRoom(roomId, function(resp) {
  var room= JSON.parse(resp);
  console.log('Room name: ', room.name);
}, errorCallback);
```

## Delete Room

And finally, to delete a determined room:

```
var roomId = '30121g51113e74fff3115502';

N.API.deleteRoom(roomId, function(result) {
  console.log('Result: ', result);
}, errorCallback);
```

# Tokens

A Token is a string that allows you to add a new participant to a determined room.

When you want to add a new participant to a room, you need to create a new token than you will consume in the client-side API. To create a token you need to specify a name and a role for the new participant.

- `Name`: a name that identify the participant.
- `Role`: indicates de permissions that the user will have in the room. To learn more about how to manage roles and permmisions you can visit <a href="http://lynckia.com/licode/roles.html" target="_blank">this post</a>.

## Create Token

```
var roomId = '30121g51113e74fff3115502';
var name = 'userName';
var role = '';

N.API.createToken(roomId, name, role, function(token) {
  console.log('Token created: ', token);
}, errorCallback);
```


# Users

A User object represents a participant in a videoconference room.

A User object has the following properties:

- `User.name`: the name specified when you created the token.
- `User.role`: the role specified when you created the token.

You can ask Nuve for a list of the users connected to a determined room.

## Get Users

```
var roomId = '30121g51113e74fff3115502';

N.API.getUsers(roomId, function(users) {
  var usersList = JSON.parse(users);
  console.log('This room has ', usersList.length, 'users');

  for(var i in usersList) {
    console.log('User ', i, ':', usersList[i].name, 'with role: ', usersList[i].role);
  }
}, errorCallback);
```

# Examples

With a deep intro into the contents out of the way, we can focus putting Server API to use. To do that, we'll utilize a basic example that includes everything we mentioned in the previous section.

Now, here's a look at a typical server application implemented in Node.js:

We first initialize Nuve (Server API)

```
var N = require('nuve');
N.API.init("531b26113e74ee30500001", "myKey", "http://localhost:3000/");
```

We also include express support for our server. We prepare it to publish static HTML files in which we will have JavaScript client applications that we will explain later.

```
var express = require('express');
var app = express.createServer();

app.use(express.bodyParser());
app.configure(function () {
    app.use(express.logger());
    app.use(express.static(__dirname + '/public'));
});
```

Requests to `/createRoom/` URL will create a new Room in Licode.

```
app.post('/createRoom/', function(req, res){

    N.API.createRoom('myRoom', function(roomID) {
        res.send(roomID);
    }, function (e) {
        console.log('Error: ', e);
    });
});
```

Requests to `/getRooms/` URL will retrieve a list of our Rooms in Licode.

```
app.get('/getRooms/', function(req, res){

    N.API.getRooms(function(rooms) {
        res.send(rooms);
    }, function (e) {
        console.log('Error: ', e);
    });
});
```

Requests to `/getUsers/roomID` URL will retrieve a list of users that are connected to room `roomID`.

```
app.get('/getUsers/:room', function(req, res){

    var room = req.params.room;
    N.API.getUsers(room, function(users) {
        res.send(users);
    }, function (e) {
        console.log('Error: ', e);
    });
});
```

Requests to `/createToken/roomID` URL will create an access token for including a participant in room `roomID`.

```
app.post('/createToken/:room', function(req, res){

    var room = req.params.room;
    var username = req.body.username;
    var role = req.body.role;
    N.API.createToken(room, username, role, function(token) {
        res.send(token);
    }, function (e) {
        console.log('Error: ', e);
    });
});
```

Finally, we will start our service, that will listen to port 80 (line 19).

```
app.listen (80);
```
