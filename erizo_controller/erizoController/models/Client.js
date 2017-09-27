'use strict';
const events = require('events');
const uuidv4 = require('uuid/v4');
const Permission = require('../permission');
const ST = require('./Stream');
const logger = require('./../../common/logger').logger;
const log = logger.getLogger('ErizoController - Client');

const PUBLISHER_INITAL = 101, PUBLISHER_READY = 104;

function listenToSocketEvents(client) {
  client.channel.socketOn('sendDataStream', client.onSendDataStream.bind(client));
  client.channel.socketOn('signaling_message', client.onSignalingMessage.bind(client));
  client.channel.socketOn('updateStreamAttributes', client.onUpdateStreamAttributes.bind(client));
  client.channel.socketOn('publish', client.onPublish.bind(client));
  client.channel.socketOn('subscribe', client.onSubscribe.bind(client));
  client.channel.socketOn('startRecorder', client.onStartRecorder.bind(client));
  client.channel.socketOn('stopRecorder', client.onStopRecorder.bind(client));
  client.channel.socketOn('unpublish', client.onUnpublish.bind(client));
  client.channel.socketOn('unsubscribe', client.onUnsubscribe.bind(client));
  client.channel.socketOn('getStreamStats', client.onGetStreamStats.bind(client));
  client.channel.on('disconnect', client.onDisconnect.bind(client));
}

class Client extends events.EventEmitter {
  constructor(channel, token, room) {
    super();
    this.channel = channel;
    this.room = room;
    this.token = token;
    this.id = uuidv4();
    listenToSocketEvents(this);
    this.user = {name: token.userName, role: token.role, permissions: {}};
    const permissions = global.config.erizoController.roles[token.role] || [];
    for (const right in permissions) {
        this.user.permissions[right] = permissions[right];
    }
    this.streams = []; //[list of streamIds]
    this.state = 'sleeping'; // ?
  }

  disconnect() {
    this.channel.disconnect();
  }

  setNewChannel(channel) {
    const oldChannel = this.channel;
    const buffer = oldChannel.getBuffer();
    log.info('message: reconnected, oldChannelId:', oldChannel.id, ', channelId:', channel.id);
    oldChannel.removeAllListeners();
    oldChannel.disconnect();
    this.channel = channel;
    listenToSocketEvents(this);
    this.channel.sendBuffer(buffer);
  }

  sendMessage(type, arg) {
    this.channel.sendMessage(type, arg);
  }

  hasPermission(action) {
    return this.user && this.user.permissions[action] === true;
  }

  onSendDataStream(message) {
    const stream = this.room.getStreamById(message.id);
    if  (stream === undefined){
      log.warn('message: Trying to send Data from a non-initialized stream, ' +
               'clientId: ' + this.id + ', ' +
               logger.objectToLog(message));
      return;
    }
    const clients = stream.getDataSubscribers();

    for (const index in clients) {
      const clientId = clients[index];
      const client = this.room.getClientById(clientId);
      if (client) {
        log.debug('message: sending dataStream, ' +
          'clientId: ' + clientId + ', dataStream: ' + message.id);
        this.room.getClientById(clientId).sendMessage('onDataStream', message);
      }
    }
  }

  onSignalingMessage(message) {
    if (this.room === undefined) {
      log.error('message: singaling_message for user in undefined room ' +
        message.streamId + ', user: ' + this.user);
      this.disconnect();
    }
    if (this.room.p2p) {
      const targetClient = this.room.getClientById(message.peerSocket);
      targetClient.sendMessage('signaling_message_peer',
                {streamId: message.streamId, peerSocket: this.id, msg: message.msg});
    } else {
        const isControlMessage = message.msg.type === 'control';
        if (!isControlMessage ||
            (isControlMessage && this.hasPermission(message.msg.action.name))) {
          this.room.controller.processSignaling(message.streamId, this.id, message.msg);
        } else {
          log.info('message: User unauthorized to execute action on stream, action: ' +
            message.msg.action.name + ', streamId: ' + message.streamId);
        }
    }
  }

