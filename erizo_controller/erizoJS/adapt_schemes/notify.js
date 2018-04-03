'use strict';
const schemeHelpers = require('./schemeHelpers.js').schemeHelpers;
exports.MonitorSubscriber = function (log) {

    var that = {},
        INTERVAL_STATS = 1000,
        TICS_PER_TRANSITION = 10;

    var calculateAverage = function (values) {

        if (values === undefined)
            return 0;
        var cnt = values.length;
        var tot = parseInt(0);
        for (var i = 0; i < values.length; i++){
            tot += parseInt(values[i]);
        }
        return Math.ceil(tot/cnt);
    };


    that.monitorMinVideoBw = function(mediaStream, callback) {
        mediaStream.bwValues = [];
        var tics = 0;
        var lastAverage, average, lastBWValue;
        log.info('message: Start wrtc adapt scheme, ' +
                 'id: ' + mediaStream.id + ', ' +
                'scheme: notify, minVideoBW: ' + mediaStream.minVideoBW);

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

              log.debug('message: Measuring interval, average: ' + average);
              if (average <= lastAverage && (average < mediaStream.lowerThres)) {
                if (++tics > TICS_PER_TRANSITION){
                  log.info('message: Insufficient Bandwidth, ' +
                  'id: ' + mediaStream.id + ', ' +
                  'averageBandwidth: ' + average + ', ' +
                  'lowerThreshold: ' + mediaStream.lowerThres);
                  tics = 0;
                  callback('callback', {type: 'bandwidthAlert',
                  message: 'insufficient',
                  bandwidth: average});
                }
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
