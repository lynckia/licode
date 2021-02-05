const readline = require('readline');
const fs = require('fs');

// eslint-disable-next-line global-require, import/no-extraneous-dependencies
const AWS = require('aws-sdk');

// eslint-disable-next-line import/no-unresolved
const config = require('../../licode_config');
const { PrometheusExporter } = require('./../common/PerformanceStats');

class RovMetricsGatherer {
  constructor(rovClient, promClient, statsPrefix, logger) {
    this.rovClient = rovClient;
    this.prefix = statsPrefix;
    this.prometheusMetrics = {
      release: new promClient.Gauge({ name: this.getNameWithPrefix('release_info'), help: 'commit short hash', labelNames: ['commit', 'date', 'ip'] }),
      activeRooms: new promClient.Gauge({ name: this.getNameWithPrefix('active_rooms'), help: 'active rooms in all erizoControllers' }),
      activeClients: new promClient.Gauge({ name: this.getNameWithPrefix('active_clients'), help: 'active clients in all erizoControllers' }),
      totalPublishers: new promClient.Gauge({ name: this.getNameWithPrefix('total_publishers'), help: 'total active publishers' }),
      totalSubscribers: new promClient.Gauge({ name: this.getNameWithPrefix('total_subscribers'), help: 'total active subscribers' }),
      activeErizoJsProcesses: new promClient.Gauge({ name: this.getNameWithPrefix('active_erizojs_processes'), help: 'active processes' }),
      totalConnectionsFailed: new promClient.Gauge({ name: this.getNameWithPrefix('total_connections_failed'), help: 'connections failed' }),
      taskDuration0To10ms: new promClient.Gauge({ name: this.getNameWithPrefix('task_duration_0_to_10_ms'), help: 'tasks lasted less than 10 ms' }),
      taskDuration10To50ms: new promClient.Gauge({ name: this.getNameWithPrefix('task_duration_10_to_50_ms'), help: 'tasks lasted between 10 and 50 ms' }),
      taskDuration50To100ms: new promClient.Gauge({ name: this.getNameWithPrefix('task_duration_50_to_100_ms'), help: 'tasks lasted between 50 and 100 ms' }),
      taskDuration100To1000ms: new promClient.Gauge({ name: this.getNameWithPrefix('task_duration_100_to_1000_ms'), help: 'tasks lasted between 100 ms and 1 s' }),
      taskDurationMoreThan1000ms: new promClient.Gauge({ name: this.getNameWithPrefix('task_duration_1000_ms'), help: 'tasks lasted more than 1 s' }),
      taskDelay0To10ms: new promClient.Gauge({ name: this.getNameWithPrefix('task_delay_0_to_10_ms'), help: 'tasks delayed less than 10 ms' }),
      taskDelay10To50ms: new promClient.Gauge({ name: this.getNameWithPrefix('task_delay_10_to_50_ms'), help: 'tasks delayed between 10 and 50 ms' }),
      taskDelay50To100ms: new promClient.Gauge({ name: this.getNameWithPrefix('task_delay_50_to_100_ms'), help: 'tasks delayed between 50 and 100 ms' }),
      taskDelay100To1000ms: new promClient.Gauge({ name: this.getNameWithPrefix('task_delay_100_to_1000_ms'), help: 'tasks delayed between 100 ms and 1 s' }),
      taskDelayMoreThan1000ms: new promClient.Gauge({ name: this.getNameWithPrefix('task_delay_1000_ms'), help: 'tasks delayed more than 1 s' }),
      connectionPromiseDuration0To10ms: new promClient.Gauge({ name: this.getNameWithPrefix('connection_promise_duration_0_to_10_ms'), help: 'Connection Promises lasted less than 10 ms' }),
      connectionPromiseDuration10To50ms: new promClient.Gauge({ name: this.getNameWithPrefix('connection_promise_duration_10_to_50_ms'), help: 'Connection Promises lasted between 10 and 50 ms' }),
      connectionPromiseDuration50To100ms: new promClient.Gauge({ name: this.getNameWithPrefix('connection_promise_duration_50_to_100_ms'), help: 'Connection Promises lasted between 50 and 100 ms' }),
      connectionPromiseDuration100To1000ms: new promClient.Gauge({ name: this.getNameWithPrefix('connection_promise_duration_100_to_1000_ms'), help: 'Connection Promises lasted between 100 ms and 1 s' }),
      connectionPromiseDurationMoreThan1000ms: new promClient.Gauge({ name: this.getNameWithPrefix('connection_promise_duration_1000_ms'), help: 'Connection Promises lasted more than 1 s' }),
      connectionPromiseDelay0To10ms: new promClient.Gauge({ name: this.getNameWithPrefix('connection_promise_delay_0_to_10_ms'), help: 'Connection Promises notification were delayed less than 10 ms' }),
      connectionPromiseDelay10To50ms: new promClient.Gauge({ name: this.getNameWithPrefix('connection_promise_delay_10_to_50_ms'), help: 'Connection Promises notification were delayed between 10 and 50 ms' }),
      connectionPromiseDelay50To100ms: new promClient.Gauge({ name: this.getNameWithPrefix('connection_promise_delay_50_to_100_ms'), help: 'Connection Promises notification were delayed between 50 and 100 ms' }),
      connectionPromiseDelay100To1000ms: new promClient.Gauge({ name: this.getNameWithPrefix('connection_promise_delay_100_to_1000_ms'), help: 'Connection Promises notification were delayed between 100 ms and 1 s' }),
      connectionPromiseDelayMoreThan1000ms: new promClient.Gauge({ name: this.getNameWithPrefix('connection_promise_delay_1000_ms'), help: 'Connection Promises notification were delayed more than 1 s' }),
      streamPromiseDuration0To10ms: new promClient.Gauge({ name: this.getNameWithPrefix('stream_promise_duration_0_to_10_ms'), help: 'Stream Promises lasted less than 10 ms' }),
      streamPromiseDuration10To50ms: new promClient.Gauge({ name: this.getNameWithPrefix('stream_promise_duration_10_to_50_ms'), help: 'Stream Promises lasted between 10 and 50 ms' }),
      streamPromiseDuration50To100ms: new promClient.Gauge({ name: this.getNameWithPrefix('stream_promise_duration_50_to_100_ms'), help: 'Stream Promises lasted between 50 and 100 ms' }),
      streamPromiseDuration100To1000ms: new promClient.Gauge({ name: this.getNameWithPrefix('stream_promise_duration_100_to_1000_ms'), help: 'Stream Promises lasted between 100 ms and 1 s' }),
      streamPromiseDurationMoreThan1000ms: new promClient.Gauge({ name: this.getNameWithPrefix('stream_promise_duration_1000_ms'), help: 'Stream Promises lasted more than 1 s' }),
      streamPromiseDelay0To10ms: new promClient.Gauge({ name: this.getNameWithPrefix('stream_promise_delay_0_to_10_ms'), help: 'Stream Promises notification were delayed less than 10 ms' }),
      streamPromiseDelay10To50ms: new promClient.Gauge({ name: this.getNameWithPrefix('stream_promise_delay_10_to_50_ms'), help: 'Stream Promises notification were delayed between 10 and 50 ms' }),
      streamPromiseDelay50To100ms: new promClient.Gauge({ name: this.getNameWithPrefix('stream_promise_delay_50_to_100_ms'), help: 'Stream Promises notification were delayed between 50 and 100 ms' }),
      streamPromiseDelay100To1000ms: new promClient.Gauge({ name: this.getNameWithPrefix('stream_promise_delay_100_to_1000_ms'), help: 'Stream Promises notification were delayed between 100 ms and 1 s' }),
      streamPromiseDelayMoreThan1000ms: new promClient.Gauge({ name: this.getNameWithPrefix('stream_promise_delay_1000_ms'), help: 'Stream Promises notification were delayed more than 1 s' }),
      connectionQualityHigh: new promClient.Gauge({ name: this.getNameWithPrefix('connection_quality_high'), help: 'connections with high quality' }),
      connectionQualityMedium: new promClient.Gauge({ name: this.getNameWithPrefix('connection_quality_medium'), help: 'connections with medium quality' }),
      connectionQualityLow: new promClient.Gauge({ name: this.getNameWithPrefix('connection_quality_low'), help: 'connections with low quality' }),
      totalPublishersInErizoJS: new promClient.Gauge({ name: this.getNameWithPrefix('total_publishers_erizojs'), help: 'total active publishers in erizo js' }),
      totalSubscribersInErizoJS: new promClient.Gauge({ name: this.getNameWithPrefix('total_subscribers_erizojs'), help: 'total active subscribers in erizo js' }),
      maxMinEventLoopLag: new promClient.Gauge({ name: this.getNameWithPrefix('event_loop_lag_max_min'), help: 'Min event loop lag' }),
      maxMaxEventLoopLag: new promClient.Gauge({ name: this.getNameWithPrefix('event_loop_lag_max_max'), help: 'Max event loop lag' }),
      maxMeanEventLoopLag: new promClient.Gauge({ name: this.getNameWithPrefix('event_loop_lag_max_mean'), help: 'Mean event loop lag' }),
      maxStdDevEventLoopLag: new promClient.Gauge({ name: this.getNameWithPrefix('event_loop_lag_max_stddev'), help: 'Standard Deviation event loop lag' }),
      maxMedianEventLoopLag: new promClient.Gauge({ name: this.getNameWithPrefix('event_loop_lag_max_median'), help: 'Median event loop lag' }),
      maxP95EventLoopLag: new promClient.Gauge({ name: this.getNameWithPrefix('event_loop_lag_max_p95'), help: 'Percentile 95 event loop lag' }),
      maxP99EventLoopLag: new promClient.Gauge({ name: this.getNameWithPrefix('event_loop_lag_max_p99'), help: 'Percentile 99 event loop lag' }),
    };
    this.log = logger;
    this.releaseInfoRead = false;
    if (config && config.erizoAgent) {
      this.publicIP = config.erizoAgent.publicIP;
    }
    try {
      this.prometheusStatsExporter = new PrometheusExporter(promClient, name => `${this.prefix}${name}`);
    } catch (e) {
      this.log.error('Error creating performances metrics exporter, message:', e.message);
    }
  }