  onUpdateStreamAttributes(message) {
    const stream = this.room.getStreamById(message.id);
    if  (stream === undefined){
      log.warn('message: Update attributes to a uninitialized stream, ' +
               logger.objectToLog(message));
      return;
    }
    const clients = stream.getDataSubscribers();
    stream.setAttributes(message.attrs);

    for (const index in clients) {
      const clientId = clients[index];
      const client = this.room.getClientById(clientId);
      if (client) {
            log.debug('message: Sending new attributes, ' +
                      'clientId: ' + clientId + ', streamId: ' + message.id);
            client.sendMessage('onUpdateAttributeStream', message);
        }
    }
  }

  onPublish(options, sdp, callback) {
    if (this.user === undefined || !this.user.permissions[Permission.PUBLISH]) {
        callback(null, 'Unauthorized');
        return;
    }
    if (this.user.permissions[Permission.PUBLISH] !== true) {
        const permissions = this.user.permissions[Permission.PUBLISH];
        for (const right in permissions) {
            if ((options[right] === true) && (permissions[right] === false)) {
              callback(null, 'Unauthorized');
              return;
            }
        }
    }

    // generate a 18 digits safe integer
    const id = Math.floor(100000000000000000 + Math.random() * 900000000000000000);

    if (options.state === 'url' || options.state === 'recording') {
        let url = sdp;
        if (options.state === 'recording') {
            const recordingId = sdp;
            if (global.config.erizoController.recording_path) {  // jshint ignore:line
                url = global.config.erizoController.recording_path + recordingId + '.mkv'; // jshint ignore:line
            } else {
                url = '/tmp/' + recordingId + '.mkv';
            }
        }
        this.room.controller.addExternalInput(id, url, (result) => {
            if (result === 'success') {
                let st = new ST.Stream({id: id,
                                    client: this.id,
                                    audio: options.audio,
                                    video: options.video,
                                    data: options.data,
                                    attributes: options.attributes});
                st.status = PUBLISHER_READY;
                this.streams.push(id);
                this.room.streams.set(id, st);
                callback(id);
                this.room.sendMessage('onAddStream', st.getPublicStream());
            } else {
                callback(null, 'Error adding External Input:' + result);
            }
        });
    } else if (options.state === 'erizo') {
        let st;
        log.info('message: addPublisher requested, ' +
                 'streamId: ' + id + ', clientId: ' + this.id + ', ' +
                 logger.objectToLog(options) + ', ' +
                 logger.objectToLog(options.attributes));
        this.room.controller.addPublisher(id, options, (signMess) => {
            if (signMess.type === 'initializing') {
                callback(id);
                st = new ST.Stream({id: id,
                                    client: this.id,
                                    audio: options.audio,
                                    video: options.video,
                                    data: options.data,
                                    screen: options.screen,
                                    attributes: options.attributes});
                this.streams.push(id);
                this.room.streams.set(id, st);
                st.status = PUBLISHER_INITAL;
                log.info('message: addPublisher, ' +
                         'state: PUBLISHER_INITIAL, ' +
                         'clientId: ' + this.id + ', ' +
                         'streamId: ' + id);

                if (global.config.erizoController.report.session_events) {  // jshint ignore:line
                    var timeStamp = new Date();
                    this.room.amqper.broadcast('event', {room: this.room.id,
                                               user: this.id,
                                               name: this.user.name,
                                               type: 'publish',
                                               stream: id,
                                               timestamp: timeStamp.getTime(),
                                               agent: signMess.agentId,
                                               attributes: options.attributes});
                }
            } else if (signMess.type === 'failed'){
                log.warn('message: addPublisher ICE Failed, ' +
                         'state: PUBLISHER_FAILED, ' +
                         'streamId: ' + id + ', ' +
                         'clientId: ' + this.id);
                this.sendMessage('connection_failed',{type:'publish', streamId: id});
                //We're going to let the client disconnect
                return;
            } else if (signMess.type === 'ready') {
                st.status = PUBLISHER_READY;
                this.room.sendMessage('onAddStream', st.getPublicStream());
                log.info('message: addPublisher, ' +
                         'state: PUBLISHER_READY, ' +
                         'streamId: ' + id + ', ' +
                         'clientId: ' + this.id);
            } else if (signMess === 'timeout-erizojs') {
                log.error('message: addPublisher timeout when contacting ErizoJS, ' +
                          'streamId: ' + id + ', clientId: ' + this.id);
                callback(null, 'ErizoJS is not reachable');
                return;
            } else if (signMess === 'timeout-agent'){
                log.error('message: addPublisher timeout when contacting Agent, ' +
                          'streamId: ' + id + ', clientId: ' + this.id);
                callback(null, 'ErizoAgent is not reachable');
                return;
            } else if (signMess === 'timeout'){
                log.error('message: addPublisher Undefined RPC Timeout, ' +
                          'streamId: ' + id + ', clientId: ' + this.id);
                callback(null, 'ErizoAgent or ErizoJS is not reachable');
                return;
            }
            log.debug('Sending message back to the client', signMess);
            this.sendMessage('signaling_message_erizo', {mess: signMess, streamId: id});
        });
    } else {
        let st = new ST.Stream({id: id,
                            client: this.id,
                            audio: options.audio,
                            video: options.video,
                            data: options.data,
                            screen: options.screen,
                            attributes: options.attributes});
        this.streams.push(id);
        this.room.streams.set(id, st);
        st.status = PUBLISHER_READY;
        callback(id);
        this.room.sendMessage('onAddStream', st.getPublicStream());
    }
  }

