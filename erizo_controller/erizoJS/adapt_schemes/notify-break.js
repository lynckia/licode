/* eslint-disable no-param-reassign */

const schemeHelpers = require('./schemeHelpers.js').schemeHelpers;

exports.MonitorSubscriber = (log) => {
  const that = {};
  const INTERVAL_STATS = 1000;
  const TICS_PER_TRANSITION = 10;

  /* BW Status
   * 0 - Stable
   * 1 - Won't recover
   */
  const BW_STABLE = 0;
  const BW_WONTRECOVER = 1;

  const calculateAverage = (values) => {
    if (values === undefined) { return 0; }
    const cnt = values.length;
    let tot = parseInt(0, 10);
    for (let i = 0; i < values.length; i += 1) {
      tot += parseInt(values[i], 10);
    }
    return Math.ceil(tot / cnt);
  };


  that.monitorMinVideoBw = (mediaStream, callback) => {
    mediaStream.bwValues = [];
    let tics = 0;
    let lastAverage;
    let average;
    let lastBWValue;
    mediaStream.bwStatus = BW_STABLE;
    log.info('message: Start wrtc adapt scheme, ' +
      `id: ${mediaStream.id}, ` +
      'scheme: notify-break, ' +
      `minVideoBW: ${mediaStream.minVideoBW}`);

    mediaStream.minVideoBW *= 1000; // We need it in bps
    mediaStream.lowerThres = Math.floor(mediaStream.minVideoBW * (0.8));
    mediaStream.upperThres = Math.ceil(mediaStream.minVideoBW);
    mediaStream.monitorInterval = setInterval(() => {
      schemeHelpers.getBandwidthStat(mediaStream).then((bandwidth) => {
        if (mediaStream.slideShowMode) {
          return;
        }
        if (bandwidth) {
          lastBWValue = bandwidth;
          mediaStream.bwValues.push(lastBWValue);
          if (mediaStream.bwValues.length > 5) {
            mediaStream.bwValues.shift();
          }
          average = calculateAverage(mediaStream.bwValues);
        }

        switch (mediaStream.bwStatus) {
          case BW_STABLE:
            if (average <= lastAverage && (average < mediaStream.lowerThres)) {
              if ((tics += 1) > TICS_PER_TRANSITION) {
                log.info('message: scheme state change, ' +
                  `id: ${mediaStream.id}, ` +
                  'previousState: BW_STABLE, ' +
                  'newState: BW_WONT_RECOVER, ' +
                  `averageBandwidth: ${average}, ` +
                  `lowerThreshold: ${mediaStream.lowerThres}`);
                mediaStream.bwStatus = BW_WONTRECOVER;
                mediaStream.setFeedbackReports(false, 1);
                tics = 0;
                callback('callback', { type: 'bandwidthAlert',
                  message: 'insufficient',
                  bandwidth: average });
              }
            }
            break;
          case BW_WONTRECOVER:
            log.info('message: Switched to audio-only, ' +
              `id: ${mediaStream.id}, ` +
              'state: BW_WONT_RECOVER, ' +
              `averageBandwidth: ${average}, ` +
              `lowerThreshold: ${mediaStream.lowerThres}`);
            tics = 0;
            average = 0;
            lastAverage = 0;
            mediaStream.minVideoBW = false;
            mediaStream.setFeedbackReports(false, 1);
            callback('callback', { type: 'bandwidthAlert',
              message: 'audio-only',
              bandwidth: average });
            clearInterval(mediaStream.monitorInterval);
            break;
          default:
            log.error(`Unknown BW status, id: ${mediaStream.id}`);
        }
        lastAverage = average;
      }).catch((reason) => {
        clearInterval(mediaStream.monitorInterval);
        log.error(`error getting stats: ${reason}`);
      });
    }, INTERVAL_STATS);
  };

  return that.monitorMinVideoBw;
};
