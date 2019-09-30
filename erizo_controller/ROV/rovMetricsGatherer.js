class RovMetricsGatherer {
  constructor(rovClient, promClient, statsPrefix, logger) {
    this.rovClient = rovClient;
    this.prefix = statsPrefix;
    this.prometheusMetrics = {
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
      connectionQualityHigh: new promClient.Gauge({ name: this.getNameWithPrefix('connection_quality_high'), help: 'connections with high quality' }),
      connectionQualityMedium: new promClient.Gauge({ name: this.getNameWithPrefix('connection_quality_medium'), help: 'connections with medium quality' }),
      connectionQualityLow: new promClient.Gauge({ name: this.getNameWithPrefix('connection_quality_low'), help: 'connections with low quality' }),
      totalPublishersInErizoJS: new promClient.Gauge({ name: this.getNameWithPrefix('total_publishers_erizojs'), help: 'total active publishers in erizo js' }),
      totalSubscribersInErizoJS: new promClient.Gauge({ name: this.getNameWithPrefix('total_subscribers_erizojs'), help: 'total active subscribers in erizo js' }),
    };
    this.log = logger;
  }

  getNameWithPrefix(name) {
    return `${this.prefix}${name}`;
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
    const command = 'var totalValues = {publishers: 0, subscribers: 0}; context.rooms.forEach((room)' +
      '=> {const pubsubList = room.controller.getSubscribers(); const publishersInList = Object.keys(pubsubList); totalValues.publishers += publishersInList.length;' +
      'publishersInList.forEach((pubId) => {totalValues.subscribers += pubsubList[pubId].length; });}); console.log(JSON.stringify(totalValues));';
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

  getErizoJSMetrics() {
    this.log.debug('Getting total connections failed');
    return this.rovClient.runInComponentList('console.log(JSON.stringify(context.getAndResetMetrics()))', this.rovClient.components.erizoJS)
      .then((results) => {
        let totalConnectionsFailed = 0;
        let taskDurationDistribution = Array(5).fill(0);
        let connectionLevels = Array(10).fill(0);
        let publishers = 0;
        let subscribers = 0;
        results.forEach((result) => {
          const parsedResult = JSON.parse(result);
          totalConnectionsFailed += parsedResult.connectionsFailed;
          taskDurationDistribution =
            taskDurationDistribution.map((a, i) => a + parsedResult.durationDistribution[i]);
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

        this.prometheusMetrics.connectionQualityHigh.set(connectionLevels[2]);
        this.prometheusMetrics.connectionQualityMedium.set(connectionLevels[1]);
        this.prometheusMetrics.connectionQualityLow.set(connectionLevels[0]);

        this.prometheusMetrics.totalPublishersInErizoJS.set(publishers);
        this.prometheusMetrics.totalSubscribersInErizoJS.set(subscribers);
        return Promise.resolve();
      });
  }

  gatherMetrics() {
    return this.rovClient.updateComponentsList()
      .then(() => this.getTotalRooms())
      .then(() => this.getTotalClients())
      .then(() => this.getTotalPublishersAndSubscribers())
      .then(() => this.getActiveProcesses())
      .then(() => this.getErizoJSMetrics());
  }
}

exports.RovMetricsGatherer = RovMetricsGatherer;