  onSubscribe(options, sdp, callback) {
    log.info('Subscribing', options, this.user);
    if (this.user === undefined || !this.user.permissions[Permission.SUBSCRIBE]) {
        callback(null, 'Unauthorized');
        return;
    }

    if (this.user.permissions[Permission.SUBSCRIBE] !== true) {
        var permissions = this.user.permissions[Permission.SUBSCRIBE];
        for (var right in permissions) {
            if ((options[right] === true) && (permissions[right] === false))
                return callback(null, 'Unauthorized');
        }
    }

    var stream = this.room.getStreamById(options.streamId);
    if (stream === undefined) {
        return;
    }

    if (stream.hasData() && options.data !== false) {
        stream.addDataSubscriber(this.id);
    }

    if (stream.hasAudio() || stream.hasVideo() || stream.hasScreen()) {
        if (this.room.p2p) {
            const clientId = stream.getClient();
            const client = this.room.getClientById(clientId);
            client.sendMessage('publish_me', {streamId: options.streamId, peerSocket: this.id});
        } else {
            log.info('message: addSubscriber requested, ' +
                     'streamId: ' + options.streamId + ', ' +
                     'clientId: ' + this.id);
            this.room.controller.addSubscriber(this.id, options.streamId, options, (signMess) => {
                if (signMess.type === 'initializing') {
                    log.info('message: addSubscriber, ' +
                             'state: SUBSCRIBER_INITIAL, ' +
                             'clientId: ' + this.id + ', ' +
                             'streamId: ' + options.streamId);
                    callback(true);
                    if (global.config.erizoController.report.session_events) {  // jshint ignore:line
                        var timeStamp = new Date();
                        this.room.amqper.broadcast('event', {room: this.room.id,
                                                   user: this.id,
                                                   name: this.user.name,
                                                   type: 'subscribe',
                                                   stream: options.streamId,
                                                   timestamp: timeStamp.getTime()});
                    }
                    return;
                } else if (signMess.type === 'failed'){
                    //TODO: Add Stats event
                    log.warn('message: addSubscriber ICE Failed, ' +
                             'state: SUBSCRIBER_FAILED, ' +
                             'streamId: ' + options.streamId + ', ' +
                             'clientId: ' + this.id);
                    this.sendMessage('connection_failed', {type: 'subscribe',
                                                      streamId: options.streamId});
                    return;
                } else if (signMess.type === 'ready') {
                    log.info('message: addSubscriber, ' +
                             'state: SUBSCRIBER_READY, ' +
                             'streamId: ' + options.streamId + ', ' +
                             'clientId: ' + this.id);

                } else if (signMess.type === 'bandwidthAlert') {
                    this.sendMessage('onBandwidthAlert', {streamID: options.streamId,
                                                   message: signMess.message,
                                                   bandwidth: signMess.bandwidth});
                } else if (signMess === 'timeout') {
                    log.error('message: addSubscriber timeout when contacting ErizoJS, ' +
                              'streamId: ', options.streamId, ', ' +
                              'clientId: ' + this.id);
                    callback(null, 'ErizoJS is not reachable');
                    return;
                }

                this.sendMessage('signaling_message_erizo', {mess: signMess,
                                                             peerId: options.streamId});
            });
        }
    } else {
        callback(true);
    }
  }

