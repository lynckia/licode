'use strict';
const schemeHelpers = require('./schemeHelpers.js').schemeHelpers;
exports.MonitorSubscriber = function (log) {

  var that = {},
  INTERVAL_STATS = 1000,
  MIN_RECOVER_BW = 50000,
  TICS_PER_TRANSITON = 10;


  /* BW Status
  * 0 - Stable
  * 1 - Insufficient Bandwidth
  * 2 - Trying recovery
  * 3 - Won't recover
  */
  var BW_STABLE = 0, BW_INSUFFICIENT = 1, BW_RECOVERING = 2, BW_WONTRECOVER = 3;

  var calculateAverage = function (values) {

    if (values === undefined)
    return 0;
    var cnt = values.length;
    var tot = parseInt(0);
    for (var i = 0; i < values.length; i++) {
      tot += parseInt(values[i]);
    }
    return Math.ceil(tot/cnt);
  };


  that.monitorMinVideoBw = function(wrtc, callback) {
    wrtc.bwValues = [];
    var tics = 0;
    var retries = 0;
    var ticsToTry = 0;
    var lastAverage, average, lastBWValue, toRecover;
    var nextRetry = 0;
    wrtc.bwStatus = BW_STABLE;
    log.info('message: Start wrtc adapt scheme, ' +
    'id: ' + wrtc.wrtcId + ', ' +
    'scheme: notify-break-recover, minVideoBW: ' + wrtc.minVideoBW);

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
        toRecover = (average/4)<MIN_RECOVER_BW?(average/4):MIN_RECOVER_BW;
        switch (wrtc.bwStatus){
          case BW_STABLE:
            if (average <= lastAverage && (average < wrtc.lowerThres)) {
              if (++tics > TICS_PER_TRANSITON) {
                log.info('message: scheme state change, ' +
                'id: ' + wrtc.wrtcId + ', ' +
                'previousState: BW_STABLE, ' +
                'newState: BW_INSUFFICIENT, ' +
                'averageBandwidth: ' + average + ', ' +
                'lowerThreshold: ' + wrtc.lowerThres);
                wrtc.bwStatus = BW_INSUFFICIENT;
                wrtc.setFeedbackReports(false, toRecover);
                tics = 0;
                callback('callback', {type: 'bandwidthAlert',
                message: 'insufficient',
                bandwidth: average});
              }
            }
            break;
          case BW_INSUFFICIENT:
            if (average > wrtc.upperThres) {
              log.info('message: scheme state change, ' +
              'id: ' + wrtc.wrtcId + ', ' +
              'previousState: BW_INSUFFICIENT, ' +
              'newState: BW_STABLE, ' +
              'averageBandwidth: ' + average + ', ' +
              'lowerThreshold: ' + wrtc.lowerThres);
              tics = 0;
              nextRetry = 0;
              retries = 0;
              wrtc.bwStatus = BW_STABLE;
              wrtc.setFeedbackReports(true, 0);
              callback('callback', {type:'bandwidthAlert',
              message:'recovered',
              bandwidth: average});
            }
            else if (retries >= 3) {
              log.info('message: scheme state change, ' +
              'id: ' + wrtc.wrtcId + ', ' +
              'previousState: BW_INSUFFICIENT, ' +
              'newState: WONT_RECOVER, ' +
              'averageBandwidth: ' + average + ', ' +
              'lowerThreshold: ' + wrtc.lowerThres);
              wrtc.bwStatus = BW_WONTRECOVER;
            }
            else if (nextRetry === 0) {  //schedule next retry
              nextRetry = tics + 20;
            }
            else if (++tics === nextRetry) {  // next retry is in order
              wrtc.bwStatus = BW_RECOVERING;
              ticsToTry = tics + 10;
              wrtc.setFeedbackReports (false, average);
            }
            break;
          case BW_RECOVERING:
            log.info('message: trying to recover, ' +
            'id: ' + wrtc.wrtcId + ', ' +
            'state: BW_RECOVERING, ' +
            'lastBandwidthValue: ' + lastBWValue + ', ' +
            'lastAverageBandwidth: ' + lastAverage + ', ' +
            'lowerThreshold: ' + wrtc.lowerThres);
            if(average > wrtc.upperThres){
              log.info('message: recovered, ' +
              'id: ' + wrtc.wrtcId + ', ' +
              'state: BW_RECOVERING, ' +
              'newState: BW_STABLE, ' +
              'averageBandwidth: ' + average + ', ' +
              'lowerThreshold: ' + wrtc.lowerThres);
              tics = 0;
              nextRetry = 0;
              retries = 0;
              wrtc.bwStatus = BW_STABLE;
              wrtc.setFeedbackReports(true, 0);
              callback('callback', {type: 'bandwidthAlert',
              message: 'recovered',
              bandwidth: average});
            }
            else if (average> lastAverage) { //we are recovering
              log.info('message: bw improvement, ' +
              'id: ' + wrtc.wrtcId + ', ' +
              'state: BW_RECOVERING, ' +
              'averageBandwidth: ' + average + ', ' +
              'lowerThreshold: ' + wrtc.lowerThres);
              wrtc.setFeedbackReports(false, average*(1+0.3));
              ticsToTry=tics+10;

            }
            else if (++tics >= ticsToTry) { //finish this retry
              log.info('message: recovery tic passed, ' +
              'id: ' + wrtc.wrtcId + ', ' +
              'state: BW_RECOVERING, ' +
              'numberOfRetries: ' + retries + ', ' +
              'averageBandwidth: ' + average + ', ' +
              'lowerThreshold: ' + wrtc.lowerThres);
              ticsToTry = 0;
              nextRetry = 0;
              retries++;
              wrtc.bwStatus = BW_INSUFFICIENT;
              wrtc.setFeedbackReports (false, toRecover);
            }
            break;
          case BW_WONTRECOVER:
            log.info('message: Stop trying to recover, ' +
            'id: ' + wrtc.wrtcId + ', ' +
            'state: BW_WONT_RECOVER, ' +
            'averageBandwidth: ' + average + ', ' +
            'lowerThreshold: ' + wrtc.lowerThres);
            tics = 0;
            nextRetry = 0;
            retries = 0;
            average = 0;
            lastAverage = 0;
            wrtc.bwStatus = BW_STABLE;
            wrtc.minVideoBW = false;
            wrtc.setFeedbackReports (false, 1);
            callback('callback', {type: 'bandwidthAlert',
            message: 'audio-only',
            bandwidth: average});
            break;
          default:
            log.error('message: Unknown BW status, id: ' + wrtc.wrtcId);
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
