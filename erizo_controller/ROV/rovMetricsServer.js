/* global require */

/* eslint-disable  no-console */

// eslint-disable-next-line import/no-extraneous-dependencies
const Getopt = require('node-getopt');
// eslint-disable-next-line import/no-extraneous-dependencies
const express = require('express');
const RovClient = require('./rovClient').RovClient;
const RovMetricsGatherer = require('./rovMetricsGatherer').RovMetricsGatherer;
// eslint-disable-next-line import/no-extraneous-dependencies
const promClient = require('prom-client');

// eslint-disable-next-line import/no-unresolved
const config = require('../../licode_config');
// Configuration default values
global.config = config || {};

// Parse command line arguments
const getopt = new Getopt([
  ['r', 'rabbit-host=ARG', 'RabbitMQ Host'],
  ['g', 'rabbit-port=ARG', 'RabbitMQ Port'],
  ['b', 'rabbit-heartbeat=ARG', 'RabbitMQ AMQP Heartbeat Timeout'],
  ['h', 'help', 'display this help'],
]);

const opt = getopt.parse(process.argv.slice(2));


Object.keys(opt.options).forEach((prop) => {
  const value = opt.options[prop];
  switch (prop) {
    case 'help':
      getopt.showHelp();
      process.exit(0);
      break;
    case 'rabbit-host':
      global.config.rabbit = global.config.rabbit || {};
      global.config.rabbit.host = value;
      break;
    case 'rabbit-port':
      global.config.rabbit = global.config.rabbit || {};
      global.config.rabbit.port = value;
      break;
    case 'rabbit-heartbeat':
      global.config.rabbit = global.config.rabbit || {};
      global.config.rabbit.heartbeat = value;
      break;
    default:
      global.config.erizoAgent[prop] = value;
      break;
  }
});

// Load submodules with updated config
const logger = require('./../common/logger').logger;
const amqper = require('./../common/amqper');

const log = logger.getLogger('RovMetricsServer');

const server = express();
const register = promClient.register;

log.info('Starting: Connecting to rabbitmq');
amqper.connect(() => {
  log.info('connected to rabbitmq');
  const rovClient = new RovClient(amqper);
  const rovMetricsGatherer =
    new RovMetricsGatherer(rovClient, promClient, config.rov.statsPrefix, log);
  promClient.collectDefaultMetrics(
    { prefix: config.rov.statsPrefix, timeout: config.rov.statsPeriod });

  setInterval(() => {
    rovMetricsGatherer.gatherMetrics().then(() => {
      log.debug('Gathered licode metrics');
    })
      .catch((error) => {
        log.error('Error gathering metrics', error);
      });
  }, config.rov.statsPeriod);
  server.get('/metrics', (req, res) => {
    res.set('Content-Type', register.contentType);
    res.end(register.metrics());
  });
  log.info(`Started rovMetricsServer. Listening in port ${config.rov.serverPort}`);
  server.listen(config.rov.serverPort);
});
