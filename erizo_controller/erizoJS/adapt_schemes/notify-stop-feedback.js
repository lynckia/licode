'use strict';
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


    that.monitorMinVideoBw = function(wrtc, callback) {
        wrtc.bwValues = [];
        var tics = 0;
        var noFeedbackTics = 0;
        var lastAverage, average, lastBWValue;
        wrtc.bwStatus = BW_STABLE;
        log.info('message: Start wrtc adapt scheme, ' +
            'id: ' + wrtc.wrtcId + ', ' +
                'scheme: notify-stop-feedback, ' +
                'minVideoBW: ' + wrtc.minVideoBW);

        wrtc.minVideoBW = wrtc.minVideoBW*1000; // We need it in bps
        wrtc.lowerThres = Math.floor(wrtc.minVideoBW*(0.8));
        wrtc.upperThres = Math.ceil(wrtc.minVideoBW);
        var intervalId = setInterval(function () {
            var newStats = wrtc.getStats();
            if (newStats === null) {
                clearInterval(intervalId);
                return;
            }

            if (wrtc.slideShowMode) {
                return;
            }

            var theStats = JSON.parse(newStats);
            for (var i = 0; i < theStats.length; i++) {
                // Only one stream should have bandwidth
                if(theStats[i].hasOwnProperty('bandwidth')) {
                    lastBWValue = theStats[i].bandwidth;
                    wrtc.bwValues.push(lastBWValue);
                    if (wrtc.bwValues.length > 5){
                        wrtc.bwValues.shift();
                    }
                    average = calculateAverage(wrtc.bwValues);
                }
            }
            switch (wrtc.bwStatus) {
                case BW_STABLE:
                    if(average <= lastAverage && (average < wrtc.lowerThres)) {
                        if (++tics > TICS_PER_TRANSITION){
                            log.info('message: scheme state change, ' +
                                'id: ' + wrtc.wrtcId + ', ' +
                                    'previousState: BW_STABLE, ' +
                                    'newState: BW_NO_FEEDBACK, ' +
                                    'averageBandwidth: ' + average + ', ' +
                                    'lowerThreshold: ' + wrtc.lowerThres);
                            wrtc.bwStatus = BW_NO_FEEDBACK;
                            wrtc.setFeedbackReports(false, 0);
                            tics = 0;
                            noFeedbackTics = 0;
                            callback('callback', {type: 'bandwidthAlert',
                                message: 'insufficient',
                                bandwidth: average});
                        }
                    }
                    break;
                case BW_NO_FEEDBACK:
                    if (average >= wrtc.upperThres) {
                        if (++tics > TICS_PER_TRANSITION) {
                            log.info('message: scheme state change, ' +
                                'id: ' + wrtc.wrtcId + ', ' +
                                    'previousState: BW_NO_FEEDBACK, ' +
                                    'newState: BW_STABLE, ' +
                                    'averageBandwidth: ' + average + ', ' +
                                    'upperThreshold: ' + wrtc.upperThres);
                            wrtc.bwStatus = BW_STABLE;
                            wrtc.setFeedbackReports(true, 0);
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
                    log.error('Unknown BW status, id: ' + wrtc.wrtcId);
            }
            lastAverage = average;
        }, INTERVAL_STATS);
    };

    return that.monitorMinVideoBw;
};
