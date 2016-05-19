exports.MonitorSubscriber = function (log) {

    var that = {},
    INTERVAL_STATS = 1000,
    MIN_RECOVER_BW = 50000;

    /* BW Status
     * 0 - Stable 
     * 1 - Insufficient Bandwidth 
     * 2 - Trying recovery
     * 3 - Won't recover
     */
    var BW_STABLE = 0, BW_INSUFFICIENT = 1, BW_RECOVERING = 2, BW_WONTRECOVER = 3;
    
    var calculateAverage = function (values) { 

        if (values.length === undefined)
            return 0;
        var cnt = values.length;
        var tot = parseInt(0);
        for (var i = 0; i < values.length; i++){
            tot+=parseInt(values[i]);
        }
        return Math.ceil(tot/cnt);
    };
    

    that.monitorMinVideoBw = function(wrtc, callback){
        wrtc.bwValues = [];
        var isReporting = true;
        var ticks = 0;
        var retries = 0;
        var ticksToTry = 0;
        var lastAverage, average, lastBWValue, toRecover;
        var nextRetry = 0;
        wrtc.bwStatus = BW_STABLE;
        log.debug("Start wrtc adapt scheme - notify-break", wrtc.minVideoBW);

        wrtc.minVideoBW = wrtc.minVideoBW*1000; // We need it in bps
        wrtc.lowerThres = Math.floor(wrtc.minVideoBW*(1-0.2));
        wrtc.upperThres = Math.ceil(wrtc.minVideoBW);
        var intervalId = setInterval(function () {
            var newStats = wrtc.getStats();
            if (newStats == null){
                log.debug("Stopping BW Monitoring");
                clearInterval(intervalId);
                return;
            }

            var theStats = JSON.parse(newStats);
            for (var i = 0; i < theStats.length; i++){ 
                if(theStats[i].hasOwnProperty('bandwidth')){   // Only one stream should have bandwidth
                    lastBWValue = theStats[i].bandwidth;
                    wrtc.bwValues.push(lastBWValue);
                    if (wrtc.bwValues.length > 5){
                        wrtc.bwValues.shift();
                    }
                    average = calculateAverage(wrtc.bwValues);
                }
            }
            switch (wrtc.bwStatus){
                case BW_STABLE:
                    if(average <= lastAverage && (average < wrtc.lowerThres)){
                        if (++ticks > 2){
                            log.debug("STABLE STATE, Bandwidth is insufficient will stop sending video", average, "lowerThres", wrtc.lowerThres);
                            wrtc.bwStatus = BW_WONTRECOVER;
                            wrtc.setFeedbackReports(false, 1);
                            ticks = 0;
                            callback('callback', {type:'bandwidthAlert', message:'insufficient', bandwidth: average});
                        }
                    }                            
                    break;
                case BW_WONTRECOVER:
                    log.debug("Switched to audio-only mode with no recovery", average);
                    ticks = 0;
                    nextRetry = 0;
                    retries = 0;
                    average = 0;
                    lastAverage = 0;
                    wrtc.minVideoBW = false;
                    wrtc.setFeedbackReports (false, 1);
                    callback('callback', {type:'bandwidthAlert', message:'audio-only', bandwidth: average});
                    clearInterval(intervalId);
                    break;
                default:
                    log.error("Unknown BW status");
            }
            lastAverage = average;
        }, INTERVAL_STATS);
    };

    return that.monitorMinVideoBw;
}
