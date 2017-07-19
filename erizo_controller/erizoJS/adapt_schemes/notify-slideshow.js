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


  that.monitorMinVideoBw = function(wrtc, callback, idPub, idSub, options, erizoJsController) {
    wrtc.bwValues = [];
    var ticks = 0;
    var retries = 0;
    var lastAverage, average, lastBWValue;
    var nextRetry = 0;
    wrtc.bwStatus = BW_STABLE;
    log.info('message: Start wrtc adapt scheme, ' +
    'id: ' + wrtc.wrtcId + ', ' +
    'scheme: notify-slideshow, ' +
    'minVideoBW: ' + wrtc.minVideoBW);

    wrtc.minVideoBW = wrtc.minVideoBW*1000; // We need it in bps
    wrtc.lowerThres = Math.floor(wrtc.minVideoBW*(0.8));
    wrtc.upperThres = Math.ceil(wrtc.minVideoBW);
    wrtc.monitorInterval = setInterval(() => {

      schemeHelpers.getBandwidthStat(wrtc).then((bandwidth) => {
        if (wrtc.slideShowMode) {
          return;
        }
        if(bandwidth) {
          lastBWValue = bandwidth;
          wrtc.bwValues.push(lastBWValue);
          if (wrtc.bwValues.length > 5) {
            wrtc.bwValues.shift();
          }
          average = calculateAverage(wrtc.bwValues);
        }

        switch (wrtc.bwStatus) {
          case BW_STABLE:
            if(average <= lastAverage && (average < wrtc.lowerThres)) {
              if (++ticks > TICKS_PER_TRANSITION){
                log.info('message: scheme state change, ' +
                'id: ' + wrtc.wrtcId + ', ' +
                'previousState: BW_STABLE, ' +
                'newState: BW_SLIDESHOW, ' +
                'averageBandwidth: ' + average + ', ' +
                'lowerThreshold: ' + wrtc.lowerThres);
                wrtc.bwStatus = BW_SLIDESHOW;
                wrtc.setFeedbackReports(false, 1);
                ticks = 0;
                callback('callback', {type: 'bandwidthAlert',
                message: 'insufficient',
                bandwidth: average});
              }
            }
            break;
          case BW_SLIDESHOW:
            log.info('message: Switched to audio-only, ' +
            'id: ' + wrtc.wrtcId + ', ' +
            'state: BW_SLIDESHOW, ' +
            'averageBandwidth: ' + average + ', ' +
            'lowerThreshold: ' + wrtc.lowerThres);
            ticks = 0;
            nextRetry = 0;
            retries = 0;
            average = 0;
            lastAverage = 0;
            wrtc.minVideoBW = false;
            erizoJsController.setSlideShow(true, idSub, idPub);
            callback('callback', {type: 'bandwidthAlert',
            message: 'slideshow',
            bandwidth: average});
            clearInterval(wrtc.monitorInterval);
            break;
          default:
            log.error('Unknown BW status, id: ' + wrtc.wrtcId);
        }
        lastAverage = average;
      }).catch((reason) => {
        clearInterval(wrtc.monitorInterval);
        log.error('error getting stats: ' + reason);
      });
    }, INTERVAL_STATS);
  };

  return that.monitorMinVideoBw;
};
