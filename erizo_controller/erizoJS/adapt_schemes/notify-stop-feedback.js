'use strict';
const schemeHelpers = require('./schemeHelpers.js').schemeHelpers;
exports.MonitorSubscriber = function (log) {

  var that = {},
  INTERVAL_STATS = 1000,
  TICS_PER_TRANSITION = 10;

  /* BW Status
  * 0 - Stable
  * 1 - Stopped Feedback
  */
  var BW_STABLE = 0, BW_NO_FEEDBACK = 1;

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


  that.monitorMinVideoBw = function(mediaStream, callback) {
    mediaStream.bwValues = [];
    var tics = 0;
    var noFeedbackTics = 0;
    var lastAverage, average, lastBWValue;
    mediaStream.bwStatus = BW_STABLE;
    log.info('message: Start wrtc adapt scheme, ' +
    'id: ' + mediaStream.wrtcId + ', ' +
    'scheme: notify-stop-feedback, ' +
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

        switch (mediaStream.bwStatus) {
          case BW_STABLE:
            if(average <= lastAverage && (average < mediaStream.lowerThres)) {
              if (++tics > TICS_PER_TRANSITION){
                log.info('message: scheme state change, ' +
                'id: ' + mediaStream.wrtcId + ', ' +
                'previousState: BW_STABLE, ' +
                'newState: BW_NO_FEEDBACK, ' +
                'averageBandwidth: ' + average + ', ' +
                'lowerThreshold: ' + mediaStream.lowerThres);
                mediaStream.bwStatus = BW_NO_FEEDBACK;
                mediaStream.setFeedbackReports(false, 0);
                tics = 0;
                noFeedbackTics = 0;
                callback('callback', {type: 'bandwidthAlert',
                message: 'insufficient',
                bandwidth: average});
              }
            }
            break;
          case BW_NO_FEEDBACK:
            if (average >= mediaStream.upperThres) {
              if (++tics > TICS_PER_TRANSITION) {
                log.info('message: scheme state change, ' +
                'id: ' + mediaStream.wrtcId + ', ' +
                'previousState: BW_NO_FEEDBACK, ' +
                'newState: BW_STABLE, ' +
                'averageBandwidth: ' + average + ', ' +
                'upperThreshold: ' + mediaStream.upperThres);
                mediaStream.bwStatus = BW_STABLE;
                mediaStream.setFeedbackReports(true, 0);
                tics = 0;
                noFeedbackTics = 0;
                callback('callback', {type: 'bandwidthAlert',
                message: 'recovered',
                bandwidth: average});
              }
            } else {
              if (++noFeedbackTics > TICS_PER_TRANSITION) {
                callback('callback', {type: 'bandwidthAlert',
                message: 'stop-feedback',
                bandwidth: average});
                noFeedbackTics = 0;
              }
            }
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
