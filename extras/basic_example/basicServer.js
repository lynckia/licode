/* global require, __dirname, console */

/* eslint-disable import/no-extraneous-dependencies, no-console */

const express = require('express');
const bodyParser = require('body-parser');
const errorhandler = require('errorhandler');
const morgan = require('morgan');
const logger = require('log4js');
// eslint-disable-next-line import/no-unresolved
const N = require('./nuve');
const fs = require('fs');
const https = require('https');
// eslint-disable-next-line import/no-unresolved
const config = require('./../../licode_config');

config.erizoController.ssl_key = config.erizoController.ssl_key || '../../cert/key.pem';
config.erizoController.ssl_cert = config.erizoController.ssl_cert || '../../cert/cert.pem';
config.basicExample.nuveUrl = config.basicExample.nuveUrl || 'http://localhost:3000/';

config.basicExample.logger = config.basicExample.logger || {};
const logFile = config.basicExample.logger.configFile || './log4js_configuration.json';

logger.configure(logFile);
const log = logger.getLogger('BasicExample');


const options = {
  key: fs.readFileSync(config.erizoController.ssl_key).toString(),
  cert: fs.readFileSync(config.erizoController.ssl_cert).toString(),
};

if (config.erizoController.sslCaCerts) {
  options.ca = [];
  config.erizoController.sslCaCerts.forEach((cert) => {
    options.ca.push(fs.readFileSync(cert).toString());
  });
}

const app = express();

// app.configure ya no existe
app.use(errorhandler({
  dumpExceptions: true,
  showStack: true,
}));
app.use(morgan('dev', {
  stream: {
    write: (str) => { log.debug(str.trim()); },
  },
  skip: (req, res) => (res.statusCode >= 400),
}));
app.use(morgan('dev', {
  stream: {
    write: (str) => { log.error(str.trim()); },
  },
  skip: (req, res) => (res.statusCode < 400),
}));
app.use(express.static(`${__dirname}/public`));

app.use(bodyParser.json());
app.use(bodyParser.urlencoded({
  extended: true,
}));
app.use((req, res, next) => {
  res.header('Access-Control-Allow-Origin', '*');
  res.header('Access-Control-Allow-Headers', 'Origin, X-Requested-With, Content-Type, Accept');
  next();
});


// app.set('views', __dirname + '/../views/');
// disable layout
// app.set("view options", {layout: false});

N.API.init(config.nuve.superserviceID, config.nuve.superserviceKey, config.basicExample.nuveUrl);

let defaultRoom;
const defaultRoomName = 'basicExampleRoom';

const getOrCreateRoom = (name, type = 'erizo', mediaConfiguration = 'default',
  callback = () => {}) => {
  if (name === defaultRoomName && defaultRoom) {
    callback(defaultRoom);
    return;
  }

  N.API.getRooms((roomlist) => {
    let theRoom = '';
    const rooms = JSON.parse(roomlist);
    for (let i = 0; i < rooms.length; i += 1) {
      const room = rooms[i];
      if (room.name === name &&
                room.data &&
                room.data.basicExampleRoom) {
        theRoom = room._id;
        callback(theRoom);
        return;
      }
    }
    const extra = { data: { basicExampleRoom: true }, mediaConfiguration };
    if (type === 'p2p') extra.p2p = true;

    N.API.createRoom(name, (roomID) => {
      theRoom = roomID._id;
      callback(theRoom);
    }, () => {}, extra);
  });
};

const deleteRoomsIfEmpty = (theRooms, callback) => {
  if (theRooms.length === 0) {
    callback(true);
    return;
  }
  const theRoomId = theRooms.pop()._id;
  N.API.getUsers(theRoomId, (userlist) => {
    const users = JSON.parse(userlist);
    if (Object.keys(users).length === 0) {
      N.API.deleteRoom(theRoomId, () => {
        deleteRoomsIfEmpty(theRooms, callback);
      });
    } else {
      deleteRoomsIfEmpty(theRooms, callback);
    }
  }, (error, status) => {
    log.error('Error getting user list for room ', theRoomId, 'reason: ', error);
    switch (status) {
      case 404:
        deleteRoomsIfEmpty(theRooms, callback);
        break;
      case 503:
        N.API.deleteRoom(theRoomId, () => {
          deleteRoomsIfEmpty(theRooms, callback);
        });
        break;
      default:
        break;
    }
  });
};

const cleanExampleRooms = (callback) => {
  log.debug('Cleaning basic example rooms');
  N.API.getRooms((roomlist) => {
    const rooms = JSON.parse(roomlist);
    const roomsToCheck = [];
    rooms.forEach((aRoom) => {
      if (aRoom.data &&
                aRoom.data.basicExampleRoom &&
                aRoom.name !== defaultRoomName) {
        roomsToCheck.push(aRoom);
      }
    });
    deleteRoomsIfEmpty(roomsToCheck, () => {
      callback('done');
    });
  }, (err) => {
    log.debug('Error cleaning example rooms', err);
    setTimeout(cleanExampleRooms.bind(this, callback), 3000);
  });
};

app.get('/getRooms/', (req, res) => {
  N.API.getRooms((rooms) => {
    res.send(rooms);
  });
});

app.get('/getUsers/:room', (req, res) => {
  const room = req.params.room;
  N.API.getUsers(room, (users) => {
    res.send(users);
  });
});


app.post('/createToken/', (req, res) => {
  log.debug('Creating token. Request body: ', req.body);

  const username = req.body.username;
  const role = req.body.role;

  let room = defaultRoomName;
  let type;
  let roomId;
  let mediaConfiguration;

  if (req.body.room) room = req.body.room;
  if (req.body.type) type = req.body.type;
  if (req.body.roomId) roomId = req.body.roomId;
  if (req.body.mediaConfiguration) mediaConfiguration = req.body.mediaConfiguration;

  const createToken = (tokenRoomId) => {
    N.API.createToken(tokenRoomId, username, role, (token) => {
      log.debug('Token created', token);
      res.send(token);
    }, (error) => {
      log.error('Error creating token', error);
      res.status(401).send('No Erizo Controller found');
    });
  };

  if (roomId) {
    createToken(roomId);
  } else {
    getOrCreateRoom(room, type, mediaConfiguration, createToken);
  }
});


app.use((req, res, next) => {
  res.header('Access-Control-Allow-Origin', '*');
  res.header('Access-Control-Allow-Methods', 'POST, GET, OPTIONS, DELETE');
  res.header('Access-Control-Allow-Headers', 'origin, content-type');
  if (req.method === 'OPTIONS') {
    res.send(200);
  } else {
    next();
  }
});

cleanExampleRooms(() => {
  getOrCreateRoom(defaultRoomName, undefined, undefined, (roomId) => {
    defaultRoom = roomId;
    let port = 3001;
    let tlsPort = 3004;
    if (config.basicExample && config.basicExample.port) {
      port = config.basicExample.port;
    }
    if (config.basicExample && config.basicExample.tlsPort) {
      tlsPort = config.basicExample.tlsPort;
    }

    app.listen(port);
    const server = https.createServer(options, app);
    log.info(`BasicExample started and listenting on port ${port}`);
    server.listen(tlsPort);
  });
});
