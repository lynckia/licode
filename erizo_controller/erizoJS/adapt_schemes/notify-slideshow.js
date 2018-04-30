'use strict';
const schemeHelpers = require('./schemeHelpers.js').schemeHelpers;
exports.MonitorSubscriber = function (log) {

  var that = {},
  INTERVAL_STATS = 1000,
  TICKS_PER_TRANSITION = 10;

  /* BW Status
  * 0 - Stable
  * 1 - SlideShow
  */
  var BW_STABLE = 0, BW_SLIDESHOW = 1;

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
    var ticks = 0;
    var retries = 0;
    var lastAverage, average, lastBWValue;
    var nextRetry = 0;
    mediaStream.bwStatus = BW_STABLE;
    log.info('message: Start wrtc adapt scheme, ' +
    'id: ' + mediaStream.id + ', ' +
    'scheme: notify-slideshow, ' +
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
              if (++ticks > TICKS_PER_TRANSITION){
                log.info('message: scheme state change, ' +
                'id: ' + mediaStream.id + ', ' +
                'previousState: BW_STABLE, ' +
                'newState: BW_SLIDESHOW, ' +
                'averageBandwidth: ' + average + ', ' +
                'lowerThreshold: ' + mediaStream.lowerThres);
                mediaStream.bwStatus = BW_SLIDESHOW;
                mediaStream.setFeedbackReports(false, 1);
                ticks = 0;
                callback('callback', {type: 'bandwidthAlert',
                message: 'insufficient',
                bandwidth: average});
              }
            }
            break;
          case BW_SLIDESHOW:
            log.info('message: Switched to audio-only, ' +
            'id: ' + mediaStream.id + ', ' +
            'state: BW_SLIDESHOW, ' +
            'averageBandwidth: ' + average + ', ' +
            'lowerThreshold: ' + mediaStream.lowerThres);
            ticks = 0;
            nextRetry = 0;
            retries = 0;
            average = 0;
            lastAverage = 0;
            mediaStream.minVideoBW = false;
            callback('scheme-slideshow-change', {enabled: true});
            callback('callback', {type: 'bandwidthAlert',
                                  message: 'slideshow',
                                  bandwidth: average});
            clearInterval(mediaStream.monitorInterval);
            break;
          default:
            log.error('Unknown BW status, id: ' + mediaStream.id);
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
