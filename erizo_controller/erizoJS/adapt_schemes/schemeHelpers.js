'use strict';
const schemeHelpers = {};

schemeHelpers.getBandwidthStat = (wrtc) => {
  return new Promise((resolve, reject) => {
    wrtc.getStats( (statsString) => {
      if (!statsString) {
        reject('no stats');
      }
      const newStats = JSON.parse(statsString);
      if(newStats.hasOwnProperty('total')) {
        resolve(newStats.total.senderBitrateEstimation);
      }
      resolve(false);
    });
  });
};

exports.schemeHelpers = schemeHelpers;
