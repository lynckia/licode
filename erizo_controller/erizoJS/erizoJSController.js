/*global require, exports, setInterval, clearInterval, Promise*/
'use strict';
var logger = require('./../common/logger').logger;
var amqper = require('./../common/amqper');
var Client = require('./models/Client').Client;
var Publisher = require('./models/Publisher').Publisher;
var ExternalInput = require('./models/Publisher').ExternalInput;
var SessionDescription = require('./models/SessionDescription');
var SemanticSdp = require('./../common/semanticSdp/SemanticSdp');
var Helpers = require('./models/Helpers');

// Logger
var log = logger.getLogger('ErizoJSController');

exports.ErizoJSController = function (threadPool, ioThreadPool) {
    var that = {},
        // {streamId1: Publisher, streamId2: Publisher}
        publishers = {},
        // {clientId: Client}
        clients = new Map(),
        io = ioThreadPool,
        // {streamId: {timeout: timeout, interval: interval}}
        statsSubscriptions = {},
        MIN_SLIDESHOW_PERIOD = 2000,
        MAX_SLIDESHOW_PERIOD = 10000,
        PLIS_TO_RECOVER = 3,
        initWebRtcConnection,
        closeWebRtcConnection,
        getOrCreateClient,
        getSdp,
        getRoap,

        CONN_INITIAL        = 101,
        // CONN_STARTED        = 102,
        CONN_GATHERED       = 103,
        CONN_READY          = 104,
        CONN_FINISHED       = 105,
        CONN_CANDIDATE      = 201,
        CONN_SDP            = 202,
        CONN_FAILED         = 500,
        WARN_NOT_FOUND      = 404,
        WARN_CONFLICT       = 409,
        WARN_PRECOND_FAILED = 412,
        WARN_BAD_CONNECTION = 502;

    that.publishers = publishers;
    that.ioThreadPool = io;
    
    getOrCreateClient = (clientId) => {
      log.debug(`getOrCreateClient with id ${clientId}`);
      let client = clients.get(clientId);
      if(client === undefined) {
        client = new Client(clientId, threadPool, ioThreadPool);
        clients.set(clientId, client);
      }
      return client;
    };
    /*
     * Given a WebRtcConnection waits for the state CANDIDATES_GATHERED for set remote SDP.
     */
    initWebRtcConnection = function (node, callback, idPub, idSub, options) {
        const connection = node.connection;
        const mediaStream = node.mediaStream;
        const wrtc = connection.wrtc;
        log.debug(`message: Init WebRtcConnection, node ClientId: ${node.clientId}, ` +
          `node StreamId: ${node.streamId}, connectionId: ${node.connection.id}, ` +
          `scheme: ${mediaStream.scheme} ${logger.objectToLog(options)}`);


        if (options.metadata) {
            wrtc.setMetadata(JSON.stringify(options.metadata));
            node.connection.metadata = options.metadata;
        }

        if (mediaStream.minVideoBW) {
            var monitorMinVideoBw = {};
            if (mediaStream.scheme) {
                try{
                    monitorMinVideoBw = require('./adapt_schemes/' + mediaStream.scheme)
                                          .MonitorSubscriber(log);
                } catch (e) {
                    log.warn('message: could not find custom adapt scheme, ' +
                             'code: ' + WARN_PRECOND_FAILED + ', ' +
                             'id:' + node.clientId + ', ' +
                             'scheme: ' + mediaStream.scheme + ', ' +
                             logger.objectToLog(options.metadata));
                }
            } else {
                monitorMinVideoBw = require('./adapt_schemes/notify').MonitorSubscriber(log);
            }
            monitorMinVideoBw(mediaStream, callback, idPub, idSub, options, that);
        }

        if (global.config.erizoController.report.rtcp_stats) {  // jshint ignore:line
            log.debug('message: RTCP Stat collection is active');
            mediaStream.getPeriodicStats(function (newStats) {
                var timeStamp = new Date();
                amqper.broadcast('stats', {pub: idPub,
                                           subs: idSub,
                                           stats: JSON.parse(newStats),
                                           timestamp:timeStamp.getTime()});
            });
        }

        wrtc.init(function (newStatus, mess) {
            log.info('message: WebRtcConnection status update, ' +
                     'id: ' + connection.id + ', status: ' + newStatus +
                      ', ' + logger.objectToLog(options.metadata));
            if (global.config.erizoController.report.connection_events) {  //jshint ignore:line
                var timeStamp = new Date();
                amqper.broadcast('event', {pub: idPub,
                                           subs: idSub,
                                           type: 'connection_status',
                                           status: newStatus,
                                           timestamp:timeStamp.getTime()});
            }

            switch(newStatus) {
                case CONN_INITIAL:
                    callback('callback', {type: 'started'});
                    break;

                case CONN_SDP:
                case CONN_GATHERED:
                    wrtc.localDescription = new SessionDescription(wrtc.getLocalDescription());
                    const sdp = wrtc.localDescription.getSdp();
                    mess = sdp.toString();
                    mess = mess.replace(that.privateRegexp, that.publicIP);
                    if (options.createOffer)
                        callback('callback', {type: 'offer', sdp: mess});
                    else
                        callback('callback', {type: 'answer', sdp: mess});
                    break;

                case CONN_CANDIDATE:
                    mess = mess.replace(that.privateRegexp, that.publicIP);
                    callback('callback', {type: 'candidate', candidate: mess});
                    break;

                case CONN_FAILED:
                    log.warn('message: failed the ICE process, ' +
                             'code: ' + WARN_BAD_CONNECTION + ', id: ' + connection.id);
                    callback('callback', {type: 'failed', sdp: mess});
                    break;

                case CONN_READY:
                    log.debug('message: connection ready, ' +
                              'id: ' + connection.id + ', ' +
                              'status: ' + newStatus);
                    // If I'm a subscriber and I'm bowser, I ask for a PLI
                    if (idSub && options.browser === 'bowser') {
                        publishers[idPub].wrtc.generatePLIPacket();
                    }
                    if (options.slideShowMode === true || 
                        Number.isSafeInteger(options.slideShowMode)) {
                        that.setSlideShow(options.slideShowMode, idSub, idPub);
                    }
                    callback('callback', {type: 'ready'});
                    break;
            }
        });
        if (options.createOffer) {
            log.debug('message: create offer requested, id:', connection);
            var audioEnabled = options.createOffer.audio;
            var videoEnabled = options.createOffer.video;
            var bundle = options.createOffer.bundle;
            wrtc.createOffer(videoEnabled, audioEnabled, bundle);
        }
        callback('callback', {type: 'initializing'});
    };

    closeWebRtcConnection = function (node) {
        const clientId = node.clientId;
        log.debug(`message: closeWebRtcConnection, clientId: ${node.clientId}, ` +
          `streamId: ${node.streamId}`);
        let client = clients.get(clientId);
        if (client === undefined) {
          log.debug(`message: trying to close connection with no associated client,` +
           `clientId: ${clientId}, streamId: ${node.streamId}`);
        }
        
        let connection = node.connection;
        let mediaStream = node.mediaStream;
        var associatedMetadata = connection.metadata || {};
        if (mediaStream && mediaStream.monitorInterval) {
          clearInterval(mediaStream.monitorInterval);
        }
        node.close();
        log.info('message: WebRtcConnection status update, ' +
            'id: ' + connection.id + ', status: ' + CONN_FINISHED + ', ' +
                logger.objectToLog(associatedMetadata));
        let remainingConnections = client.maybeCloseConnection(connection.id);
        if (remainingConnections === 0) {
          log.debug(`message: Removing empty client from list, clientId: ${client.id}`);
          clients.delete(client.id);
        }
    };

    /*
     * Gets SDP from roap message.
     */
    getSdp = function (roap) {

        var reg1 = new RegExp(/^.*sdp\":\"(.*)\",.*$/),
            sdp = roap.match(reg1)[1],
            reg2 = new RegExp(/\\r\\n/g);

        sdp = sdp.replace(reg2, '\n');

        return sdp;
    };

    /*
     * Gets roap message from SDP.
     */
    getRoap = function (sdp, offerRoap) {

        var reg1 = new RegExp(/\n/g),
            offererSessionId = offerRoap.match(/("offererSessionId":)(.+?)(,)/)[0],
            answererSessionId = '106',
            answer = ('\n{\n \"messageType\":\"ANSWER\",\n');

                sdp = sdp.replace(reg1, '\\r\\n');

                //var reg2 = new RegExp(/^.*offererSessionId\":(...).*$/);
                //var offererSessionId = offerRoap.match(reg2)[1];

                answer += ' \"sdp\":\"' + sdp + '\",\n';
                //answer += ' \"offererSessionId\":' + offererSessionId + ',\n';
                answer += ' ' + offererSessionId + '\n';
                answer += ' \"answererSessionId\":' + answererSessionId + ',\n \"seq\" : 1\n}\n';

                return answer;
    };

    that.addExternalInput = function (streamId, url, callback) {
        if (publishers[streamId] === undefined) {
            let client = getOrCreateClient(url);
            var ei = publishers[streamId] = new ExternalInput(url, streamId, threadPool);
            var answer = ei.init();
            // We add the connection manually to the client
            client.addConnection(ei);
            if (answer >= 0) {
                callback('callback', 'success');
            } else {
                callback('callback', answer);
            }
        } else {
            log.warn('message: Publisher already set, code: ' +
              WARN_CONFLICT + ', streamId: ' + streamId);
        }
    };

    that.addExternalOutput = function (streamId, url, options) {
        if (publishers[streamId]) publishers[streamId].addExternalOutput(url, options);
    };

    that.removeExternalOutput = function (streamId, url) {
        if (publishers[streamId] !== undefined) {
            log.info('message: Stopping ExternalOutput, id: ' +
                publishers[streamId].getExternalOutput(url).id);
            publishers[streamId].removeExternalOutput(url);
        }
    };

    var processControlMessage = function(publisher, subscriberId, action) {
      var publisherSide = subscriberId === undefined || action.publisherSide;
      switch(action.name) {
        case 'controlhandlers':
          if (action.enable) {
            publisher.enableHandlers(publisherSide ? undefined : subscriberId, action.handlers);
          } else {
            publisher.disableHandlers(publisherSide ? undefined : subscriberId, action.handlers);
          }
          break;
      }
    };

    var disableDefaultHandlers = function(node) {
      let mediaStream = node.mediaStream;
      var disabledHandlers = global.config.erizo.disabledHandlers;
      for (var index in disabledHandlers) {
        mediaStream.disableHandler(disabledHandlers[index]);
      }
    };

    that.processSignaling = function (clientId, streamId, msg) {
        log.info('message: Process Signaling message, ' +
                 'streamId: ' + streamId + ', clientId: ' + clientId);
        if (publishers[streamId] !== undefined) {
            let publisher = publishers[streamId];
            if (publisher.hasSubscriber(clientId)) {
                let subscriber = publisher.getSubscriber(clientId);
                let connection = subscriber.connection;
                let wrtc = connection.wrtc;
                
                if (msg.type === 'offer') {
                    const sdp = SemanticSdp.SDPInfo.processString(msg.sdp);
                    connection.remoteDescription =
                        new SessionDescription(sdp, connection.mediaConfiguration);
                    wrtc.setRemoteDescription(
                        connection.remoteDescription.connectionDescription);
                    disableDefaultHandlers(subscriber);
                } else if (msg.type === 'candidate') {
                    wrtc.addRemoteCandidate(msg.candidate.sdpMid,
                                                  msg.candidate.sdpMLineIndex,
                                                  msg.candidate.candidate);
                } else if (msg.type === 'updatestream') {
                    if(msg.sdp) {
                        const sdp = SemanticSdp.SDPInfo.processString(msg.sdp);
                        connection.remoteDescription =
                            new SessionDescription(sdp, connection.mediaConfiguration);
                        wrtc.setRemoteDescription(
                            subscriber.connection.remoteDescription.connectionDescription);
                      }
                    if (msg.config) {
                        if (msg.config.slideShowMode !== undefined) {
                            that.setSlideShow(msg.config.slideShowMode, clientId, streamId);
                        }
                        if (msg.config.muteStream !== undefined) {
                            that.muteStream(msg.config.muteStream, clientId, streamId);
                        }
                        if (msg.config.qualityLayer !== undefined) {
                            that.setQualityLayer(msg.config.qualityLayer, clientId, streamId);
                        }
                        if (msg.config.video !== undefined) {
                            that.setVideoConstraints(msg.config.video, clientId, streamId);
                        }
                    }
                } else if (msg.type === 'control') {
                  processControlMessage(publisher, clientId, msg.action);
                }
            } else {
                let connection = publisher.connection;
                let wrtc = connection.wrtc;
                if (msg.type === 'offer') {
                    const sdp = SemanticSdp.SDPInfo.processString(msg.sdp);
                    connection.remoteDescription =
                        new SessionDescription(sdp, connection.mediaConfiguration);
                    wrtc.setRemoteDescription(
                        connection.remoteDescription.connectionDescription);
                    disableDefaultHandlers(publisher);
                } else if (msg.type === 'candidate') {
                    wrtc.addRemoteCandidate(msg.candidate.sdpMid,
                                            msg.candidate.sdpMLineIndex,
                                            msg.candidate.candidate);
                } else if (msg.type === 'updatestream') {
                    if (msg.sdp) {
                        const sdp = SemanticSdp.SDPInfo.processString(msg.sdp);
                        connection.remoteDescription =
                            new SessionDescription(sdp, connection.mediaConfiguration);
                        wrtc.setRemoteDescription(
                            connection.remoteDescription.connectionDescription);
                    }
                    if (msg.config) {
                        if (msg.config.minVideoBW) {
                            log.debug('message: updating minVideoBW for publisher, ' +
                                      'id: ' + publishers[streamId].streamId+ ', ' +
                                      'minVideoBW: ' + msg.config.minVideoBW);
                            publisher.minVideoBW = msg.config.minVideoBW;
                            for (var sub in publisher.subscribers) {
                                var theConn = publisher.getSubscriber(sub);
                                theConn.minVideoBW = msg.config.minVideoBW * 1000; // bps
                                theConn.lowerThres = Math.floor(theConn.minVideoBW*(1-0.2));
                                theConn.upperThres = Math.ceil(theConn.minVideoBW*(1+0.1));
                            }
                        }
                        if (msg.config.muteStream !== undefined) {
                            that.muteStream (msg.config.muteStream, clientId, streamId);
                        }
                    }
                } else if (msg.type === 'control') {
                  processControlMessage(publisher, undefined, msg.action);
                }
            }

        }
    };

    /*
     * Adds a publisher to the room. This creates a new OneToManyProcessor
     * and a new WebRtcConnection. This WebRtcConnection will be the publisher
     * of the OneToManyProcessor.
     */
    that.addPublisher = function (clientId, streamId, options, callback) {
        let publisher;
        log.info('addPublisher, clientId', clientId, 'streamId', streamId);
        let client = getOrCreateClient(clientId);

        if (publishers[streamId] === undefined) {
          let erizoStreamId = Helpers.getErizoStreamId(clientId, streamId);
          let connection = client.getOrCreateConnection();
            log.info('message: Adding publisher, ' +
                     'clientId: ' + clientId + ', ' +
                     'streamId: ' + streamId + ', ' +
                     logger.objectToLog(options) + ', ' +
                     logger.objectToLog(options.metadata));
            publisher = new Publisher(clientId, streamId, connection, options);
            publishers[streamId] = publisher;

            initWebRtcConnection(publisher, callback, streamId, undefined, options);

        } else {
            publisher = publishers[streamId];
            if (publisher.numSubscribers === 0) {
                log.warn('message: publisher already set but no subscribers will ignore, ' +
                         'code: ' + WARN_CONFLICT + ', streamId: ' + streamId + ', ' +
                         logger.objectToLog(options.metadata));
            } else {
                log.warn('message: publisher already set has subscribers will ignore, ' +
                         'code: ' + WARN_CONFLICT + ', streamId: ' + streamId);
            }
        }
    };

    /*
     * Adds a subscriber to the room. This creates a new WebRtcConnection.
     * This WebRtcConnection will be added to the subscribers list of the
     * OneToManyProcessor.
     */
    that.addSubscriber = function (clientId, streamId, options, callback) {
        const publisher = publishers[streamId];
        const erizoStreamId = Helpers.getErizoStreamId(clientId, streamId);
        if (publisher === undefined) {
            log.warn('message: addSubscriber to unknown publisher, ' +
                     'code: ' + WARN_NOT_FOUND + ', streamId: ' + streamId +
                      ', clientId: ' + clientId +
                      ', ' + logger.objectToLog(options.metadata));
            //We may need to notify the clients
            return;
        }
        const subscriber = publisher.getSubscriber(clientId);
        const client = getOrCreateClient(clientId);
        if (subscriber !== undefined) {
            log.warn('message: Duplicated subscription will resubscribe, ' +
                     'code: ' + WARN_CONFLICT + ', streamId: ' + streamId +
                     ', clientId: ' + clientId +
                     ', ' + logger.objectToLog(options.metadata));
            that.removeSubscriber(clientId, streamId);
        }
        let connection = client.getOrCreateConnection();
        publisher.addSubscriber(clientId, connection, options);
        initWebRtcConnection(publisher.getSubscriber(clientId),
          callback, streamId, clientId, options);
    };

    /*
     * Removes a publisher from the room. This also deletes the associated OneToManyProcessor.
     */
    that.removePublisher = function (clientId, streamId) {
      return new Promise(function(resolve, reject) {
        var publisher = publishers[streamId];
          if (publisher !== undefined) {
              log.info(`message: Removing publisher, id: ${clientId}, streamId: ${streamId}`);
              if (publisher.mediaStream.periodicPlis !== undefined) {
                  log.debug('message: clearing periodic PLIs for publisher, id: ' + streamId);
                  clearInterval (publisher.mediaStream.periodicPlis);
              }
              for (let subscriberKey in publisher.subscribers) {
                  let subscriber = publisher.getSubscriber(subscriberKey);
                  log.info('message: Removing subscriber, id: ' + subscriber);
                  closeWebRtcConnection(subscriber);
              }
              publisher.removeExternalOutputs().then(function() {
                closeWebRtcConnection(publisher);
                publisher.muxer.close(function(message) {
                    log.info('message: muxer closed succesfully, ' +
                             'id: ' + streamId + ', ' +
                             logger.objectToLog(message));
                    delete publishers[streamId];
                    var count = 0;
                    for (var k in publishers) {
                        if (publishers.hasOwnProperty(k)) {
                            ++count;
                        }
                    }
                    log.debug('message: remaining publishers, publisherCount: ' + count);
                    resolve();
                    if (count === 0)  {
                        log.info('message: Removed all publishers. Killing process.');
                        process.exit(0);
                    }
                });
              });

          } else {
              log.warn('message: removePublisher that does not exist, ' +
                       'code: ' + WARN_NOT_FOUND + ', id: ' + streamId);
              reject();
          }
        }.bind(this));
    };

    /*
     * Removes a subscriber from the room.
     * This also removes it from the associated OneToManyProcessor.
     */
    that.removeSubscriber = function (clientId, streamId) {
        const publisher = publishers[streamId];
        if (publisher && publisher.hasSubscriber(clientId)) {
            
            let subscriber = publisher.getSubscriber(clientId);
            log.info(`message: removing subscriber, streamId: ${subscriber.streamId}, ` +
              `clientId: ${clientId}`);
            closeWebRtcConnection(subscriber);
            publisher.removeSubscriber(clientId);
        }
        // get the right mediaStream and stop the slideShow
        if (publisher && publisher.connection &&
           publisher.mediaStream &&
           publisher.mediaStream.periodicPlis !== undefined) {
            for (var i in publisher.subscribers) {
                if (publisher.getSubscriber(i).mediaStream.slideShowMode === true) {
                    return;
                }
            }
            log.debug('message: clearing Pli interval as no more ' +
                      'slideshows subscribers are present');
            clearInterval(publisher.mediaStream.periodicPlis);
            publisher.mediaStream.periodicPlis = undefined;
        }
    };

    /*
     * Removes all the subscribers related with a client.
     */
    that.removeSubscriptions = function (clientId) {
        log.info('message: removing subscriptions, clientId:', clientId);
        // we go through all the connections in the client and we close them
        for (var to in publishers) {
            if (publishers.hasOwnProperty(to)) {
                var publisher = publishers[to];
                var subscriber = publisher.getSubscriber(clientId);
                if (subscriber) {
                    log.debug('message: removing subscription, ' +
                              'id:', subscriber.clientId);
                    closeWebRtcConnection(subscriber);
                    publisher.removeSubscriber(clientId);
                }
            }
        }
    };

    /*
     * Enables/Disables slideshow mode for a subscriber
     */
    that.setSlideShow = function (slideShowMode, clientId, streamId) {
        const publisher = publishers[streamId];
        const subscriber = publisher.getSubscriber(clientId);
        if (!subscriber) {
            log.warn('message: subscriber not found for updating slideshow, ' +
                     'code: ' + WARN_NOT_FOUND + ', id: ' + clientId + '_' + streamId);
            return;
        }

        log.debug('message: setting SlideShow, id: ' + subscriber.clientId +
                  ', slideShowMode: ' + slideShowMode);
        let period =  slideShowMode === true ? MIN_SLIDESHOW_PERIOD : slideShowMode;
        if (Number.isSafeInteger(period)) {
            period = period < MIN_SLIDESHOW_PERIOD ? MIN_SLIDESHOW_PERIOD : period;
            period = period > MAX_SLIDESHOW_PERIOD ? MAX_SLIDESHOW_PERIOD : period;
            subscriber.mediaStream.setSlideShowMode(true);
            subscriber.mediaStream.slideShowMode = true;
            if (publisher.mediaStream.periodicPlis) {
                clearInterval(publisher.mediaStream.periodicPlis);
                publisher.mediaStream.periodicPlis = undefined;
            }
            publisher.mediaStream.periodicPlis = setInterval(function () {
                if(publisher)
                    publisher.mediaStream.generatePLIPacket();
            }, period);
        } else {
            for (var pliIndex = 0; pliIndex < PLIS_TO_RECOVER; pliIndex++) {
              publisher.mediaStream.generatePLIPacket();
            }

            subscriber.mediaStream.setSlideShowMode(false);
            subscriber.mediaStream.slideShowMode = false;
            if (publisher .mediaStream.periodicPlis !== undefined) {
                for (var i in publisher.subscribers) {
                    if (publisher.getSubscriber(i).mediaStream.slideShowMode === true) {
                        return;
                    }
                }
                log.debug('message: clearing PLI interval for publisher slideShow, ' +
                          'id: ' + publisher.clientId);
                clearInterval(publisher.mediaStream.periodicPlis);
                publisher.mediaStream.periodicPlis = undefined;
            }
        }

    };

    that.muteStream = function (muteStreamInfo, clientId, streamId) {
        var publisher = this.publishers[streamId];
        if (muteStreamInfo.video === undefined) {
            muteStreamInfo.video = false;
        }

        if (muteStreamInfo.audio === undefined) {
            muteStreamInfo.audio = false;
        }
        if (publisher.hasSubscriber(clientId)) {
          publisher.muteSubscriberStream(clientId, muteStreamInfo.video, muteStreamInfo.audio);
        } else {
          publisher.muteStream(muteStreamInfo.video, muteStreamInfo.audio);
        }
    };

    that.setQualityLayer = function (qualityLayer, clientId, streamId) {
      var publisher = this.publishers[streamId];
      if (publisher.hasSubscriber(clientId)) {
        publisher.setQualityLayer(clientId, qualityLayer.spatialLayer, qualityLayer.temporalLayer);
      }
    };

    that.setVideoConstraints = function (video, clientId, streamId) {
      var publisher = this.publishers[streamId];
      if (publisher.hasSubscriber(clientId)) {
        publisher.setVideoConstraints(clientId, video.width, video.height, video.frameRate);
      }
    };

    const getWrtcStats = (label, stats, node) => {
      const promise = new Promise((resolve) => {
        node.mediaStream.getStats((statsString) => {
          const unfilteredStats = JSON.parse(statsString);
          unfilteredStats.metadata = node.connection.metadata;
          stats[label] = unfilteredStats;
          resolve();
        });
      });
      return promise;
    };

    that.getStreamStats = function (streamId, callback) {
        var stats = {};
        var publisher;
        log.debug('message: Requested stream stats, streamID: ' + streamId);
        var promises = [];
        if (streamId && publishers[streamId]) {
          publisher = publishers[streamId];
          promises.push(getWrtcStats('publisher', stats, publisher));
          for (var sub in publisher.subscribers) {
            promises.push(getWrtcStats(sub, stats, publisher.subscribers[sub]));
          }
          Promise.all(promises).then(() => {
            callback('callback', stats);
          });
        }
    };

    that.subscribeToStats = function (streamId, timeout, interval, callback) {

        var publisher;
        log.debug('message: Requested subscription to stream stats, streamId: ' + streamId);

        if (streamId && publishers[streamId]) {
            publisher = publishers[streamId];

            if (global.config.erizoController.reportSubscriptions &&
                global.config.erizoController.reportSubscriptions.maxSubscriptions > 0) {

                if (timeout > global.config.erizoController.reportSubscriptions.maxTimeout)
                    timeout = global.config.erizoController.reportSubscriptions.maxTimeout;
                if (interval < global.config.erizoController.reportSubscriptions.minInterval)
                    interval = global.config.erizoController.reportSubscriptions.minInterval;

                if (statsSubscriptions[streamId]) {
                    log.debug('message: Renewing subscription to stream: ' + streamId);
                    clearTimeout(statsSubscriptions[streamId].timeout);
                    clearInterval(statsSubscriptions[streamId].interval);
                } else if (Object.keys(statsSubscriptions).length <
                    global.config.erizoController.reportSubscriptions.maxSubscriptions){
                    statsSubscriptions[streamId] = {};
                }

                if (!statsSubscriptions[streamId]) {
                    log.debug('message: Max Subscriptions limit reached, ignoring message');
                    return;
                }

                statsSubscriptions[streamId].interval = setInterval(function () {
                    let promises = [];
                    let stats = {};

                    stats.streamId = streamId;
                    promises.push(getWrtcStats('publisher', stats, publisher));

                    for (var sub in publisher.subscribers) {
                        promises.push(getWrtcStats(sub, stats, publisher.subscribers[sub]));
                    }

                    Promise.all(promises).then(() => {
                        amqper.broadcast('stats_subscriptions', stats);
                    });

                }, interval*1000);

                statsSubscriptions[streamId].timeout = setTimeout(function () {
                    clearInterval(statsSubscriptions[streamId].interval);
                    delete statsSubscriptions[streamId];
                }, timeout*1000);

                callback('success');

            } else {
                log.debug('message: Report subscriptions disabled by config, ignoring message');
            }
        } else {
            log.debug('message: stream not found - ignoring message, streamId: ' + streamId);
        }
    };

    return that;
};
