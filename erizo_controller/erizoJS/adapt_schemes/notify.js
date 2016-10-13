exports.MonitorSubscriber = function (log) {

    var that = {},
    INTERVAL_STATS = 1000;

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
        var ticks = 0;
        var lastAverage, average, lastBWValue, toRecover;
        log.info("message: Start wrtc adapt scheme, id: " + wrtc.wrtcId + ", scheme: notify, minVideoBW: " + wrtc.minVideoBW);

        wrtc.minVideoBW = wrtc.minVideoBW*1000; // We need it in bps
        wrtc.lowerThres = Math.floor(wrtc.minVideoBW*(1-0.2));
        wrtc.upperThres = Math.ceil(wrtc.minVideoBW);
        var intervalId = setInterval(function () {
               
            var newStats = wrtc.getStats();
            if (newStats == null){
                clearInterval(intervalId);
                return;
            }

            if (wrtc.slideShowMode===true)
                return;
            
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
            if(average <= lastAverage && (average < wrtc.lowerThres)){
                if (++ticks > 2){
                    log.debug("message: Insufficient Bandwidth, id: " + wrtc.wrtcId + ", averageBandwidth: " + average + ", lowerThreshold: " + wrtc.lowerThres);
                    ticks = 0;
                    callback('callback', {type:'bandwidthAlert', message:'insufficient', bandwidth: average});
                }
            }                            
            lastAverage = average;
        }, INTERVAL_STATS);
    };

    return that.monitorMinVideoBw;
}
