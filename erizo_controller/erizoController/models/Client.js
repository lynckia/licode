/* eslint-disable no-param-reassign */

const events = require('events');
// eslint-disable-next-line import/no-extraneous-dependencies
const uuidv4 = require('uuid/v4');
const Permission = require('../permission');
const PublishedStream = require('./Stream').PublishedStream;
const StreamStates = require('./Stream').StreamStates;
const logger = require('./../../common/logger').logger;

const log = logger.getLogger('ErizoController - Client');

class Client extends events.EventEmitter {
  constructor(channel, token, options, room) {
    super();
    this.channel = channel;
    this.room = room;
    this.token = token;
    this.id = uuidv4();
    this.options = options;
    this.options.streamPriorityStrategy =
      Client.getStreamPriorityStrategy(options.streamPriorityStrategy);
    this.socketEventListeners = new Map();
    this.listenToSocketEvents();
    this.user = { name: token.userName, role: token.role, permissions: {} };
    const permissions = global.config.erizoController.roles[token.role] || {};
    Object.keys(permissions).forEach((right) => {
      this.user.permissions[right] = permissions[right];
    });
    this.state = 'sleeping'; // ?
  }

  listenToSocketEvents() {
    log.debug(`message: Adding listeners to socket events, client.id: ${this.id}`);
    this.socketEventListeners.set('sendDataStream', this.onSendDataStream.bind(this));
    this.socketEventListeners.set('connectionMessage', this.onConnectionMessage.bind(this));
    this.socketEventListeners.set('streamMessage', this.onStreamMessage.bind(this));
    this.socketEventListeners.set('streamMessageP2P', this.onStreamMessageP2P.bind(this));
    this.socketEventListeners.set('updateStreamAttributes', this.onUpdateStreamAttributes.bind(this));
    this.socketEventListeners.set('publish', this.onPublish.bind(this));
    this.socketEventListeners.set('subscribe', this.onSubscribe.bind(this));
    this.socketEventListeners.set('startRecorder', this.onStartRecorder.bind(this));
    this.socketEventListeners.set('stopRecorder', this.onStopRecorder.bind(this));
    this.socketEventListeners.set('unpublish', this.onUnpublish.bind(this));
    this.socketEventListeners.set('unsubscribe', this.onUnsubscribe.bind(this));
    this.socketEventListeners.set('autoSubscribe', this.onAutoSubscribe.bind(this));
    this.socketEventListeners.set('getStreamStats', this.onGetStreamStats.bind(this));
    this.socketEventListeners.set('clientDisconnection', this.onClientDisconnection.bind(this));
    this.socketEventListeners.set('setStreamPriorityStrategy', this.onSetStreamPriorityStrategy.bind(this));
    this.socketEventListeners.forEach((value, key) => {
      this.channel.socketOn(key, value);
    });
    this.channel.on('disconnect', this.onDisconnect.bind(this));
  }
  stopListeningToSocketEvents() {
    log.debug(`message: Removing listeners to socket events, client.id: ${this.id}`);
    this.socketEventListeners.forEach((value, key) => {
      this.channel.socketRemoveListener(key, value);
    });
  }
  static getStreamPriorityStrategy(streamPriorityStrategy) {
    log.debug(`message: getting streamPriorityStrategy configuration, streamPriorityStrategy: ${streamPriorityStrategy}`);
    const isStrategyIsDefined = streamPriorityStrategy
      && global.bwDistributorConfig.strategyDefinitions
      && !!global.bwDistributorConfig.strategyDefinitions[streamPriorityStrategy];
    log.debug(`message: getting streamPriorityStrategy configuration, streamPriorityStrategy: ${streamPriorityStrategy}, is present = ${isStrategyIsDefined}`);
    return isStrategyIsDefined ? streamPriorityStrategy : false;
  }

  disconnect() {
    this.stopListeningToSocketEvents();
    this.channel.disconnect();
  }

  getChannel() {
    return this.channel;
  }

  setNewChannel(channel) {
    const oldChannel = this.channel;
    const buffer = oldChannel.getBuffer();
    log.info('message: reconnected, oldChannelId:', oldChannel.id, ', channelId:', channel.id, ',', logger.objectToLog(this.token));
    oldChannel.removeAllListeners();
    oldChannel.disconnect();
    this.channel = channel;
    this.listenToSocketEvents();
    this.channel.sendBuffer(buffer);
  }

  setSelectors(selectors, negativeSelectors, options) {
    this.selectors = selectors;
    this.negativeSelectors = negativeSelectors;
    this.selectorOptions = options;
    this.onInternalAutoSubscriptionChange();
  }