  getNameWithPrefix(name) {
    return `${this.prefix}${name}`;
  }

  getIP() {
    // Here we assume that ROV runs in the same instance than Erizo Controller
    if (config && config.cloudProvider && config.cloudProvider.name === 'amazon') {
      return new Promise((resolve) => {
        new AWS.MetadataService({
          httpOptions: {
            timeout: 5000,
          },
        }).request('/latest/meta-data/public-ipv4', (err, data) => {
          if (err) {
            this.log.error('Error: ', err);
          } else {
            this.publicIP = data;
          }
          resolve();
        });
      });
    }
    return Promise.resolve();
  }

  getReleaseInfo() {
    this.log.debug('Getting release info');
    if (!this.releaseInfoRead) {
      this.releaseInfoRead = true;
      try {
        return new Promise((resolve) => {
          const input = fs.createReadStream('../../RELEASE');

          input.on('error', (e) => {
            this.log.error('Error reading release file', e);
            resolve();
          });
          const fileReader = readline.createInterface({
            input,
            output: process.stdout,
            console: false,
          });
          let lineNumber = 0;
          let releaseCommit = '';
          let releaseDate = '';
          fileReader.on('line', (line) => {
            this.log.info(line);
            if (lineNumber === 0) {
              releaseCommit = line;
            } else {
              releaseDate = line;
            }
            lineNumber += 1;
          });

          fileReader.once('close', () => {
            this.prometheusMetrics.release.labels(releaseCommit, releaseDate, this.publicIP).set(1);
            resolve();
          });
        });
      } catch (e) {
        this.log.error('Error reading release file', e);
      }
    }
    return Promise.resolve();
  }

