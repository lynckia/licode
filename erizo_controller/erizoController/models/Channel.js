'use strict';
const events = require('events');
const logger = require('./../../common/logger').logger;
const crypto = require('crypto');
const log = logger.getLogger('ErizoController - Channel');

const NUVE_KEY = global.config.nuve.superserviceKey;

const calculateSignature = (token, key) => {
    var toSign = token.tokenId + ',' + token.host,
        signed = crypto.createHmac('sha1', key).update(toSign).digest('hex');
    return (new Buffer(signed)).toString('base64');
};

const checkSignature = (token, key) => {
    var calculatedSignature = calculateSignature(token, key);

    if (calculatedSignature !== token.signature) {
        log.info('message: invalid token signature');
        return false;
    } else {
        return true;
    }
};

function listenToSocketHandshakeEvents(channel) {
  channel.socket.on('token', channel.onToken.bind(channel));
  channel.socket.on('reconnected', channel.onReconnected.bind(channel));
  channel.socket.on('disconnect', channel.onDisconnect.bind(channel));
}

class Channel extends events.EventEmitter {
  constructor(socket, nuve) {
    super();
    this.socket = socket;
    this.nuve = nuve;
    listenToSocketHandshakeEvents(this);
  }

  onToken(token, callback) {
    log.debug('message: token received');
    if (checkSignature(token, NUVE_KEY)) {
      this.nuve.deleteToken(token.tokenId).then(tokenDB => {
        if (token.host === tokenDB.host) {
          this.emit('connected', tokenDB, callback);
        } else {
          log.warn('message: Token has invalid host, clientId: ' + this.id);
          callback('error', 'Invalid host');
          this.disconnect();
        }
      }).catch((reason) => {
        if (reason === 'error') {
            log.warn('message: Trying to use token that does not exist - ' +
                     'disconnecting Client, clientId: ' + this.id);
            callback('error', 'Token does not exist');
            this.disconnect();

        } else if (reason === 'timeout') {
            log.warn('message: Nuve does not respond token check - ' +
                     'disconnecting client, clientId: ' + this.id);
            callback('error', 'Nuve does not respond');
            this.disconnect();

        }
      });
    } else {
        log.warn('message: Token authentication error, clientId: ' + this.id);
        callback('error', 'Authentication error');
        this.disconnect();
    }
  }

  onDisconnect(reason) {
    log.debug('message: socket disconnected, reason:', reason);
    this.emit('disconnect');
  }

  socketOn(eventName, listener) {
    this.socket.on(eventName, listener);
  }

  onReconnected(clientId) {
    this.emit('reconnected',  clientId);
  }

  sendMessage(type, arg) {
    this.socket.emit(type, arg);
  }

  disconnect() {
    this.socket.disconnect();
  }

}

exports.Channel = Channel;