  onInternalAutoSubscriptionChange() {
    if (!this.selectors && !this.negativeSelectors) {
      return;
    }
    const subscribableStreams = [];
    const unsubscribableStreams = [];
    this.room.streamManager.forEachPublishedStream((stream) => {
      // We don't subscribe/unsubscribe to own published
      if (stream.getClientId() === this.id) {
        return;
      }
      if (stream.meetAnySelector(this.selectors) &&
          !stream.meetAnySelector(this.negativeSelectors)) {
        if (stream.hasData() && this.options.data !== false) {
          stream.addDataSubscriber(this.id);
        }
        if (stream.hasAudio() || stream.hasVideo() || stream.hasScreen()) {
          subscribableStreams.push(stream);
        }
      } else {
        if (stream.hasData() && this.options.data !== false) {
          stream.removeDataSubscriber(this.id);
        }
        if (stream.hasAudio() || stream.hasVideo() || stream.hasScreen()) {
          unsubscribableStreams.push(stream);
        }
      }
    });
    if (subscribableStreams.length > 0) {
      this.onMultipleSubscribe(subscribableStreams, this.selectorOptions);
    }
    if (unsubscribableStreams.length > 0) {
      this.onMultipleUnsubscribe(unsubscribableStreams);
    }
  }

  onMultipleSubscribe(streams, options = {}) {
    if (this.room.p2p) {
      streams.forEach((stream) => {
        const clientId = stream.getClientId();
        const client = this.room.getClientById(clientId);
        client.sendMessage('publish_me', { streamId: stream.getID(), peerSocket: this.id });
      });
      return;
    }
    log.info('message: addMultipleSubscribers requested, ' +
                 `streams: ${streams}, ` +
                 `clientId: ${this.id},`,
    logger.objectToLog(this.token));
    options.mediaConfiguration = this.token.mediaConfiguration;
    options.singlePC = this.options.singlePC || false;
    let streamIds = streams.map(stream => stream.getID());
    streamIds = streamIds.filter(streamId =>
      this.room.streamManager.hasPublishedStream(streamId) &&
      !this.room.streamManager.getPublishedStreamById(streamId).hasAvSubscriber(this.id));
    streamIds.forEach((streamId) => {
      const publishedStream = this.room.streamManager.getPublishedStreamById(streamId);
      publishedStream.addAvSubscriber(this.id);
    });

    this.room.controller.addMultipleSubscribers(this.id, streamIds, options, (signMess) => {
      // We can receive multiple initializing messages with subsets of streamIds. Each subset
      // is sent from a single ErizoJS.
      if (signMess.type === 'multiple-initializing') {
        log.info('message: addMultipleSubscribers, ' +
                         'state: SUBSCRIBER_INITIAL, ' +
                         `clientId: ${this.id}, ` +
                         `streamIds: ${signMess.streamIds},`,
        logger.objectToLog(this.token));
        const initializingStreamIds = [];
        if (signMess.streamIds) {
          signMess.streamIds.forEach((streamId) => {
            if (this.room.streamManager.hasPublishedStream(streamId)) {
              this.room.streamManager
                .getPublishedStreamById(streamId)
                .updateAvSubscriberState(this.id, StreamStates.SUBSCRIBER_INITIAL);
              initializingStreamIds.push(streamId);
            }
          });
        }
        if (global.config.erizoController.report.session_events) {
          const timeStamp = new Date();
          initializingStreamIds.forEach((streamId) => {
            this.room.amqper.broadcast('event', { room: this.room.id,
              user: this.id,
              name: this.user.name,
              type: 'subscribe',
              stream: streamId,
              timestamp: timeStamp.getTime() });
          });
        }
      } else if (signMess.type === 'failed') {
        // TODO: Add Stats event
        log.warn('message: addMultipleSubscribers ICE Failed, ' +
                         'state: SUBSCRIBER_FAILED, ' +
                         `streamId: ${signMess.streamId}, ` +
                         `clientId: ${this.id},`,
        logger.objectToLog(this.token));
        if (this.room.streamManager.hasPublishedStream(signMess.streamId)) {
          this.room.streamManager.getPublishedStreamById(signMess.streamId)
            .removeAvSubscriber(this.id);
        }
        this.sendMessage('connection_failed', { type: 'subscribe',
          streamId: signMess.streamId });
        return;
      } else if (signMess.type === 'ready') {
        log.info('message: addMultipleSubscribers, ' +
                         'state: SUBSCRIBER_READY, ' +
                         `streamId: ${signMess.streamId}, ` +
                         `clientId: ${this.id},`,
        logger.objectToLog(this.token));
        if (this.room.streamManager.hasPublishedStream(signMess.streamId)) {
          this.room.streamManager
            .getPublishedStreamById(signMess.streamId)
            .updateAvSubscriberState(this.id, StreamStates.SUBSCRIBER_READY);
        }
      } else if (signMess.type === 'bandwidthAlert') {
        this.sendMessage('onBandwidthAlert', { streamID: signMess.streamId,
          message: signMess.message,
          bandwidth: signMess.bandwidth });
        return;
      } else if (signMess === 'timeout') {
        log.error('message: addMultipleSubscribers timeout when contacting ErizoJS, ' +
                          `streamId: ${signMess.streamId}, ` +
                          `clientId: ${this.id},`,
        logger.objectToLog(this.token));
        if (this.room.streamManager.hasPublishedStream(signMess.streamId)) {
          this.room.streamManager.getPublishedStreamById(signMess.streamId)
            .removeAvSubscriber(this.id);
        }
        return;
      }

      this.sendMessage('stream_message_erizo', { mess: signMess,
        options,
        context: signMess.context,
        peerIds: signMess.streamIds });
    });
  }

