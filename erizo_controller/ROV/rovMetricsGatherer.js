class RovMetricsGatherer {
  constructor(rovClient, promClient, logger) {
    this.rovClient = rovClient;
    this.prometheusMetrics = {
      activeRooms: new promClient.Gauge({ name: 'active_rooms', help: 'active rooms in all erizoControllers' }),
      activeClients: new promClient.Gauge({ name: 'active_clients', help: 'active clients in all erizoControllers' }),
      totalPublishers: new promClient.Gauge({ name: 'total_publishers', help: 'total active publishers' }),
      totalSubscribers: new promClient.Gauge({ name: 'total_subscribers', help: 'total active subscribers' }),
      activeErizoJsProcesses: new promClient.Gauge({ name: 'active_erizojs_processes', help: 'active processes' }),
    };
    this.log = logger;
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

  gatherMetrics() {
    return this.rovClient.updateComponentsList()
      .then(() => this.getTotalRooms())
      .then(() => this.getTotalClients())
      .then(() => this.getTotalPublishersAndSubscribers())
      .then(() => this.getActiveProcesses());
  }
}

exports.RovMetricsGatherer = RovMetricsGatherer;
