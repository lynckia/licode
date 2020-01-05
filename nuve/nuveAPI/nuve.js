/* global require, __dirname */

// eslint-disable-next-line import/no-extraneous-dependencies
const express = require('express');
// eslint-disable-next-line import/no-extraneous-dependencies
const bodyParser = require('body-parser');
const rpc = require('./rpc/rpc');

// eslint-disable-next-line import/no-unresolved
const config = require('./../../licode_config');

const app = express();

rpc.connect();

const nuveAuthenticator = require('./auth/nuveAuthenticator');

const roomsResource = require('./resource/roomsResource');
const roomResource = require('./resource/roomResource');
const tokensResource = require('./resource/tokensResource');
const servicesResource = require('./resource/servicesResource');
const serviceResource = require('./resource/serviceResource');
const usersResource = require('./resource/usersResource');
const userResource = require('./resource/userResource');

app.use(express.static(`${__dirname}/public`));
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({
  extended: true,
}));
app.set('view engine', 'ejs');
app.set('view options', {
  layout: false,
});

app.use((req, res, next) => {
  res.header('Access-Control-Allow-Origin', '*');
  res.header('Access-Control-Allow-Methods', 'POST, GET, OPTIONS, DELETE');
  res.header('Access-Control-Allow-Headers', 'origin, authorization, content-type');

  next();
});

app.get('/test', (req, res) => {
  res.render('test');
});

app.options('*', (req, res) => {
  res.send(200);
});

app.get('*', nuveAuthenticator.authenticate);
app.post('*', nuveAuthenticator.authenticate);
app.put('*', nuveAuthenticator.authenticate);
app.patch('*', nuveAuthenticator.authenticate);
app.delete('*', nuveAuthenticator.authenticate);

app.post('/rooms', roomsResource.createRoom);
app.get('/rooms', roomsResource.represent);

app.get('/rooms/:room', roomResource.represent);
app.put('/rooms/:room', roomResource.updateRoom);
app.patch('/rooms/:room', roomResource.patchRoom);
app.delete('/rooms/:room', roomResource.deleteRoom);

app.post('/rooms/:room/tokens', tokensResource.create);

app.post('/services', servicesResource.create);
app.get('/services', servicesResource.represent);

app.get('/services/:service', serviceResource.represent);
app.delete('/services/:service', serviceResource.deleteService);

app.get('/rooms/:room/users', usersResource.getList);

app.get('/rooms/:room/users/:user', userResource.getUser);
app.delete('/rooms/:room/users/:user', userResource.deleteUser);

// handle 404 errors
app.use((req, res) => {
  res.status(404).send('Resource not found');
});

const nuvePort = config.nuve.port || 3000;

app.listen(nuvePort);