  onMultipleUnsubscribe(streams) {
    if (this.room.p2p) {
      streams.forEach((stream) => {
        const clientId = stream.getClientId();
        const client = this.room.getClientById(clientId);
        client.sendMessage('unpublish_me', { streamId: stream.getID(), peerSocket: this.id });
      });
      return;
    }
    let streamIds = streams.map(stream => stream.getID());
    streamIds = streamIds.filter(streamId => this.room.streamManager.hasPublishedStream(streamId) &&
      this.room.streamManager.getPublishedStreamById(streamId).hasAvSubscriber(this.id));
    log.info('message: removeMultipleSubscribers requested, ' +
      `streamIds: ${streamIds}, ` +
      `clientId: ${this.id},`,
    logger.objectToLog(this.token));
    if (streamIds.length === 0) {
      return;
    }

    this.room.controller.removeMultipleSubscribers(this.id, streamIds, (signMess) => {
      signMess.streamIds.forEach((streamId) => {
        this.room.streamManager.getPublishedStreamById(streamId).removeAvSubscriber(this.id);
      });
      if (global.config.erizoController.report.session_events) {
        if (signMess === 'timeout') {
          log.error('message: removeMultipleSubscribers timeout when contacting ErizoJS, ' +
                            `streamId: ${signMess.streamId}, ` +
                            `clientId: ${this.id},`,
          logger.objectToLog(this.token));
          return;
        }

        const timeStamp = new Date();
        signMess.streamIds.forEach((streamId) => {
          this.room.amqper.broadcast('event', { room: this.room.id,
            user: this.id,
            type: 'unsubscribe',
            stream: streamId,
            timestamp: timeStamp.getTime() });
        });
      }

      this.sendMessage('stream_message_erizo', { mess: signMess,
        options: {},
        context: signMess.context,
        peerIds: signMess.streamIds });
    });
  }

  sendMessage(type, arg) {
    this.channel.sendMessage(type, arg);
  }

  hasPermission(action, options = false) {
    if (this.user === undefined || !this.user.permissions[action]) {
      return false;
    }

    if (options && this.user.permissions[action] !== true) {
      const permissions = this.user.permissions[action];
      const result = Object.keys(permissions).every((permissionAction) => {
        if ((options[permissionAction] === true) && (permissions[permissionAction] === false)) {
          return false;
        }
        return true;
      });
      return result;
    }
    return true;
  }

  onSendDataStream(message, cb) {
    const stream = this.room.streamManager.getPublishedStreamById(message.id);
    if (stream === undefined) {
      log.warn('message: Trying to send Data from a non-initialized stream, ' +
               `clientId: ${this.id},`, logger.objectToLog(message));
      cb();
      return;
    }
    stream.forEachDataSubscriber((index, dataSubscriber) => {
      const client = this.room.getClientById(dataSubscriber);
      if (client) {
        log.debug('message: sending dataStream, ' +
          `clientId: ${dataSubscriber}, dataStream: ${message.id},`,
        logger.objectToLog(this.token));
        client.sendMessage('onDataStream', message);
      }
    });
    cb();
  }

  onStreamMessageP2P({ options }, cb) {
    const message = options;
    if (this.room === undefined) {
      log.error('message: streamMessageP2P for user in undefined room' +
        `, streamId: ${message.streamId}, user: ${this.user},`,
      logger.objectToLog(this.token));
      cb();
      this.disconnect();
      return;
    }
    if (!this.room.p2p) {
      log.error('message: streamMessageP2P for user in non p2p room' +
        `, streamId: ${message.streamId}, user: ${this.user},`,
      logger.objectToLog(this.token));
      cb();
      return;
    }
    const targetClient = this.room.getClientById(message.peerSocket);
    if (targetClient) {
      targetClient.sendMessage('stream_message_p2p',
        { streamId: message.streamId,
          peerSocket: this.id,
          msg: message.msg });
    }
    cb();
  }