  getTotalRooms() {
    this.log.debug('Getting total rooms');
    return this.rovClient.runInComponentList('console.log(context.rooms.size)', this.rovClient.components.erizoControllers)
      .then((results) => {
        let totalRooms = 0;
        results.forEach((roomsSize) => {
          totalRooms += parseInt(roomsSize, 10);
        });
        this.log.debug(`Total rooms result: ${totalRooms}`);
        this.prometheusMetrics.activeRooms.set(totalRooms);
        return Promise.resolve();
      });
  }

  getTotalClients() {
    this.log.debug('Getting total clients');
    return this.rovClient.runInComponentList('var totalClients = 0; context.rooms.forEach((room) => {totalClients += room.clients.size}); console.log(totalClients);',
      this.rovClient.components.erizoControllers)
      .then((results) => {
        let totalClients = 0;
        results.forEach((clientsInRoom) => {
          totalClients += isNaN(clientsInRoom) ? 0 : parseInt(clientsInRoom, 10);
        });
        this.prometheusMetrics.activeClients.set(totalClients);
        this.log.debug(`Total clients result: ${totalClients}`);
        return Promise.resolve();
      });
  }

  getTotalPublishersAndSubscribers() {
    this.log.debug('Getting total publishers and subscribers');
    const requestPromises = [];
    const command = 'var totalValues = {publishers: 0, subscribers: 0};' +
      'context.rooms.forEach((room) => {room.streamManager.forEachPublishedStream((pub) => {totalValues.publishers += 1;' +
      'totalValues.subscribers += pub.avSubscribers.size; });}); console.log(JSON.stringify(totalValues));';
    this.rovClient.components.erizoControllers.forEach((controller) => {
      requestPromises.push(controller.runAndGetPromise(command));
    });
    let totalPublishers = 0;
    let totalSubscribers = 0;
    return Promise.all(requestPromises).then((results) => {
      results.forEach((result) => {
        const parsedResult = JSON.parse(result);
        totalPublishers += parsedResult.publishers;
        totalSubscribers += parsedResult.subscribers;
      });
      this.prometheusMetrics.totalPublishers.set(totalPublishers);
      this.prometheusMetrics.totalSubscribers.set(totalSubscribers);
      this.log.debug(`Total publishers and subscribers result: ${totalPublishers}, ${totalSubscribers}`);
      return Promise.resolve();
    });
  }

