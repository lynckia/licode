'use strict';
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


    that.monitorMinVideoBw = function(wrtc, callback) {
        wrtc.bwValues = [];
        var tics = 0;
        var lastAverage, average, lastBWValue;
        log.info('message: Start wrtc adapt scheme, ' +
                 'id: ' + wrtc.wrtcId + ', ' +
                'scheme: notify, minVideoBW: ' + wrtc.minVideoBW);

        wrtc.minVideoBW = wrtc.minVideoBW*1000; // We need it in bps
        wrtc.lowerThres = Math.floor(wrtc.minVideoBW*(0.8));
        wrtc.upperThres = Math.ceil(wrtc.minVideoBW);
        var intervalId = setInterval(function() {
            var newStats = wrtc.getStats();
            if (!newStats){
                clearInterval(intervalId);
                return;
            }

            if (wrtc.slideShowMode) {
                return;
            }
            var theStats = JSON.parse(newStats);
            for (var i = 0; i < theStats.length; i++){
                // Only one stream should have bandwidth
                if(theStats[i].hasOwnProperty('bandwidth')) {
                    lastBWValue = theStats[i].bandwidth;
                    wrtc.bwValues.push(lastBWValue);
                    if (wrtc.bwValues.length > 5) {
                        wrtc.bwValues.shift();
                    }
                    average = calculateAverage(wrtc.bwValues);
                }
            }

            log.debug('message: Measuring interval, average: ' + average); 
            if (average <= lastAverage && (average < wrtc.lowerThres)) {
                if (++tics > TICS_PER_TRANSITION){
                    log.info('message: Insufficient Bandwidth, ' +
                             'id: ' + wrtc.wrtcId + ', ' +
                             'averageBandwidth: ' + average + ', ' +
                             'lowerThreshold: ' + wrtc.lowerThres);
                    tics = 0;
                    callback('callback', {type: 'bandwidthAlert',
                                          message: 'insufficient',
                                          bandwidth: average});
                }
            }
            lastAverage = average;
        }, INTERVAL_STATS);
    };

    return that.monitorMinVideoBw;
};