  onConnectionMessage({ options }, cb) {
    if (this.room === undefined) {
      log.error('message: connectionMessage for user in undefined room' +
        `, connectionId: ${options.connectionId}, user: ${this.user},`,
      logger.objectToLog(this.token));
      this.disconnect();
      return;
    }
    if (this.room.p2p) {
      log.error('options: connectionMessage for user in p2p room' +
        `, connectionId: ${options.connectionId}, user: ${this.user},`,
      logger.objectToLog(this.token));
      return;
    }
    const callback = (result) => {
      let type = options && options.msg && options.msg.type;
      type = type || 'unknown';
      if (result.error && type === 'offer') {
        this.sendMessage('connection_message_erizo', {
          connectionId: options.connectionId,
          info: 'error',
          evt: { type: 'error', previousType: 'offer' },
        });
      }
    };
    this.room.controller.processConnectionMessageFromClient(options.erizoId, this.id,
      options.connectionId, options.msg, callback.bind(this));
    cb();
  }

  onStreamMessage({ options }, cb) {
    const message = options;
    if (this.room === undefined) {
      log.error('message: streamMessage for user in undefined room' +
        `, streamId: ${message.streamId}, user: ${this.user},`,
      logger.objectToLog(this.token));
      this.disconnect();
      return;
    }
    if (this.room.p2p) {
      log.error('message: streamMessage for user in p2p room' +
        `, streamId: ${message.streamId}, user: ${this.user},`,
      logger.objectToLog(this.token));
      return;
    }
    const isControlMessage = message.msg.type === 'control';
    if (!isControlMessage ||
          (isControlMessage && this.hasPermission(message.msg.action.name))) {
      this.room.controller.processStreamMessageFromClient(message.erizoId, this.id,
        message.streamId, message.msg);
    } else {
      log.info('message: User unauthorized to execute action on stream, action: ' +
        `${message.msg.action.name}, streamId: ${message.streamId},`,
      logger.objectToLog(this.token));
    }
    cb();
  }

  onUpdateStreamAttributes(message, cb) {
    const stream = this.room.streamManager.getPublishedStreamById(message.id);
    if (stream === undefined) {
      log.warn('message: Update attributes to a uninitialized stream,',
        logger.objectToLog(message));
      return;
    }
    stream.setAttributes(message.attrs);
    stream.forEachDataSubscriber((index, dataSubscriber) => {
      const client = this.room.getClientById(dataSubscriber);
      if (client) {
        log.debug('message: Sending new attributes, ' +
                      `clientId: ${dataSubscriber}, streamId: ${message.id},`,
        logger.objectToLog(this.token));
        client.sendMessage('onUpdateAttributeStream', message);
      }
    });
    this.room.forEachClient((client) => {
      client.onInternalAutoSubscriptionChange();
    });
    cb();
  }

  _publishExternalInput(id, options, sdp, callback) {
    let url = sdp;
    if (options.state === 'recording') {
      const recordingId = sdp;
      if (global.config.erizoController.recording_path) {
        url = `${global.config.erizoController.recording_path + recordingId}.mkv`;
      } else {
        url = `/tmp/${recordingId}.mkv`;
      }
    }
    const st = new PublishedStream({ id,
      client: this.id,
      audio: options.audio,
      video: options.video,
      data: options.data,
      label: options.label,
      attributes: options.attributes });
    this.room.streamManager.addPublishedStream(id, st);
    this.room.controller.addExternalInput(this.id, id, url, options.label, (result) => {
      if (result === 'success') {
        st.updateStreamState(StreamStates.PUBLISHER_READY);
        callback(id);
        this.room.sendMessage('onAddStream', st.getPublicStream());
      } else {
        this.room.streamManager.removePublishedStream(id);
        callback(null, `Error adding External Input:${result}`);
      }
    });
  }