  getActiveProcesses() {
    this.log.debug('Getting active processes');
    let totalActiveProcesses = 0;
    this.rovClient.components.erizoJS.forEach((process) => {
      totalActiveProcesses += process.idle ? 0 : 1;
    });
    this.log.debug(`Active processes result: ${totalActiveProcesses}`);
    this.prometheusMetrics.activeErizoJsProcesses.set(totalActiveProcesses);
    return Promise.resolve();
  }

  getNodeJSEventLoopLag() {
    this.log.debug('Getting NodeJSEventLoopLag');
    return this.rovClient.runInComponentList('console.log(JSON.stringify(context.computeEventLoopLags()))', this.rovClient.components.erizoJS)
      .then((results) => {
        let maxMin = 0;
        let maxMax = 0;
        let maxMean = 0;
        let maxStdDev = 0;
        let maxMedian = 0;
        let maxP95 = 0;
        let maxP99 = 0;
        results.forEach((data) => {
          const result = JSON.parse(data);
          maxMin = Math.max(result.min, maxMin);
          maxMax = Math.max(result.max, maxMax);
          maxMean = Math.max(result.mean, maxMean);
          maxStdDev = Math.max(result.stddev, maxStdDev);
          maxMedian = Math.max(result.median, maxMedian);
          maxP95 = Math.max(result.p95, maxP95);
          maxP99 = Math.max(result.p99, maxP99);
        });
        this.log.debug(`Max Mean Event Loop Lags: ${maxMean}`);
        this.prometheusMetrics.maxMinEventLoopLag.set(maxMin);
        this.prometheusMetrics.maxMaxEventLoopLag.set(maxMax);
        this.prometheusMetrics.maxMeanEventLoopLag.set(maxMean);
        this.prometheusMetrics.maxStdDevEventLoopLag.set(maxStdDev);
        this.prometheusMetrics.maxMedianEventLoopLag.set(maxMedian);
        this.prometheusMetrics.maxP95EventLoopLag.set(maxP95);
        this.prometheusMetrics.maxP99EventLoopLag.set(maxP99);
        return Promise.resolve();
      });
  }

