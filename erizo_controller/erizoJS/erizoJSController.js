/*global require, exports, setInterval, clearInterval*/
'use strict';
var addon = require('./../../erizoAPI/build/Release/addon');
var logger = require('./../common/logger').logger;
var amqper = require('./../common/amqper');

// Logger
var log = logger.getLogger('ErizoJSController');

exports.ErizoJSController = function () {
    var that = {},
        // {id: {subsId1: wrtc1, subsId2: wrtc2}}
        subscribers = {},
        // {id: {muxer: OneToManyProcessor, wrtc: WebRtcConnection}
        publishers = {},

        // {id: ExternalOutput}
        externalOutputs = {},

        SLIDESHOW_TIME = 1000,
        PLIS_TO_RECOVER = 3,
        initWebRtcConnection,
        getSdp,
        getRoap,

        CONN_INITIAL        = 101,
        // CONN_STARTED        = 102,
        CONN_GATHERED       = 103,
        CONN_READY          = 104,
        // CONN_FINISHED       = 105,
        CONN_CANDIDATE      = 201,
        CONN_SDP            = 202,
        CONN_FAILED         = 500,
        WARN_NOT_FOUND      = 404,
        WARN_CONFLICT       = 409,
        WARN_PRECOND_FAILED = 412,
        WARN_BAD_CONNECTION = 502;

    /*
     * Given a WebRtcConnection waits for the state CANDIDATES_GATHERED for set remote SDP.
     */
    initWebRtcConnection = function (wrtc, callback, idPub, idSub, options) {
        log.debug('message: Init WebRtcConnection, id: ' + wrtc.wrtcId + ', ' +
                  logger.objectToLog(options));

        if (options.metadata) {
          wrtc.setMetadata(JSON.stringify(options.metadata));
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

        if (GLOBAL.config.erizoController.report.rtcp_stats) {  // jshint ignore:line
            log.debug('message: RTCP Stat collection is active');
            wrtc.getStats(function (newStats) {
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
            if (GLOBAL.config.erizoController.report.connection_events) {  //jshint ignore:line
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
                    if (options.slideShowMode === true) {
                        that.setSlideShow(true, idSub, idPub);
                    }
                    callback('callback', {type: 'ready'});
                    break;
            }
        });
        if (options.createOffer === true) {
            log.debug('message: create offer requested, id:', wrtc.wrtcId);
            var audioEnabled = true;
            var videoEnabled = true;
            var bundle = true;
            wrtc.createOffer(audioEnabled, videoEnabled, bundle);
        }
        callback('callback', {type: 'initializing'});
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

            var eiId = from + '_' + url;

            log.info('message: Adding ExternalInput, id: ' + eiId);

            var muxer = new addon.OneToManyProcessor(),
                ei = new addon.ExternalInput(url);

            ei.wrtcId = eiId;

            publishers[from] = {muxer: muxer};
            subscribers[from] = {};

            ei.setAudioReceiver(muxer);
            ei.setVideoReceiver(muxer);
            muxer.setExternalPublisher(ei);

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
        if (publishers[to] !== undefined) {
            var eoId = url + '_' + to;
            log.info('message: Adding ExternalOutput, id: ' + eoId);
            var externalOutput = new addon.ExternalOutput(url);
            externalOutput.wrtcId = eoId;
            externalOutput.init();
            publishers[to].muxer.addExternalOutput(externalOutput, url);
            externalOutputs[url] = externalOutput;
        }
    };

    that.removeExternalOutput = function (to, url) {
        if (externalOutputs[url] !== undefined && publishers[to] !== undefined) {
            log.info('message: Stopping ExternalOutput, id: ' + externalOutputs[url].wrtcId);
            publishers[to].muxer.removeSubscriber(url);
            delete externalOutputs[url];
        }
    };

    that.processSignaling = function (streamId, peerId, msg) {
        log.info('message: Process Signaling message, ' +
                 'streamId: ' + streamId + ', peerId: ' + peerId);
        if (publishers[streamId] !== undefined) {
            if (subscribers[streamId][peerId]) {
                if (msg.type === 'offer') {
                    subscribers[streamId][peerId].setRemoteSdp(msg.sdp);
                } else if (msg.type === 'candidate') {
                    subscribers[streamId][peerId].addRemoteCandidate(msg.candidate.sdpMid,
                                                                     msg.candidate.sdpMLineIndex,
                                                                     msg.candidate.candidate);
                } else if (msg.type === 'updatestream') {
                    if(msg.sdp)
                        subscribers[streamId][peerId].setRemoteSdp(msg.sdp);
                    if (msg.config) {
                        if (msg.config.slideShowMode !== undefined) {
                            that.setSlideShow(msg.config.slideShowMode, peerId, streamId);
                        }
                    }
                }
            } else {
                if (msg.type === 'offer') {
                    publishers[streamId].wrtc.setRemoteSdp(msg.sdp);
                } else if (msg.type === 'candidate') {
                    publishers[streamId].wrtc.addRemoteCandidate(msg.candidate.sdpMid,
                                                                 msg.candidate.sdpMLineIndex,
                                                                 msg.candidate.candidate);
                } else if (msg.type === 'updatestream') {
                    if (msg.sdp) {
                        publishers[streamId].wrtc.setRemoteSdp(msg.sdp);
                    }
                    if (msg.config) {
                        if (msg.config.minVideoBW) {
                            log.debug('message: updating minVideoBW for publisher, ' +
                                      'id: ' + publishers[streamId].wrtcId + ', ' +
                                      'minVideoBW: ' + msg.config.minVideoBW);
                            publishers[streamId].minVideoBW = msg.config.minVideoBW;
                            for (var sub in subscribers[streamId]) {
                                var theConn = subscribers[streamId][sub];
                                theConn.minVideoBW = msg.config.minVideoBW * 1000; // bps
                                theConn.lowerThres = Math.floor(theConn.minVideoBW*(1-0.2));
                                theConn.upperThres = Math.ceil(theConn.minVideoBW*(1+0.1));
                            }
                        }
                    }
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
        var muxer;
        var wrtc;

        if (publishers[from] === undefined) {

            log.info('message: Adding publisher, ' +
                     'streamId: ' + from + ', ' +
                     logger.objectToLog(options) + ', ' +
                     logger.objectToLog(options.metadata));
            var wrtcId = from;
            muxer = new addon.OneToManyProcessor();
            wrtc = new addon.WebRtcConnection(wrtcId,
                                              GLOBAL.config.erizo.stunserver,
                                              GLOBAL.config.erizo.stunport,
                                              GLOBAL.config.erizo.minport,
                                              GLOBAL.config.erizo.maxport,
                                              false,
                                              JSON.stringify(GLOBAL.mediaConfig),
                                              GLOBAL.config.erizo.turnserver,
                                              GLOBAL.config.erizo.turnport,
                                              GLOBAL.config.erizo.turnusername,
                                              GLOBAL.config.erizo.turnpass);
            wrtc.wrtcId = wrtcId;

            publishers[from] = {muxer: muxer,
                                wrtc: wrtc,
                                minVideoBW: options.minVideoBW,
                                scheme: options.scheme};
            subscribers[from] = {};

            wrtc.setAudioReceiver(muxer);
            wrtc.setVideoReceiver(muxer);
            muxer.setPublisher(wrtc);

            initWebRtcConnection(wrtc, callback, from, undefined, options);

        } else {
            if (Object.keys(subscribers[from]).length === 0) {
                log.warn('message: publisher already set but no subscribers will republish, ' +
                         'code: ' + WARN_CONFLICT + ', streamId: ' + from + ', ' +
                         logger.objectToLog(options.metadata));

                wrtc = new addon.WebRtcConnection(from,
                                                  GLOBAL.config.erizo.stunserver,
                                                  GLOBAL.config.erizo.stunport,
                                                  GLOBAL.config.erizo.minport,
                                                  GLOBAL.config.erizo.maxport,
                                                  false,
                                                  JSON.stringify(GLOBAL.mediaConfig),
                                                  GLOBAL.config.erizo.turnserver,
                                                  GLOBAL.config.erizo.turnport,
                                                  GLOBAL.config.erizo.turnusername,
                                                  GLOBAL.config.erizo.turnpass);
                muxer = publishers[from].muxer;
                publishers[from].wrtc = wrtc;
                wrtc.setAudioReceiver(muxer);
                wrtc.setVideoReceiver(muxer);
                muxer.setPublisher(wrtc);

                initWebRtcConnection(wrtc, callback, from, undefined, options);
            }else{
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

        if (publishers[to] === undefined) {
            log.warn('message: addSubscriber to unknown publisher, ' +
                     'code: ' + WARN_NOT_FOUND + ', streamId: ' + to + ', clientId: ' + from +
                      ', ' + logger.objectToLog(options.metadata));
            //We may need to notify the clients
            return;
        }
        if (subscribers[to][from] !== undefined) {
            log.warn('message: Duplicated subscription will resubscribe, ' +
                     'code: ' + WARN_CONFLICT + ', streamId: ' + to + ', clientId: ' + from+
                      ', ' + logger.objectToLog(options.metadata));
            that.removeSubscriber(from,to);
        }
        var wrtcId = from + '_' + to;
        log.info('message: Adding subscriber, id: ' + wrtcId + ', ' +
                 logger.objectToLog(options)+
                  ', ' + logger.objectToLog(options.metadata));
        var wrtc = new addon.WebRtcConnection(wrtcId,
                                              GLOBAL.config.erizo.stunserver,
                                              GLOBAL.config.erizo.stunport,
                                              GLOBAL.config.erizo.minport,
                                              GLOBAL.config.erizo.maxport,
                                              false,
                                              JSON.stringify(GLOBAL.mediaConfig),
                                              GLOBAL.config.erizo.turnserver,
                                              GLOBAL.config.erizo.turnport,
                                              GLOBAL.config.erizo.turnusername,
                                              GLOBAL.config.erizo.turnpass);

        wrtc.wrtcId = wrtcId;
        subscribers[to][from] = wrtc;
        publishers[to].muxer.addSubscriber(wrtc, from);
        wrtc.minVideoBW = publishers[to].minVideoBW;
        log.debug('message: Setting scheme from publisher to subscriber, ' +
                  'id: ' + wrtcId + ', scheme: ' + publishers[to].scheme+
                   ', ' + logger.objectToLog(options.metadata));
        wrtc.scheme = publishers[to].scheme;
        initWebRtcConnection(wrtc, callback, to, from, options);
    };

    /*
     * Removes a publisher from the room. This also deletes the associated OneToManyProcessor.
     */
    that.removePublisher = function (from) {
        if (subscribers[from] !== undefined && publishers[from] !== undefined) {
            log.info('message: Removing publisher, id: ' + from);
            if(publishers[from].periodicPlis!==undefined) {
                log.debug('message: clearing periodic PLIs for publisher, id: ' + from);
                clearInterval (publishers[from].periodicPlis);
            }
            for (var key in subscribers[from]) {
                if (subscribers[from].hasOwnProperty(key)) {
                    log.info('message: Removing subscriber, id: ' + subscribers[from][key].wrtcId);
                    subscribers[from][key].close();
                }
            }
            publishers[from].wrtc.close();
            publishers[from].muxer.close(function(message) {
                log.info('message: muxer closed succesfully, ' +
                         'id: ' + from + ', ' +
                         logger.objectToLog(message));
                delete subscribers[from];
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

        if (subscribers[to] && subscribers[to][from]) {
            log.info('message: removing subscriber, id: ' + subscribers[to][from].wrtcId);
            subscribers[to][from].close();
            publishers[to].muxer.removeSubscriber(from);
            delete subscribers[to][from];
        }

        if (publishers[to] && publishers[to].wrtc.periodicPlis !== undefined) {
            for (var i in subscribers[to]) {
                if(subscribers[to][i].slideShowMode === true) {
                    return;
                }
            }
            log.debug('message: clearing Pli interval as no more ' +
                      'slideshows subscribers are present');
            clearInterval(publishers[to].wrtc.periodicPlis);
            publishers[to].wrtc.periodicPlis = undefined;
        }
    };

    /*
     * Removes all the subscribers related with a client.
     */
    that.removeSubscriptions = function (from) {
        log.info('message: removing subscriptions, peerId:', from);
        for (var to in subscribers) {
            if (subscribers.hasOwnProperty(to)) {
                if (subscribers[to][from]) {
                    log.debug('message: removing subscription, ' +
                              'id:', subscribers[to][from].wrtcId);
                    subscribers[to][from].close();
                    publishers[to].muxer.removeSubscriber(from);
                    delete subscribers[to][from];
                }
            }
        }
    };
    
    /*
     * Enables/Disables slideshow mode for a subscriber
     */
    that.setSlideShow = function (slideShowMode, from, to) {
        var wrtcPub;
        var theWrtc = subscribers[to][from];
        if (!theWrtc) {
            log.warn('message: wrtc not found for updating slideshow, ' +
                     'code: ' + WARN_NOT_FOUND + ', id: ' + from + '_' + to);
            return;
        }

        log.debug('message: setting SlideShow, id: ' + theWrtc.wrtcId +
                  ', slideShowMode: ' + slideShowMode);
        if (slideShowMode === true) {
            theWrtc.setSlideShowMode(true);
            theWrtc.slideShowMode = true;
            wrtcPub = publishers[to].wrtc;
            if (wrtcPub.periodicPlis === undefined) {
                wrtcPub.periodicPlis = setInterval(function () {
                    if(wrtcPub)
                        wrtcPub.generatePLIPacket();
                }, SLIDESHOW_TIME);
            }
        } else {
            wrtcPub = publishers[to].wrtc;
            for (var pliIndex = 0; pliIndex < PLIS_TO_RECOVER; pliIndex++) {
              wrtcPub.generatePLIPacket();
            }

            theWrtc.setSlideShowMode(false);
            theWrtc.slideShowMode = false;
            if (publishers[to].wrtc.periodicPlis !== undefined) {
                for (var i in subscribers[to]) {
                    if (subscribers[to][i].slideShowMode === true) {
                        return;
                    }
                }
                log.debug('message: clearing PLI interval for publisher slideShow, ' +
                          'id: ' + publishers[to].wrtc.wrtcId);
                clearInterval(publishers[to].wrtc.periodicPlis);
                publishers[to].wrtc.periodicPlis = undefined;
            }
        }

    };

    return that;
};