  _publishErizo(id, options, sdp, callback) {
    options.mediaConfiguration = this.token.mediaConfiguration;
    options.singlePC = this.options.singlePC || false;
    options.streamPriorityStrategy = this.options.streamPriorityStrategy;
    log.info('message: addPublisher requested, ',
      `streamId: ${id}, clientId: ${this.id},`,
      logger.objectToLog(options),
      logger.objectToLog(options.metadata),
    );
    const st = new PublishedStream({ id,
      client: this.id,
      audio: options.audio,
      video: options.video,
      data: options.data,
      label: options.label,
      screen: options.screen,
      attributes: options.attributes });
    this.room.streamManager.addPublishedStream(id, st);
    this.room.controller.addPublisher(this.id, id, options, (signMess) => {
      if (!this.room.streamManager.hasPublishedStream(id)) {
        log.warn(`message: addPublisher of removed publisher, messageType: ${signMess.type},` +
          `label: ${options.label}, clientId: ${this.id}, streamId: ${id},`,
        logger.objectToLog(this.token));
        return;
      }
      if (signMess.type === 'initializing') {
        callback(id, signMess.erizoId, signMess.connectionId);
        st.updateStreamState(StreamStates.PUBLISHER_INITAL);
        log.info('message: addPublisher, ' +
          `label: ${options.label}, ` +
          'state: PUBLISHER_INITIAL, ' +
          `clientId: ${this.id}, ` +
          `streamId: ${id},`,
        logger.objectToLog(this.token));

        if (global.config.erizoController.report.session_events) {
          const timeStamp = new Date();
          this.room.amqper.broadcast('event', { room: this.room.id,
            user: this.id,
            name: this.user.name,
            type: 'publish',
            stream: id,
            timestamp: timeStamp.getTime(),
            agent: signMess.agentId,
            attributes: options.attributes });
        }
        return;
      } else if (signMess.type === 'started') {
        return;
      } else if (signMess.type === 'failed') {
        log.warn('message: addPublisher ICE Failed, ' +
          'state: PUBLISHER_FAILED, ' +
          `streamId: ${id}, ` +
          `clientId: ${this.id},`,
        logger.objectToLog(this.token));
        this.sendMessage('connection_failed', { type: 'publish', streamId: id });
        this.room.streamManager.removePublishedStream(id);
        // We're going to let the client disconnect
        return;
      } else if (signMess.type === 'ready') {
        st.updateStreamState(StreamStates.PUBLISHER_READY);
        this.room.forEachClient((client) => {
          client.onInternalAutoSubscriptionChange();
        });
        this.room.sendMessage('onAddStream', st.getPublicStream());
        log.info('message: addPublisher, ' +
          'state: PUBLISHER_READY, ' +
          `streamId: ${id}, ` +
          `clientId: ${this.id},`,
        logger.objectToLog(this.token));
        // We're going to let the client disconnect
        return;
      } else if (signMess === 'timeout-erizojs') {
        log.error('message: addPublisher timeout when contacting ErizoJS, ' +
          `streamId: ${id}, clientId: ${this.id},`,
        logger.objectToLog(this.token));
        this.room.streamManager.removePublishedStream(id);
        callback(null, null, 'ErizoJS is not reachable');
        return;
      } else if (signMess === 'timeout-erizojs-retry') {
        log.warn('message: addPublisher timeout when contacting ErizoJS but will try again, ' +
          `streamId: ${id}, clientId: ${this.id},`,
        logger.objectToLog(this.token));
        st.updateStreamState(StreamStates.PUBLISHER_CREATED);
        return;
      } else if (signMess === 'timeout-agent') {
        log.error('message: addPublisher timeout when contacting Agent, ' +
          `streamId: ${id}, clientId: ${this.id},`,
        logger.objectToLog(this.token));
        this.room.streamManager.removePublishedStream(id);
        callback(null, null, 'ErizoAgent is not reachable');
        return;
      } else if (signMess === 'timeout') {
        log.error('message: addPublisher Undefined RPC Timeout, ' +
          `streamId: ${id}, clientId: ${this.id},`,
        logger.objectToLog(this.token));
        this.room.streamManager.removePublishedStream(id);
        callback(null, null, 'ErizoAgent or ErizoJS is not reachable');
        return;
      }
      log.debug('Sending message back to the client', id);
      this.sendMessage('stream_message_erizo', { mess: signMess, streamId: id });
    });
  }

  _publishP2P(id, options, sdp, callback) {
    const st = new PublishedStream({ id,
      client: this.id,
      audio: options.audio,
      video: options.video,
      data: options.data,
      label: options.label,
      screen: options.screen,
      attributes: options.attributes });
    this.room.streamManager.addPublishedStream(id, st);
    st.updateStreamState(StreamStates.PUBLISHER_READY);
    callback(id);
    this.room.sendMessage('onAddStream', st.getPublicStream());
  }

  onPublish({ options, sdp }, callback) {
    if (!this.hasPermission(Permission.PUBLISH, options)) {
      callback(null, 'Unauthorized');
      return;
    }

    // generate a 18 digits safe integer
    const id = Math.floor(100000000000000000 + (Math.random() * 900000000000000000));

    if (options.state === 'url' || options.state === 'recording') {
      this._publishExternalInput(id, options, sdp, callback);
    } else if (options.state === 'erizo') {
      this._publishErizo(id, options, sdp, callback);
    } else {
      this._publishP2P(id, options, sdp, callback);
    }
  }

