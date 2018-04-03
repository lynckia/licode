'use strict';
const schemeHelpers = {};

schemeHelpers.getBandwidthStat = (mediaStream) => {
  return new Promise((resolve, reject) => {
    mediaStream.getStats( (statsString) => {
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
