
const schemeHelpers = {};

schemeHelpers.getBandwidthStat = mediaStream => new Promise((resolve, reject) => {
  mediaStream.getStats((statsString) => {
    if (!statsString) {
      reject('no stats');
    }
    const newStats = JSON.parse(statsString);
    if (Object.prototype.hasOwnProperty.call(newStats, 'total')) {
      resolve(newStats.total.senderBitrateEstimation);
    }
    resolve(false);
  });
});

exports.schemeHelpers = schemeHelpers;