  onSubscribe({ options }, callback) {
    if (!this.hasPermission(Permission.SUBSCRIBE, options)) {
      callback(null, 'Unauthorized');
      return;
    }

    const stream = this.room.streamManager.getPublishedStreamById(options.streamId);
    if (stream === undefined) {
      log.warn('message: addSubscriber can not be requested, ',
        'reason: publisher not found',
        `streamId: ${options.streamId}, `,
        `clientId: ${this.id},`,
        logger.objectToLog(options),
        logger.objectToLog(options.metadata));
      return;
    }
    if (stream.hasData() && options.data !== false) {
      stream.addDataSubscriber(this.id);
    }

    if (stream.hasAudio() || stream.hasVideo() || stream.hasScreen()) {
      if (this.room.p2p) {
        const clientId = stream.getClientId();
        const client = this.room.getClientById(clientId);
        stream.addAvSubscriber(this.id);
        stream.updateAvSubscriberState(this.id, StreamStates.SUBSCRIBER_READY);
        client.sendMessage('publish_me', { streamId: options.streamId, peerSocket: this.id });
      } else {
        if (stream.hasAvSubscriber(this.id)) {
          log.warn('message: addSubscriber can not be requested, ',
            'reason: this client is already subscribed',
            `streamId: ${options.streamId}, `,
            `clientId: ${this.id},`,
            logger.objectToLog(options),
            logger.objectToLog(options.metadata));
          return;
        }

        log.info('message: addSubscriber requested, ' +
                     `streamId: ${options.streamId}, ` +
                     `clientId: ${this.id},`,
        logger.objectToLog(this.token));
        options.mediaConfiguration = this.token.mediaConfiguration;
        options.singlePC = this.options.singlePC || false;
        options.streamPriorityStrategy = this.options.streamPriorityStrategy;
        stream.addAvSubscriber(this.id);
        this.room.controller.addSubscriber(this.id, options.streamId, options, (signMess) => {
          if (!this.room.streamManager.hasPublishedStream(options.streamId)
              || !stream.hasAvSubscriber(this.id)) {
            log.warn(`message: addSubscriber of removed subscriber, messageType: ${signMess.type},` +
              `clientId: ${this.id}, streamId: ${options.streamId},`,
            logger.objectToLog(this.token));
            return;
          }
          if (signMess.type === 'initializing') {
            log.info('message: addSubscriber, ' +
                             'state: SUBSCRIBER_INITIAL, ' +
                             `clientId: ${this.id}, ` +
                             `streamId: ${options.streamId},`,
            logger.objectToLog(this.token));
            stream.updateAvSubscriberState(this.id, StreamStates.SUBSCRIBER_INITIAL);
            callback(true, signMess.erizoId, signMess.connectionId);
            if (global.config.erizoController.report.session_events) {
              const timeStamp = new Date();
              this.room.amqper.broadcast('event', { room: this.room.id,
                user: this.id,
                name: this.user.name,
                type: 'subscribe',
                stream: options.streamId,
                timestamp: timeStamp.getTime() });
            }
            return;
          } else if (signMess.type === 'failed') {
            // TODO: Add Stats event
            log.warn('message: addSubscriber ICE Failed, ' +
                             'state: SUBSCRIBER_FAILED, ' +
                             `streamId: ${options.streamId}, ` +
                             `clientId: ${this.id},`,
            logger.objectToLog(this.token));
            this.sendMessage('connection_failed', { type: 'subscribe',
              streamId: options.streamId });
            stream.removeAvSubscriber(this.id);
            return;
          } else if (signMess.type === 'started') {
            return;
          } else if (signMess.type === 'ready') {
            log.info('message: addSubscriber, ' +
                             'state: SUBSCRIBER_READY, ' +
                             `streamId: ${options.streamId}, ` +
                             `clientId: ${this.id},`,
            logger.objectToLog(this.token));
            stream.updateAvSubscriberState(this.id, StreamStates.SUBSCRIBER_READY);
            return;
          } else if (signMess.type === 'bandwidthAlert') {
            this.sendMessage('onBandwidthAlert', { streamID: options.streamId,
              message: signMess.message,
              bandwidth: signMess.bandwidth });
          } else if (signMess === 'timeout') {
            log.error('message: addSubscriber timeout when contacting ErizoJS, ' +
                              `streamId: ${options.streamId}, ` +
                              `clientId: ${this.id},`,
            logger.objectToLog(this.token));
            stream.removeAvSubscriber(this.id);
            callback(null, null, 'ErizoJS is not reachable');
            return;
          }
          this.sendMessage('stream_message_erizo', { mess: signMess,
            peerId: options.streamId });
        });
      }
    } else {
      callback(true);
    }
  }