  getErizoJSPerformanceMetrics() {
    this.log.debug('Getting performance metrics');
    return this.rovClient.runInComponentList('console.log(JSON.stringify(context.computePerformanceStats()))', this.rovClient.components.erizoJS)
      .then((results) => {
        this.prometheusStatsExporter.exportToPrometheus(results);
      });
  }

  getErizoJSMetrics() {
    this.log.debug('Getting total connections failed');
    return this.rovClient.runInComponentList('console.log(JSON.stringify(context.getAndResetMetrics()))', this.rovClient.components.erizoJS)
      .then((results) => {
        let totalConnectionsFailed = 0;
        let taskDurationDistribution = Array(5).fill(0);
        let taskDelayDistribution = Array(5).fill(0);
        let connectionPromiseDurationDistribution = Array(5).fill(0);
        let connectionPromiseDelayDistribution = Array(5).fill(0);
        let streamPromiseDurationDistribution = Array(5).fill(0);
        let streamPromiseDelayDistribution = Array(5).fill(0);
        let connectionLevels = Array(10).fill(0);
        let publishers = 0;
        let subscribers = 0;
        results.forEach((result) => {
          const parsedResult = JSON.parse(result);
          totalConnectionsFailed += parsedResult.connectionsFailed;
          taskDurationDistribution =
            taskDurationDistribution.map((a, i) => a + parsedResult.durationDistribution[i]);
          taskDelayDistribution =
            taskDelayDistribution.map((a, i) => a + parsedResult.delayDistribution[i]);
          connectionPromiseDurationDistribution =
            connectionPromiseDurationDistribution.map((a, i) =>
              a + parsedResult.connectionDurationDistribution[i]);
          connectionPromiseDelayDistribution =
            connectionPromiseDelayDistribution.map((a, i) =>
              a + parsedResult.connectionDelayDistribution[i]);
          streamPromiseDurationDistribution =
            streamPromiseDurationDistribution.map((a, i) =>
              a + parsedResult.streamDurationDistribution[i]);
          streamPromiseDelayDistribution =
            streamPromiseDelayDistribution.map((a, i) =>
              a + parsedResult.streamDelayDistribution[i]);
          connectionLevels = connectionLevels.map((a, i) => a + parsedResult.connectionLevels[i]);
          publishers += parsedResult.publishers;
          subscribers += parsedResult.subscribers;
        });
        this.log.debug(`Total connections failed: ${totalConnectionsFailed}`);
        this.prometheusMetrics.totalConnectionsFailed.set(totalConnectionsFailed);
        this.prometheusMetrics.taskDuration0To10ms.set(taskDurationDistribution[0]);
        this.prometheusMetrics.taskDuration10To50ms.set(taskDurationDistribution[1]);
        this.prometheusMetrics.taskDuration50To100ms.set(taskDurationDistribution[2]);
        this.prometheusMetrics.taskDuration100To1000ms.set(taskDurationDistribution[3]);
        this.prometheusMetrics.taskDurationMoreThan1000ms.set(taskDurationDistribution[4]);

        this.prometheusMetrics.taskDelay0To10ms.set(taskDelayDistribution[0]);
        this.prometheusMetrics.taskDelay10To50ms.set(taskDelayDistribution[1]);
        this.prometheusMetrics.taskDelay50To100ms.set(taskDelayDistribution[2]);
        this.prometheusMetrics.taskDelay100To1000ms.set(taskDelayDistribution[3]);
        this.prometheusMetrics.taskDelayMoreThan1000ms.set(taskDelayDistribution[4]);

        this.prometheusMetrics.connectionPromiseDuration0To10ms.set(
          connectionPromiseDurationDistribution[0]);
        this.prometheusMetrics.connectionPromiseDuration10To50ms.set(
          connectionPromiseDurationDistribution[1]);
        this.prometheusMetrics.connectionPromiseDuration50To100ms.set(
          connectionPromiseDurationDistribution[2]);
        this.prometheusMetrics.connectionPromiseDuration100To1000ms.set(
          connectionPromiseDurationDistribution[3]);
        this.prometheusMetrics.connectionPromiseDurationMoreThan1000ms.set(
          connectionPromiseDurationDistribution[4]);

        this.prometheusMetrics.connectionPromiseDelay0To10ms.set(
          connectionPromiseDelayDistribution[0]);
        this.prometheusMetrics.connectionPromiseDelay10To50ms.set(
          connectionPromiseDelayDistribution[1]);
        this.prometheusMetrics.connectionPromiseDelay50To100ms.set(
          connectionPromiseDelayDistribution[2]);
        this.prometheusMetrics.connectionPromiseDelay100To1000ms.set(
          connectionPromiseDelayDistribution[3]);
        this.prometheusMetrics.connectionPromiseDelayMoreThan1000ms.set(
          connectionPromiseDelayDistribution[4]);

        this.prometheusMetrics.streamPromiseDuration0To10ms.set(
          streamPromiseDurationDistribution[0]);
        this.prometheusMetrics.streamPromiseDuration10To50ms.set(
          streamPromiseDurationDistribution[1]);
        this.prometheusMetrics.streamPromiseDuration50To100ms.set(
          streamPromiseDurationDistribution[2]);
        this.prometheusMetrics.streamPromiseDuration100To1000ms.set(
          streamPromiseDurationDistribution[3]);
        this.prometheusMetrics.streamPromiseDurationMoreThan1000ms.set(
          streamPromiseDurationDistribution[4]);

        this.prometheusMetrics.streamPromiseDelay0To10ms.set(
          streamPromiseDelayDistribution[0]);
        this.prometheusMetrics.streamPromiseDelay10To50ms.set(
          streamPromiseDelayDistribution[1]);
        this.prometheusMetrics.streamPromiseDelay50To100ms.set(
          streamPromiseDelayDistribution[2]);
        this.prometheusMetrics.streamPromiseDelay100To1000ms.set(
          streamPromiseDelayDistribution[3]);
        this.prometheusMetrics.streamPromiseDelayMoreThan1000ms.set(
          streamPromiseDelayDistribution[4]);

        this.prometheusMetrics.connectionQualityHigh.set(connectionLevels[2]);
        this.prometheusMetrics.connectionQualityMedium.set(connectionLevels[1]);
        this.prometheusMetrics.connectionQualityLow.set(connectionLevels[0]);

        this.prometheusMetrics.totalPublishersInErizoJS.set(publishers);
        this.prometheusMetrics.totalSubscribersInErizoJS.set(subscribers);
        return Promise.resolve();
      });
  }

  gatherMetrics() {
    return this.getIP()
      .then(() => this.getReleaseInfo())
      .then(() => this.rovClient.updateComponentsList())
      .then(() => this.getTotalRooms())
      .then(() => this.getTotalClients())
      .then(() => this.getTotalPublishersAndSubscribers())
      .then(() => this.getActiveProcesses())
      .then(() => this.getNodeJSEventLoopLag())
      .then(() => this.getErizoJSPerformanceMetrics())
      .then(() => this.getErizoJSMetrics());
  }
}

exports.RovMetricsGatherer = RovMetricsGatherer;