  onStartRecorder(options, callback) {
    if (this.user === undefined || !this.user.permissions[Permission.RECORD]) {
        callback(null, 'Unauthorized');
        return;
    }
    var streamId = options.to;
    var recordingId = Math.random() * 1000000000000000000;
    var url;

    if (global.config.erizoController.recording_path) {  // jshint ignore:line
        url = global.config.erizoController.recording_path + recordingId + '.mkv';  // jshint ignore:line
    } else {
        url = '/tmp/' + recordingId + '.mkv';
    }

    log.info('message: startRecorder, ' +
             'state: RECORD_REQUESTED, ' +
             'streamId: ' + streamId + ', ' +
             'url: ' + url);

    if (this.room.p2p) {
       callback(null, 'Stream can not be recorded');
    }

    let stream = this.room.getStreamById(streamId);

    if (stream.hasAudio() || stream.hasVideo() || stream.hasScreen()) {
        this.room.controller.addExternalOutput(streamId, url, function (result) {
            if (result === 'success') {
                log.info('message: startRecorder, ' +
                         'state: RECORD_STARTED, ' +
                         'streamId: ' + streamId + ', ' +
                         'url: ' + url);
                callback(recordingId);
            } else {
                log.warn('message: startRecorder stream not found, ' +
                         'state: RECORD_FAILED, ' +
                         'streamId: ' + streamId + ', ' +
                         'url: ' + url);
                callback(null, 'Unable to subscribe to stream for recording, ' +
                               'publisher not present');
            }
        });

    } else {
        log.warn('message: startRecorder stream cannot be recorded, ' +
                 'state: RECORD_FAILED, ' +
                 'streamId: ' + streamId + ', ' +
                 'url: ' + url);
        callback(null, 'Stream can not be recorded');
    }
  }

  onStopRecorder(options, callback) {
    if (this.user === undefined || !this.user.permissions[Permission.RECORD]) {
        if (callback) callback(null, 'Unauthorized');
        return;
    }
    var recordingId = options.id;
    var url;

    if (global.config.erizoController.recording_path) {  // jshint ignore:line
        url = global.config.erizoController.recording_path + recordingId + '.mkv';  // jshint ignore:line
    } else {
        url = '/tmp/' + recordingId + '.mkv';
    }

    log.info('message: startRecorder, ' +
             'state: RECORD_STOPPED, ' +
             'streamId: ' + options.id + ', ' +
             'url: ' + url);
    this.room.controller.removeExternalOutput(url, callback);
  }

  onUnpublish(streamId, callback) {
    if (this.user === undefined || !this.user.permissions[Permission.PUBLISH]) {
        if (callback) callback(null, 'Unauthorized');
        return;
    }

    // Stream has been already deleted or it does not exist
    let stream = this.room.getStreamById(streamId);
    if (stream === undefined) {
        return;
    }

    this.room.sendMessage('onRemoveStream', {id: streamId});

    if (stream.hasAudio() || stream.hasVideo() || stream.hasScreen()) {

        this.state = 'sleeping';
        if (!this.room.p2p) {
            this.room.controller.removePublisher(streamId);
            if (global.config.erizoController.report.session_events) {  // jshint ignore:line
                var timeStamp = new Date();
                this.room.amqper.broadcast('event', {room: this.room.id,
                                           user: this.id,
                                           type: 'unpublish',
                                           stream: streamId,
                                           timestamp: timeStamp.getTime()});
            }
        }
    }

    let index = this.streams.indexOf(streamId);
    if (index !== -1) {
        this.streams.splice(index, 1);
    }
    this.room.removeStream(streamId);
    callback(true);
  }