  onStartRecorder(options, callback) {
    if (!this.hasPermission(Permission.RECORD)) {
      callback(null, 'Unauthorized');
      return;
    }
    const streamId = options.to;
    const recordingId = Math.random() * 1000000000000000000;
    let url;

    if (global.config.erizoController.recording_path) {
      url = `${global.config.erizoController.recording_path + recordingId}.mkv`;
    } else {
      url = `/tmp/${recordingId}.mkv`;
    }

    log.info('message: startRecorder, ' +
             'state: RECORD_REQUESTED, ' +
             `streamId: ${streamId}, ` +
             `url: ${url},`,
    logger.objectToLog(this.token));

    if (this.room.p2p) {
      callback(null, 'Stream can not be recorded');
      return;
    }

    const stream = this.room.streamManager.getPublishedStreamById(streamId);
    if (!stream) {
      log.warn('message: startRecorder stream not found, ' +
        'state: RECORD_FAILED, ' +
        `streamId: ${streamId}, ` +
        `url: ${url},`,
      logger.objectToLog(this.token));
      callback(null, 'Unable to subscribe to stream for recording, ' +
        'publisher not present');
      return;
    }
    if (stream.hasAudio() || stream.hasVideo() || stream.hasScreen()) {
      const mediaOptions = { mediaConfiguration: this.token.mediaConfiguration };
      stream.addExternalOutputSubscriber(url);
      stream.updateExternalOutputSubscriberState(url, StreamStates.SUBSCRIBER_CREATED);
      this.room.controller.addExternalOutput(streamId, url, mediaOptions, () => {
        log.info('message: startRecorder, ' +
          'state: RECORD_STARTED, ' +
          `streamId: ${streamId}, ` +
          `url: ${url},`,
        logger.objectToLog(this.token));
        callback(recordingId);
        stream.updateExternalOutputSubscriberState(url, StreamStates.SUBSCRIBER_READY);
      });
    } else {
      log.warn('message: startRecorder stream cannot be recorded, ' +
        'state: RECORD_FAILED, ' +
        `streamId: ${streamId}, ` +
        `url: ${url},`,
      logger.objectToLog(this.token));
      callback(null, 'Stream can not be recorded');
    }
  }

  onStopRecorder(options, callback) {
    if (!this.hasPermission(Permission.RECORD)) {
      if (callback) callback(null, 'Unauthorized');
      return;
    }
    const recordingId = options.id;
    let url;

    if (global.config.erizoController.recording_path) {
      url = `${global.config.erizoController.recording_path + recordingId}.mkv`;
    } else {
      url = `/tmp/${recordingId}.mkv`;
    }


    log.info('message: stopRecorder requested, ' +
             `recordingId: ${options.id}, ` +
             `url: ${url},`,
    logger.objectToLog(this.token));
    let removed = false;
    this.room.streamManager.forEachPublishedStream((stream) => {
      if (stream.hasExternalOutputSubscriber(url)) {
        stream.removeExternalOutputSubscriber(url);
        this.room.controller.removeExternalOutput(options.id, url, callback);
        removed = true;
      }
    });
    if (removed) {
      callback(true);
    } else {
      log.warn('message: removeExternalOutput no publisher, ' +
                     `recordingId: ${recordingId}, ` +
                     `url: ${url},`,
      logger.objectToLog(this.token));
      callback(null, 'This stream is not being recorded');
    }
  }

  onUnpublish(streamId, callback) {
    if (!this.hasPermission(Permission.PUBLISH)) {
      if (callback) callback(null, 'Unauthorized');
      return;
    }

    // Stream has been already deleted or it does not exist
    const stream = this.room.streamManager.getPublishedStreamById(streamId);
    if (stream === undefined) {
      log.warn('message: onUnpublish - Publisher not found, ' +
                     `clientId: ${this.id}, ` +
                     `streamId: ${streamId},`,
      logger.objectToLog(this.token));
      return;
    }

    stream.updateStreamState(StreamStates.PUBLISHER_REQUESTED_CLOSE);

    if (stream.hasAudio() || stream.hasVideo() || stream.hasScreen()) {
      this.state = 'sleeping';
      if (!this.room.p2p) {
        this.room.controller.removePublisher(this.id, streamId, () => {
          if (global.config.erizoController.report.session_events) {
            const timeStamp = new Date();
            this.room.amqper.broadcast('event', { room: this.room.id,
              user: this.id,
              type: 'unpublish',
              stream: streamId,
              timestamp: timeStamp.getTime() });
          }
          this.room.streamManager.removePublishedStream(streamId);
          this.room.sendMessage('onRemoveStream', { id: streamId });
          callback(true);
        });
      } else {
        this.room.sendMessage('onRemoveStream', { id: streamId });
        this.room.streamManager.removePublishedStream(streamId);
        callback(true);
      }
    } else {
      this.room.streamManager.removePublishedStream(streamId);
      callback(true);
    }
  }

