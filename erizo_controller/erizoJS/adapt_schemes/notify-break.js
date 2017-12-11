'use strict';
const schemeHelpers = require('./schemeHelpers.js').schemeHelpers;
exports.MonitorSubscriber = function (log) {

  var that = {},
  INTERVAL_STATS = 1000,
  TICS_PER_TRANSITION = 10;


  /* BW Status
  * 0 - Stable
  * 1 - Won't recover
  */
  var BW_STABLE = 0, BW_WONTRECOVER = 1;

  var calculateAverage = function (values) {

    if (values === undefined)
    return 0;
    var cnt = values.length;
    var tot = parseInt(0);
    for (var i = 0; i < values.length; i++){
      tot+=parseInt(values[i]);
    }
    return Math.ceil(tot/cnt);
  };


  that.monitorMinVideoBw = function(mediaStream, callback){
    mediaStream.bwValues = [];
    var tics = 0;
    var retries = 0;
    var lastAverage, average, lastBWValue;
    var nextRetry = 0;
    mediaStream.bwStatus = BW_STABLE;
    log.info('message: Start wrtc adapt scheme, ' +
    'id: ' + mediaStream.wrtcId + ', ' +
    'scheme: notify-break, ' +
    'minVideoBW: ' + mediaStream.minVideoBW);

    mediaStream.minVideoBW = mediaStream.minVideoBW*1000; // We need it in bps
    mediaStream.lowerThres = Math.floor(mediaStream.minVideoBW*(0.8));
    mediaStream.upperThres = Math.ceil(mediaStream.minVideoBW);
    mediaStream.monitorInterval = setInterval(() => {

      schemeHelpers.getBandwidthStat(mediaStream).then((bandwidth) => {
        if (mediaStream.slideShowMode) {
          return;
        }
        if(bandwidth) {
          lastBWValue = bandwidth;
          mediaStream.bwValues.push(lastBWValue);
          if (mediaStream.bwValues.length > 5) {
            mediaStream.bwValues.shift();
          }
          average = calculateAverage(mediaStream.bwValues);
        }

        switch (mediaStream.bwStatus){
          case BW_STABLE:
            if(average <= lastAverage && (average < mediaStream.lowerThres)) {
              if (++tics > TICS_PER_TRANSITION) {
                log.info('message: scheme state change, ' +
                'id: ' + mediaStream.wrtcId + ', ' +
                'previousState: BW_STABLE, ' +
                'newState: BW_WONT_RECOVER, ' +
                'averageBandwidth: ' + average + ', ' +
                'lowerThreshold: ' + mediaStream.lowerThres);
                mediaStream.bwStatus = BW_WONTRECOVER;
                mediaStream.setFeedbackReports(false, 1);
                tics = 0;
                callback('callback', {type: 'bandwidthAlert',
                message: 'insufficient',
                bandwidth: average});
              }
            }
            break;
          case BW_WONTRECOVER:
            log.info('message: Switched to audio-only, ' +
            'id: ' + mediaStream.wrtcId + ', ' +
            'state: BW_WONT_RECOVER, ' +
            'averageBandwidth: ' + average + ', ' +
            'lowerThreshold: ' + mediaStream.lowerThres);
            tics = 0;
            nextRetry = 0;
            retries = 0;
            average = 0;
            lastAverage = 0;
            mediaStream.minVideoBW = false;
            mediaStream.setFeedbackReports (false, 1);
            callback('callback', {type: 'bandwidthAlert',
            message: 'audio-only',
            bandwidth: average});
            clearInterval(mediaStream.monitorInterval);
            break;
          default:
            log.error('Unknown BW status, id: ' + mediaStream.wrtcId);
        }
        lastAverage = average;
      }).catch((reason) => {
        clearInterval(mediaStream.monitorInterval);
        log.error('error getting stats: ' + reason);
      });
    }, INTERVAL_STATS);
  };

  return that.monitorMinVideoBw;
};
