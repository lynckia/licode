/* global exports */

exports.getMediaConfiguration = (mediaConfiguration = 'default') => {
  if (global.mediaConfig && global.mediaConfig.codecConfigurations) {
    if (global.mediaConfig.codecConfigurations[mediaConfiguration]) {
      return JSON.stringify(global.mediaConfig.codecConfigurations[mediaConfiguration]);
    } else if (global.mediaConfig.codecConfigurations.default) {
      return JSON.stringify(global.mediaConfig.codecConfigurations.default);
    }
    return JSON.stringify({});
  }
  return JSON.stringify({});
};

exports.getErizoStreamId = (clientId, streamId) => `${clientId}_${streamId}`;

exports.retryWithPromise = (fn, timeout, retries = 3) =>
  new Promise((resolve, reject) => {
    if (retries < 0) {
      reject('max-retries');
      return;
    }
    fn().then(resolve)
      .catch((error) => {
        if (error === 'retry') {
          setTimeout(() => {
            exports.retryWithPromise(fn, timeout, retries - 1).then(resolve, reject);
          }, timeout);
        } else {
          // For now we're resolving the promise instead of rejecting it since
          // we don't handle errors at the moment
          resolve();
        }
      });
  });

exports.serializeStreamPriorityStrategy = (strat) => {
  const resultArray = [];
  let failed = false;
  strat.strategy.forEach((entry) => {
    const entryArray = entry.split('_');
    const priority = entryArray[0];
    const priorityLayer = entryArray[1];
    const entryForPriority = strat.priorities[priority][priorityLayer];
    if (!entryForPriority) {
      failed = true;
      return;
    }
    resultArray.push([priority, strat.priorities[priority][priorityLayer].replace('SL', '')]);
  });
  if (failed) {
    return false;
  }
  return resultArray;
};
