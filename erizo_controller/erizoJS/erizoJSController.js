/*global require, exports, setInterval, clearInterval, Promise*/
'use strict';
var logger = require('./../common/logger').logger;
var amqper = require('./../common/amqper');
var Publisher = require('./models/Publisher').Publisher;
var ExternalInput = require('./models/Publisher').ExternalInput;

// Logger
var log = logger.getLogger('ErizoJSController');

exports.ErizoJSController = function (threadPool, ioThreadPool) {
    var that = {},
        // {id1: Publisher, id2: Publisher}
        publishers = {},
        io = ioThreadPool,
        // {streamId: {timeout: timeout, interval: interval}}
        statsSubscriptions = {},
        MIN_SLIDESHOW_PERIOD = 2000,
        MAX_SLIDESHOW_PERIOD = 10000,
        PLIS_TO_RECOVER = 3,
        initWebRtcConnection,
        closeWebRtcConnection,
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

    /*
     * Given a WebRtcConnection waits for the state CANDIDATES_GATHERED for set remote SDP.
     */
    initWebRtcConnection = function (wrtc, callback, idPub, idSub, options) {
        log.debug('message: Init WebRtcConnection, id: ' + wrtc.wrtcId + ', ' +
                  logger.objectToLog(options));

        if (options.metadata) {
            wrtc.setMetadata(JSON.stringify(options.metadata));
            wrtc.metadata = options.metadata;
        }

        if (wrtc.minVideoBW) {
            var monitorMinVideoBw = {};
            if (wrtc.scheme) {
                try{
                    monitorMinVideoBw = require('./adapt_schemes/' + wrtc.scheme)
                                          .MonitorSubscriber(log);
                } catch (e) {
                    log.warn('message: could not find custom adapt scheme, ' +
                             'code: ' + WARN_PRECOND_FAILED + ', ' +
                             'id:' + wrtc.wrtcId + ', ' +
                             'scheme: ' + wrtc.scheme + ', ' +
                             logger.objectToLog(options.metadata));
                    monitorMinVideoBw = require('./adapt_schemes/notify').MonitorSubscriber(log);
                }
            } else {
                monitorMinVideoBw = require('./adapt_schemes/notify').MonitorSubscriber(log);
            }
            monitorMinVideoBw(wrtc, callback, idPub, idSub, options, that);
        }

        if (global.config.erizoController.report.rtcp_stats) {  // jshint ignore:line
            log.debug('message: RTCP Stat collection is active');
            wrtc.getPeriodicStats(function (newStats) {
                var timeStamp = new Date();
                amqper.broadcast('stats', {pub: idPub,
                                           subs: idSub,
                                           stats: JSON.parse(newStats),
                                           timestamp:timeStamp.getTime()});
            });
        }

        wrtc.init(function (newStatus, mess) {
            log.info('message: WebRtcConnection status update, ' +
                     'id: ' + wrtc.wrtcId + ', status: ' + newStatus +
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
                             'code: ' + WARN_BAD_CONNECTION + ', id: ' + wrtc.wrtcId);
                    callback('callback', {type: 'failed', sdp: mess});
                    break;

                case CONN_READY:
                    log.debug('message: connection ready, ' +
                              'id: ' + wrtc.wrtcId + ', ' +
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
            log.debug('message: create offer requested, id:', wrtc.wrtcId);
            var audioEnabled = options.createOffer.audio;
            var videoEnabled = options.createOffer.video;
            var bundle = options.createOffer.bundle;
            wrtc.createOffer(videoEnabled, audioEnabled, bundle);
        }
        callback('callback', {type: 'initializing'});
    };

    closeWebRtcConnection = function (wrtc) {
        var associatedMetadata = wrtc.metadata || {};
        if (wrtc.monitorInterval) {
          clearInterval(wrtc.monitorInterval);
        }
        wrtc.close();
        log.info('message: WebRtcConnection status update, ' +
            'id: ' + wrtc.wrtcId + ', status: ' + CONN_FINISHED + ', ' +
                logger.objectToLog(associatedMetadata));
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

    that.addExternalInput = function (from, url, callback) {
        if (publishers[from] === undefined) {
            var ei = publishers[from] = new ExternalInput(from, threadPool, ioThreadPool, url);
            var answer = ei.init();
            if (answer >= 0) {
                callback('callback', 'success');
            } else {
                callback('callback', answer);
            }
        } else {
            log.warn('message: Publisher already set, code: ' + WARN_CONFLICT + ', id: ' + from);
        }
    };

    that.addExternalOutput = function (to, url) {
        if (publishers[to]) publishers[to].addExternalOutput(url);
    };

    that.removeExternalOutput = function (to, url) {
        if (publishers[to] !== undefined) {
            log.info('message: Stopping ExternalOutput, id: ' +
                publishers[to].getExternalOutput(url).wrtcId);
            publishers[to].removeExternalOutput(url);
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

    var disableDefaultHandlers = function(wrtc) {
      var disabledHandlers = global.config.erizo.disabledHandlers;
      for (var index in disabledHandlers) {
        wrtc.disableHandler(disabledHandlers[index]);
      }
    };

    that.processSignaling = function (streamId, peerId, msg) {
        log.info('message: Process Signaling message, ' +
                 'streamId: ' + streamId + ', peerId: ' + peerId);
        if (publishers[streamId] !== undefined) {
            var publisher = publishers[streamId];
            if (publisher.hasSubscriber(peerId)) {
                var subscriber = publisher.getSubscriber(peerId);
                if (msg.type === 'offer') {
                    subscriber.setRemoteSdp(msg.sdp);
                    disableDefaultHandlers(subscriber);
                } else if (msg.type === 'candidate') {
                    subscriber.addRemoteCandidate(msg.candidate.sdpMid,
                                                  msg.candidate.sdpMLineIndex,
                                                  msg.candidate.candidate);
                } else if (msg.type === 'updatestream') {
                    if(msg.sdp)
                        subscriber.setRemoteSdp(msg.sdp);
                    if (msg.config) {
                        if (msg.config.slideShowMode !== undefined) {
                            that.setSlideShow(msg.config.slideShowMode, peerId, streamId);
                        }
                        if (msg.config.muteStream !== undefined) {
                            that.muteStream (msg.config.muteStream, peerId, streamId);
                        }
                        if (msg.config.qualityLayer !== undefined) {
                            that.setQualityLayer (msg.config.qualityLayer, peerId, streamId);
                        }
                        if (msg.config.video !== undefined) {
                            that.setVideoConstraints(msg.config.video, peerId, streamId);
                        }
                    }
                } else if (msg.type === 'control') {
                  processControlMessage(publisher, peerId, msg.action);
                }
            } else {
                if (msg.type === 'offer') {
                    publisher.wrtc.setRemoteSdp(msg.sdp);
                    disableDefaultHandlers(publisher.wrtc);
                } else if (msg.type === 'candidate') {
                    publisher.wrtc.addRemoteCandidate(msg.candidate.sdpMid,
                                                                 msg.candidate.sdpMLineIndex,
                                                                 msg.candidate.candidate);
                } else if (msg.type === 'updatestream') {
                    if (msg.sdp) {
                        publisher.wrtc.setRemoteSdp(msg.sdp);
                    }
                    if (msg.config) {
                        if (msg.config.minVideoBW) {
                            log.debug('message: updating minVideoBW for publisher, ' +
                                      'id: ' + publishers[streamId].wrtcId + ', ' +
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
                            that.muteStream (msg.config.muteStream, peerId, streamId);
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
    that.addPublisher = function (from, options, callback) {
        var publisher;

        if (publishers[from] === undefined) {

            log.info('message: Adding publisher, ' +
                     'streamId: ' + from + ', ' +
                     logger.objectToLog(options) + ', ' +
                     logger.objectToLog(options.metadata));
            publisher = new Publisher(from, threadPool, ioThreadPool, options);
            publishers[from] = publisher;

            initWebRtcConnection(publisher.wrtc, callback, from, undefined, options);

        } else {
            publisher = publishers[from];
            if (publisher.numSubscribers === 0) {
                log.warn('message: publisher already set but no subscribers will republish, ' +
                         'code: ' + WARN_CONFLICT + ', streamId: ' + from + ', ' +
                         logger.objectToLog(options.metadata));


                publisher.resetWrtc();

                initWebRtcConnection(publisher.wrtc, callback, from, undefined, options);
            } else {
                log.warn('message: publisher already set has subscribers will ignore, ' +
                         'code: ' + WARN_CONFLICT + ', streamId: ' + from);
            }
        }
    };

    /*
     * Adds a subscriber to the room. This creates a new WebRtcConnection.
     * This WebRtcConnection will be added to the subscribers list of the
     * OneToManyProcessor.
     */
    that.addSubscriber = function (from, to, options, callback) {
        var publisher = publishers[to];
        if (publisher === undefined) {
            log.warn('message: addSubscriber to unknown publisher, ' +
                     'code: ' + WARN_NOT_FOUND + ', streamId: ' + to + ', clientId: ' + from +
                      ', ' + logger.objectToLog(options.metadata));
            //We may need to notify the clients
            return;
        }
        var subscriber = publisher.getSubscriber(from);
        if (subscriber !== undefined) {
            log.warn('message: Duplicated subscription will resubscribe, ' +
                     'code: ' + WARN_CONFLICT + ', streamId: ' + to + ', clientId: ' + from+
                      ', ' + logger.objectToLog(options.metadata));
            that.removeSubscriber(from,to);
        }
        publisher.addSubscriber(from, options);
        initWebRtcConnection(publisher.getSubscriber(from), callback, to, from, options);
    };

    /*
     * Removes a publisher from the room. This also deletes the associated OneToManyProcessor.
     */
    that.removePublisher = function (from) {
      var publisher = publishers[from];
        if (publisher !== undefined) {
            log.info('message: Removing publisher, id: ' + from);
            if (publisher.periodicPlis !== undefined) {
                log.debug('message: clearing periodic PLIs for publisher, id: ' + from);
                clearInterval (publisher.periodicPlis);
            }
            for (var key in publisher.subscribers) {
                var subscriber = publisher.getSubscriber(key);
                log.info('message: Removing subscriber, id: ' + subscriber.wrtcId);
                closeWebRtcConnection(subscriber);
            }
            closeWebRtcConnection(publisher.wrtc);
            publisher.muxer.close(function(message) {
                log.info('message: muxer closed succesfully, ' +
                         'id: ' + from + ', ' +
                         logger.objectToLog(message));
                delete publishers[from];
                var count = 0;
                for (var k in publishers) {
                    if (publishers.hasOwnProperty(k)) {
                        ++count;
                    }
                }
                log.debug('message: remaining publishers, publisherCount: ' + count);
                if (count === 0)  {
                    log.info('message: Removed all publishers. Killing process.');
                    process.exit(0);
                }
            });

        } else {
            log.warn('message: removePublisher that does not exist, ' +
                     'code: ' + WARN_NOT_FOUND + ', id: ' + from);
        }
    };

    /*
     * Removes a subscriber from the room.
     * This also removes it from the associated OneToManyProcessor.
     */
    that.removeSubscriber = function (from, to) {
        var publisher = publishers[to];
        if (publisher && publisher.hasSubscriber(from)) {
            var subscriber = publisher.getSubscriber(from);
            log.info('message: removing subscriber, id: ' + subscriber.wrtcId);
            closeWebRtcConnection(subscriber);
            publisher.removeSubscriber(from);
        }

        if (publisher && publisher.wrtc.periodicPlis !== undefined) {
            for (var i in publisher.subscribers) {
                if (publisher.getSubscriber(i).slideShowMode === true) {
                    return;
                }
            }
            log.debug('message: clearing Pli interval as no more ' +
                      'slideshows subscribers are present');
            clearInterval(publisher.wrtc.periodicPlis);
            publisher.wrtc.periodicPlis = undefined;
        }
    };

    /*
     * Removes all the subscribers related with a client.
     */
    that.removeSubscriptions = function (from) {
        log.info('message: removing subscriptions, peerId:', from);
        for (var to in publishers) {
            if (publishers.hasOwnProperty(to)) {
                var publisher = publishers[to];
                var subscriber = publisher.getSubscriber(from);
                if (subscriber) {
                    log.debug('message: removing subscription, ' +
                              'id:', subscriber.wrtcId);
                    closeWebRtcConnection(subscriber);
                    publisher.removeSubscriber(from);
                }
            }
        }
    };

    /*
     * Enables/Disables slideshow mode for a subscriber
     */
    that.setSlideShow = function (slideShowMode, from, to) {
        var wrtcPub;
        var publisher = publishers[to];
        var theWrtc = publisher.getSubscriber(from);
        if (!theWrtc) {
            log.warn('message: wrtc not found for updating slideshow, ' +
                     'code: ' + WARN_NOT_FOUND + ', id: ' + from + '_' + to);
            return;
        }

        log.debug('message: setting SlideShow, id: ' + theWrtc.wrtcId +
                  ', slideShowMode: ' + slideShowMode);
        var period =  slideShowMode === true ? MIN_SLIDESHOW_PERIOD : slideShowMode;
        if (Number.isSafeInteger(period)) {
            period = period < MIN_SLIDESHOW_PERIOD ? MIN_SLIDESHOW_PERIOD : period;
            period = period > MAX_SLIDESHOW_PERIOD ? MAX_SLIDESHOW_PERIOD : period;
            theWrtc.setSlideShowMode(true);
            theWrtc.slideShowMode = true;
            wrtcPub = publisher.wrtc;
            if (wrtcPub.periodicPlis) {
                clearInterval(wrtcPub.periodicPlis);
                wrtcPub.periodicPlis = undefined;
            }
            wrtcPub.periodicPlis = setInterval(function () {
                if(wrtcPub)
                    wrtcPub.generatePLIPacket();
            }, period);
        } else {
            wrtcPub = publisher.wrtc;
            for (var pliIndex = 0; pliIndex < PLIS_TO_RECOVER; pliIndex++) {
              wrtcPub.generatePLIPacket();
            }

            theWrtc.setSlideShowMode(false);
            theWrtc.slideShowMode = false;
            if (publisher.wrtc.periodicPlis !== undefined) {
                for (var i in publisher.subscribers) {
                    if (publisher.getSubscriber(i).slideShowMode === true) {
                        return;
                    }
                }
                log.debug('message: clearing PLI interval for publisher slideShow, ' +
                          'id: ' + publisher.wrtc.wrtcId);
                clearInterval(publisher.wrtc.periodicPlis);
                publisher.wrtc.periodicPlis = undefined;
            }
        }

    };

    that.muteStream = function (muteStreamInfo, from, to) {
        var publisher = this.publishers[to];
        if (muteStreamInfo.video === undefined) {
            muteStreamInfo.video = false;
        }

        if (muteStreamInfo.audio === undefined) {
            muteStreamInfo.audio = false;
        }
        if (publisher.hasSubscriber(from)) {
          publisher.muteSubscriberStream(from, muteStreamInfo.video, muteStreamInfo.audio);
        } else {
          publisher.muteStream(muteStreamInfo.video, muteStreamInfo.audio);
        }
    };

    that.setQualityLayer = function (qualityLayer, from, to) {
      var publisher = this.publishers[to];
      if (publisher.hasSubscriber(from)) {
        publisher.setQualityLayer(from, qualityLayer.spatialLayer, qualityLayer.temporalLayer);
      }
    };

    that.setVideoConstraints = function (video, from, to) {
      var publisher = this.publishers[to];
      if (publisher.hasSubscriber(from)) {
        publisher.setVideoConstraints(from, video.width, video.height, video.frameRate);
      }
    };

    const getWrtcStats = (label, stats, wrtc) => {
      const promise = new Promise((resolve) => {
        wrtc.getStats((statsString) => {
          const unfilteredStats = JSON.parse(statsString);
          unfilteredStats.metadata = wrtc.metadata;
          stats[label] = unfilteredStats;
          resolve();
        });
      });
      return promise;
    };

    that.getStreamStats = function (to, callback) {
        var stats = {};
        var publisher;
        log.debug('message: Requested stream stats, streamID: ' + to);
        var promises = [];
        if (to && publishers[to]) {
          publisher = publishers[to];
          promises.push(getWrtcStats('publisher', stats, publisher.wrtc));
          for (var sub in publisher.subscribers) {
            promises.push(getWrtcStats(sub, stats, publisher.subscribers[sub]));
          }
          Promise.all(promises).then(() => {
            callback('callback', stats);
          });
        }
    };

    that.subscribeToStats = function (to, timeout, interval, callback) {

        var publisher;
        log.debug('message: Requested subscription to stream stats, streamID: ' + to);

        if (to && publishers[to]) {
            publisher = publishers[to];

            if (global.config.erizoController.reportSubscriptions &&
                global.config.erizoController.reportSubscriptions.maxSubscriptions > 0) {

                if (timeout > global.config.erizoController.reportSubscriptions.maxTimeout)
                    timeout = global.config.erizoController.reportSubscriptions.maxTimeout;
                if (interval < global.config.erizoController.reportSubscriptions.minInterval)
                    interval = global.config.erizoController.reportSubscriptions.minInterval;

                if (statsSubscriptions[to]) {
                    log.debug('message: Renewing subscription to stream: ' + to);
                    clearTimeout(statsSubscriptions[to].timeout);
                    clearInterval(statsSubscriptions[to].interval);
                } else if (Object.keys(statsSubscriptions).length <
                    global.config.erizoController.reportSubscriptions.maxSubscriptions){
                    statsSubscriptions[to] = {};
                }

                if (!statsSubscriptions[to]) {
                    log.debug('message: Max Subscriptions limit reached, ignoring message');
                    return;
                }

                statsSubscriptions[to].interval = setInterval(function () {
                    let promises = [];
                    let stats = {};

                    stats.streamId = to;
                    promises.push(getWrtcStats('publisher', stats, publisher.wrtc));

                    for (var sub in publisher.subscribers) {
                        promises.push(getWrtcStats(sub, stats, publisher.subscribers[sub]));
                    }

                    Promise.all(promises).then(() => {
                        amqper.broadcast('stats_subscriptions', stats);
                    });

                }, interval*1000);

                statsSubscriptions[to].timeout = setTimeout(function () {
                    clearInterval(statsSubscriptions[to].interval);
                    delete statsSubscriptions[to];
                }, timeout*1000);

                callback('success');

            } else {
                log.debug('message: Report subscriptions disabled by config, ignoring message');
            }
        } else {
            log.debug('message: Im not handling this stream, ignoring message, streamID: ' + to);
        }
    };

    return that;
};
