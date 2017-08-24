const nativeConnection = require('./simpleNativeConnection');
const logger = require('./logger').logger;

exports.buildSpine = (configuration) => {
  const that = {};
  that.nativeConnections = [];

  const log = logger.getLogger('Spine');
  const streamConfig = Object.assign({}, configuration);

  let streamPublishConfig;
  let streamSubscribeConfig;

  if (streamConfig.publishConfig) {
    streamPublishConfig = {
      publishConfig: streamConfig.publishConfig,
      serverUrl: streamConfig.basicExampleUrl,
    };
    if (streamConfig.publishersAreSubscribers) {
      streamPublishConfig.subscribeConfig = streamConfig.subscribeConfig;
    }
    log.info('publish Configuration: ', streamSubscribeConfig);
  }

  if (streamConfig.subscribeConfig) {
    streamSubscribeConfig = {
      subscribeConfig: streamConfig.subscribeConfig,
      serverUrl: streamConfig.basicExampleUrl,
    };
    log.info('Subscribe Configuration: ', streamSubscribeConfig);
  }

  const startSingleNativeClient = (clientConfig) => {
    const nativeClient = nativeConnection.ErizoSimpleNativeConnection(clientConfig,
    (msg) => {
      log.debug('Callback from nativeClient', msg);
    },
    (msg) => {
      log.debug('Error from nativeClient', msg);
    });
    that.nativeConnections.push(nativeClient);
  };

  const startStreams = (stConf, num, time) => {
    let started = 0;
    const interval = setInterval(() => {
      started += 1;
      if (started >= num) {
        log.info('All streams have been started');
        clearInterval(interval);
      }
      log.info('Will start stream with config', stConf);
      startSingleNativeClient(stConf);
    }, time);
  };

  that.getAllStreamStats = () => {
    const promises = [];
    that.nativeConnections.forEach((connection) => {
      promises.push(connection.getStats());
    });
    return Promise.all(promises);
  };

  that.getAllStreamStatuses = () => {
    const statuses = [];
    that.nativeConnections.forEach((connection) => {
      statuses.push(connection.getStatus());
    });
    return statuses;
  };

  that.run = () => {
    log.info('Starting ', streamConfig.numSubscribers, 'subscriber streams',
    'and', streamConfig.numPublishers, 'publisherStreams');
    if (streamSubscribeConfig && streamConfig.numSubscribers) {
      startStreams(streamSubscribeConfig,
        streamConfig.numSubscribers,
        streamConfig.connectionCreationInterval);
    }
    if (streamPublishConfig && streamConfig.numPublishers) {
      startStreams(streamPublishConfig,
        streamConfig.numPublishers,
        streamConfig.connectionCreationInterval);
    }
  };
  return that;
};
