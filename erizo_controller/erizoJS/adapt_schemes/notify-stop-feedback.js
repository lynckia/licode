/* eslint-disable no-param-reassign */

const schemeHelpers = require('./schemeHelpers.js').schemeHelpers;

exports.MonitorSubscriber = (log) => {
  const that = {};
  const INTERVAL_STATS = 1000;
  const TICS_PER_TRANSITION = 10;

  /* BW Status
   * 0 - Stable
   * 1 - Stopped Feedback
   */
  const BW_STABLE = 0;
  const BW_NO_FEEDBACK = 1;

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
    let noFeedbackTics = 0;
    let lastAverage;
    let average;
    let lastBWValue;
    mediaStream.bwStatus = BW_STABLE;
    log.info('message: Start wrtc adapt scheme, ' +
      `id: ${mediaStream.id}, ` +
      'scheme: notify-stop-feedback, ' +
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
                  'newState: BW_NO_FEEDBACK, ' +
                  `averageBandwidth: ${average}, ` +
                  `lowerThreshold: ${mediaStream.lowerThres}`);
                mediaStream.bwStatus = BW_NO_FEEDBACK;
                mediaStream.setFeedbackReports(false, 0);
                tics = 0;
                noFeedbackTics = 0;
                callback('callback', { type: 'bandwidthAlert',
                  message: 'insufficient',
                  bandwidth: average });
              }
            }
            break;
          case BW_NO_FEEDBACK:
            if (average >= mediaStream.upperThres) {
              if ((tics += 1) > TICS_PER_TRANSITION) {
                log.info('message: scheme state change, ' +
                  `id: ${mediaStream.id}, ` +
                  'previousState: BW_NO_FEEDBACK, ' +
                  'newState: BW_STABLE, ' +
                  `averageBandwidth: ${average}, ` +
                  `upperThreshold: ${mediaStream.upperThres}`);
                mediaStream.bwStatus = BW_STABLE;
                mediaStream.setFeedbackReports(true, 0);
                tics = 0;
                noFeedbackTics = 0;
                callback('callback', { type: 'bandwidthAlert',
                  message: 'recovered',
                  bandwidth: average });
              }
            } else if ((noFeedbackTics += 1) > TICS_PER_TRANSITION) {
              callback('callback', { type: 'bandwidthAlert',
                message: 'stop-feedback',
                bandwidth: average });
              noFeedbackTics = 0;
            }
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
