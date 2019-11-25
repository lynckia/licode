/* global exports, require, setTimeout, clearTimeout */

// eslint-disable-next-line import/no-extraneous-dependencies
const amqp = require('amqp');
const rpcPublic = require('./rpcPublic');
// eslint-disable-next-line import/no-unresolved
const config = require('./../../../licode_config');
const logger = require('./../logger').logger;

// Logger
const log = logger.getLogger('RPC');

// Configuration default values
config.rabbit = config.rabbit || {};
config.rabbit.host = config.rabbit.host || 'localhost';
config.rabbit.port = config.rabbit.port || 5672;

const TIMEOUT = 3000;

let corrID = 0;
const map = {}; // {corrID: {fn: callback, to: timeout}}
let clientQueue;
let connection;
let exc;

// Create the amqp connection to rabbitMQ server
const addr = {};

if (config.rabbit.url !== undefined) {
  addr.url = config.rabbit.url;
} else {
  addr.host = config.rabbit.host;
  addr.port = config.rabbit.port;
}

if (config.rabbit.heartbeat !== undefined) {
  addr.heartbeat = config.rabbit.heartbeat;
}

exports.connect = (callback) => {
  connection = amqp.createConnection(addr);

  connection.on('ready', () => {
    log.info('message: AMQP connected');

    // Create a direct exchange
    exc = connection.exchange('rpcExchange', { type: 'direct' }, (exchange) => {
      log.info(`message: rpcExchange open, exchangeName: ${exchange.name}`);

      const next = () => {
        // Create the queue for receive messages
        const q = connection.queue('nuveQueue', (queue) => {
          log.info(`message: queue open, queueName: ${queue.name}`);

          q.bind('rpcExchange', 'nuve');
          q.subscribe((message) => {
            rpcPublic[message.method](message.args, (type, result) => {
              exc.publish(message.replyTo,
                { data: result, corrID: message.corrID, type });
            });
          });
          if (callback) {
            callback();
          }
        });
      };

      // Create the queue for send messages
      clientQueue = connection.queue('', (q) => {
        log.info(`message: clientQueue open, queueName: ${q.name}`);

        clientQueue.bind('rpcExchange', clientQueue.name);

        clientQueue.subscribe((message) => {
          if (map[message.corrID] !== undefined) {
            map[message.corrID].fn[message.type](message.data);
            clearTimeout(map[message.corrID].to);
            delete map[message.corrID];
          }
        });
        next();
      });
    });
  });

  connection.on('error', (e) => {
    log.error(`message: AMQP connection error killing process, errorMsg: ${logger.objectToLog(e)}`);
    process.exit(1);
  });
};

const callbackError = (corrIdForCallback) => {
  Object.keys(map[corrIdForCallback].fn).forEach((fnKey) => {
    map[corrIdForCallback].fn[fnKey]('timeout');
  });
  delete map[corrID];
};

/*
 * Calls remotely the 'method' function defined in rpcPublic of 'to'.
 */
exports.callRpc = (to, method, args, callbacks) => {
  corrID += 1;
  if (callbacks) {
    map[corrID] = {};
    map[corrID].fn = callbacks;
    map[corrID].to = setTimeout(callbackError, TIMEOUT, corrID);
  }
  const send = { method, args, corrID, replyTo: clientQueue.name };
  exc.publish(to, send);
};
