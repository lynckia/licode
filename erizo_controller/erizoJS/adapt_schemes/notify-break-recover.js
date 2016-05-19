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
        var onlyNotifyBW= true; //TODO: For now we only notify, study if we want to enable the rest
        var isReporting = true;
        var ticks = 0;
        var retries = 0;
        var ticksToTry = 0;
        var lastAverage, average, lastBWValue, toRecover;
        var nextRetry = 0;
        wrtc.bwStatus = BW_STABLE;
        log.debug("Start wrtc adapt scheme - notify-break-recover", wrtc.minVideoBW);

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
            toRecover = (average/4)<MIN_RECOVER_BW?(average/4):MIN_RECOVER_BW;
            switch (wrtc.bwStatus){
                case BW_STABLE:
                    if(average <= lastAverage && (average < wrtc.lowerThres)){
                        if (++ticks > 2){
                            log.debug("STABLE STATE, Bandwidth is insufficient, moving to state BW_INSUFFICIENT", average, "lowerThres", wrtc.lowerThres);
                            if (!onlyNotifyBW){
                                wrtc.bwStatus = BW_INSUFFICIENT;
                                wrtc.setFeedbackReports(false, toRecover);
                            }
                            ticks = 0;
                            callback('callback', {type:'bandwidthAlert', message:'insufficient', bandwidth: average});
                        }
                    }                            
                    break;
                case BW_INSUFFICIENT:
                    if(average > wrtc.upperThres){
                        log.debug("BW_INSUFFICIENT State: we have recovered", average, "lowerThres", wrtc.lowerThres);
                        ticks = 0;
                        nextRetry = 0;
                        retries = 0;
                        wrtc.bwStatus = BW_STABLE;
                        wrtc.setFeedbackReports(true, 0);
                        callback('callback', {type:'bandwidthAlert', message:'recovered', bandwidth: average});
                    }
                    else if (retries>=3){
                        log.debug("BW_INSUFFICIENT State: moving to won't recover", average, "lowerThres", wrtc.lowerThres);
                        wrtc.bwStatus = BW_WONTRECOVER; 
                    }
                    else if (nextRetry === 0){  //schedule next retry
                        nextRetry = ticks + 20;
                    }
                    else if (++ticks == nextRetry){  // next retry is in order
                        wrtc.bwStatus = BW_RECOVERING;
                        ticksToTry = ticks + 10;
                        wrtc.setFeedbackReports (false, average);                                
                    }
                    break;
                case BW_RECOVERING:
                    log.debug("In RECOVERING state lastValue", lastBWValue, "lastAverage", lastAverage, "lowerThres", wrtc.lowerThres);
                    if(average > wrtc.upperThres){ 
                        log.debug("BW_RECOVERING State: we have recovered", average, "lowerThres", wrtc.lowerThres);
                        ticks = 0;
                        nextRetry = 0;
                        retries = 0;
                        wrtc.bwStatus = BW_STABLE;
                        wrtc.setFeedbackReports(true, 0);
                        callback('callback', {type:'bandwidthAlert', message:'recovered', bandwidth: average});
                    }
                    else if (average> lastAverage){ //we are recovering
                        log.debug("BW_RECOVERING State: we have improved, more trying time", average, "lowerThres", wrtc.lowerThres);
                        wrtc.setFeedbackReports(false, average*(1+0.3));
                        ticksToTry=ticks+10;

                    }
                    else if (++ticks >= ticksToTry){ //finish this retry
                        log.debug("BW_RECOVERING State: Finished this retry", retries, average, "lowerThres", wrtc.lowerThres);
                        ticksToTry = 0;
                        nextRetry = 0;
                        retries++;
                        wrtc.bwStatus = BW_INSUFFICIENT;
                        wrtc.setFeedbackReports (false, toRecover);
                    }
                    break;
                case BW_WONTRECOVER:
                    log.debug("BW_WONTRECOVER", average, "lowerThres", wrtc.lowerThres);
                    ticks = 0;
                    nextRetry = 0;
                    retries = 0;
                    average = 0;
                    lastAverage = 0;
                    wrtc.bwStatus = BW_STABLE;
                    wrtc.minVideoBW = false;                      
                    wrtc.setFeedbackReports (false, 1);
                    callback('callback', {type:'bandwidthAlert', message:'audio-only', bandwidth: average});
                    break;
                default:
                    log.error("Unknown BW status");
            }
            lastAverage = average;
        }, INTERVAL_STATS);
    };

    return that.monitorMinVideoBw;
}