  onUnsubscribe(to, callback) {
    if (this.user === undefined || !this.user.permissions[Permission.SUBSCRIBE]) {
        if (callback) callback(null, 'Unauthorized');
        return;
    }

    let stream = this.room.getStreamById(to);
    if (stream === undefined) {
        return;
    }

    stream.removeDataSubscriber(this.id);

    if (stream.hasAudio() || stream.hasVideo() || stream.hasScreen()) {
        if (this.room.p2p) {
            const clientId = stream.getClient();
            const client = this.room.getClientById(clientId);
            client.sendMessage('unpublish_me', {streamId: stream.getID(), peerSocket: this.id});
        } else {
            this.room.controller.removeSubscriber(this.id, to);
            if (global.config.erizoController.report.session_events) {  // jshint ignore:line
                var timeStamp = new Date();
                this.room.amqper.broadcast('event', {room: this.room.id,
                                           user: this.id,
                                           type: 'unsubscribe',
                                           stream: to,
                                           timestamp: timeStamp.getTime()});
            }
        }
    }
    callback(true);
  }

  onDisconnect() {
    let timeStamp = new Date();

    log.info('message: Channel disconnect, clientId: ' + this.id, ', channelId:', this.channel.id);

    for (let streamId of this.streams) {
      this.room.sendMessage('onRemoveStream', {id: streamId});
    }

    if (this.room !== undefined) {
      this.room.forEachStream((stream) => {
        stream.removeDataSubscriber(this.id);
        if (this.room.p2p) {
          const clientId = stream.getClient();
          const client = this.room.getClientById(clientId);
          client.sendMessage('unpublish_me', {streamId: stream.getID(), peerSocket: this.id});
        }
      });

      this.room.removeClient(this.id);

      if (this.room.controller) {
          this.room.controller.removeSubscriptions(this.id);
      }

      for (let streamId of this.streams) {
        let stream = this.room.getStreamById(streamId);
        if (stream !== undefined) {
          if (stream.hasAudio() || stream.hasVideo() || stream.hasScreen()) {
            if (!this.room.p2p) {
              log.info('message: Unpublishing stream, streamId:', streamId);
              this.room.controller.removePublisher(streamId);
              if (global.config.erizoController.report.session_events) {  // jshint ignore:line

                this.room.amqper.broadcast('event', {room: this.room.id,
                                           user: this.id,
                                           type: 'unpublish',
                                           stream: streamId,
                                           timestamp: timeStamp.getTime()});
              }
            }
          }
          this.room.removeStream(streamId);
        }
      }
      if (!this.room.p2p &&
          global.config.erizoController.report.session_events) {  // jshint ignore:line
          this.room.amqper.broadcast('event', {room: this.room.id,
                                    user: this.id,
                                    type: 'user_disconnection',
                                    timestamp: timeStamp.getTime()});
      }

      this.emit('disconnect');
    }
  }

  onGetStreamStats(streamId, callback) {
    log.debug('message: getting stats, streamId: ' + streamId);
    if (this.user === undefined || !this.user.permissions[Permission.STATS]) {
        log.info('message: unauthorized getStreamStats request');
        if (callback) callback(null, 'Unauthorized');
        return;
    }
    if (this.room.getStreamById(streamId) === undefined) {
        log.info('message: bad getStreamStats request');
        return;
    }
    if (this.room !== undefined && !this.room.p2p) {
        this.room.controller.getStreamStats(streamId, function (result) {
            callback(result);
        });
    }
  }

}

exports.Client = Client;