  onUnsubscribe(streamId, callback) {
    if (!this.hasPermission(Permission.SUBSCRIBE)) {
      if (callback) callback(null, 'Unauthorized');
      return;
    }

    const stream = this.room.streamManager.getPublishedStreamById(streamId);
    if (stream === undefined) {
      log.warn('message: onUnsubscribe - Publisher not found, ' +
                     `clientId: ${this.id}, ` +
                     `streamId: ${streamId}`,
      logger.objectToLog(this.token));
      return;
    }

    stream.removeDataSubscriber(this.id);

    if (stream.hasAudio() || stream.hasVideo() || stream.hasScreen()) {
      if (!stream.hasAvSubscriber(this.id)) {
        log.warn('message: onUnsubscribe - Publisher not found, ' +
          `clientId: ${this.id}, ` +
          `streamId: ${streamId},`,
        logger.objectToLog(this.token));
        return;
      }
      stream.updateAvSubscriberState(this.id, StreamStates.SUBSCRIBER_REQUESTED_CLOSE);
      if (this.room.p2p) {
        const clientId = stream.getClientId();
        const client = this.room.getClientById(clientId);
        stream.removeAvSubscriber(this.id);
        client.sendMessage('unpublish_me', { streamId: stream.getID(), peerSocket: this.id });
        callback(true);
      } else {
        this.room.controller.removeSubscriber(this.id, streamId, (result) => {
          if (global.config.erizoController.report.session_events) {
            const timeStamp = new Date();
            this.room.amqper.broadcast('event', { room: this.room.id,
              user: this.id,
              type: 'unsubscribe',
              stream: streamId,
              timestamp: timeStamp.getTime() });
          }
          stream.removeAvSubscriber(this.id);
          callback(result);
        });
      }
    }
  }

  onAutoSubscribe(data, callback = () => {}) {
    if (!this.hasPermission(Permission.SUBSCRIBE)) {
      if (callback) callback(null, 'Unauthorized');
      return;
    }

    const selectors = (data && data.selectors) || {};
    const negativeSelectors = (data && data.negativeSelectors) || {};
    const options = (data && data.options) || {};

    this.setSelectors(selectors, negativeSelectors, options);
    callback();
  }

  onClientDisconnection() {
    log.info(`message: Client requests disconnection, clientId: ${this.id},`,
      logger.objectToLog(this.token));
    this.channel.clientWillDisconnect();
  }

  onSetStreamPriorityStrategy(strategyId, callback = () => {}) {
    this.options.streamPriorityStrategy =
      Client.getStreamPriorityStrategy(strategyId);
    this.room.amqper.broadcast('ErizoJS', { method: 'setClientStreamPriorityStrategy', args: [this.id, strategyId] });
    callback();
  }

  onDisconnect() {
    this.stopListeningToSocketEvents();
    const timeStamp = new Date();

    log.info(`message: Channel disconnect, clientId: ${this.id}`, ', channelId:', this.channel.id,
      ',', logger.objectToLog(this.token));

    this.room.streamManager.forEachPublishedStream((stream) => {
      if (stream.getClientId() === this.id) {
        this.room.sendMessage('onRemoveStream', { id: stream.id });
      }
    });

    if (this.room !== undefined) {
      this.room.streamManager.forEachPublishedStream((stream) => {
        stream.removeDataSubscriber(this.id);
        if (this.room.p2p) {
          const clientId = stream.getClientId();
          const client = this.room.getClientById(clientId);
          client.sendMessage('unpublish_me', { streamId: stream.getID(), peerSocket: this.id });
        }
      });

      this.room.streamManager.forEachPublishedStream((stream) => {
        if (stream.hasAvSubscriber(this.id)) {
          stream.removeAvSubscriber(this.id);
        }
      });

      this.room.streamManager.forEachPublishedStream((stream) => {
        if (stream.getClientId() === this.id) {
          if (stream.hasAudio() || stream.hasVideo() || stream.hasScreen()) {
            if (!this.room.p2p) {
              log.info('message: Unpublishing stream, streamId:', stream.id, ',',
                logger.objectToLog(this.token));
              if (global.config.erizoController.report.session_events) {
                this.room.amqper.broadcast('event', { room: this.room.id,
                  user: this.id,
                  type: 'unpublish',
                  stream: stream.id,
                  timestamp: timeStamp.getTime() });
              }
            }
          }
          this.room.streamManager.removePublishedStream(stream.id);
        }
      });
      if (!this.room.p2p &&
          global.config.erizoController.report.session_events) {
        this.room.amqper.broadcast('event', { room: this.room.id,
          user: this.id,
          type: 'user_disconnection',
          timestamp: timeStamp.getTime() });
      }
      if (!this.room.p2p) {
        this.room.removeClient(this.id);
      }
      this.emit('disconnect');
    }
  }

  onGetStreamStats(streamId, callback) {
    log.debug(`message: getting stats, streamId: ${streamId}`);
    if (!this.hasPermission(Permission.STATS)) {
      log.info('message: unauthorized getStreamStats request');
      if (callback) callback(null, 'Unauthorized');
      return;
    }
    if (this.room.streamManager.getPublishedStreamById(streamId) === undefined) {
      log.info('message: bad getStreamStats request,', logger.objectToLog(this.token));
      return;
    }
    if (this.room !== undefined && !this.room.p2p) {
      this.room.controller.getStreamStats(streamId, (result) => {
        callback(result);
      });
    }
  }
}

exports.Client = Client;
