/* eslint-disable no-param-reassign */

const schemeHelpers = require('./schemeHelpers.js').schemeHelpers;

exports.MonitorSubscriber = (log) => {
  const that = {};
  const INTERVAL_STATS = 1000;
  const TICS_PER_TRANSITION = 10;

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
    log.info('message: Start wrtc adapt scheme, ' +
      `id: ${mediaStream.id}, ` +
      `scheme: notify, minVideoBW: ${mediaStream.minVideoBW}`);

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

        log.debug(`message: Measuring interval, average: ${average}`);
        if (average <= lastAverage && (average < mediaStream.lowerThres)) {
          if ((tics += 1) > TICS_PER_TRANSITION) {
            log.info('message: Insufficient Bandwidth, ' +
              `id: ${mediaStream.id}, ` +
              `averageBandwidth: ${average}, ` +
              `lowerThreshold: ${mediaStream.lowerThres}`);
            tics = 0;
            callback('callback', { type: 'bandwidthAlert',
              message: 'insufficient',
              bandwidth: average });
          }
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
