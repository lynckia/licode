var express = require('express'); 
var db = require('./mdb/dataBase').db;
require('./rpc/rpcServer');
var app = express.createServer();


var nuveAuthenticator = require('./auth/nuveAuthenticator');

var roomsResource = require('./resource/roomsResource');
var roomResource = require('./resource/roomResource');
var tokensResource = require('./resource/tokensResource');
var servicesResource = require('./resource/servicesResource');
var serviceResource = require('./resource/serviceResource');
var usersResource = require('./resource/usersResource');
var userResource = require('./resource/userResource');

app.use(express.bodyParser());
app.set('view engine', 'ejs');
app.set('view options', {
  layout: false
});

app.get('/test', function(req,res) {
	res.render('test');
});

app.all('*', nuveAuthenticator.authenticate);

app.post('/rooms', roomsResource.createRoom);
app.get('/rooms', roomsResource.represent);

app.get('/rooms/:room', roomResource.represent);
app.delete('/rooms/:room', roomResource.deleteRoom);

app.post('/rooms/:room/tokens', tokensResource.create);

app.post('/services', servicesResource.create);
app.get('/services', servicesResource.represent);

app.get('/services/:service', serviceResource.represent);
app.delete('/services/:service', serviceResource.deleteService);

app.get('/rooms/:room/users', usersResource.getList);

app.get('/rooms/:room/users/:user', userResource.getUser);
app.delete('/rooms/:room/users/:user', userResource.deleteUser);

app.listen(3000);