/* eslint-disable no-param-reassign */

// eslint-disable-next-line import/no-extraneous-dependencies
const amqp = require('amqp');
const logger = require('./logger').logger;

// Logger
const log = logger.getLogger('AMQPER');

// Configuration default values
global.config.rabbit = global.config.rabbit || {};
global.config.rabbit.host = global.config.rabbit.host || 'localhost';
global.config.rabbit.port = global.config.rabbit.port || 5672;

const TIMEOUT = 5000;

// This timeout shouldn't be too low because it won't listen to onReady responses from ErizoJS
const REMOVAL_TIMEOUT = 300000;

const map = {}; // {corrID: {fn: callback, to: timeout}}
let connection;
let rpcExc;
let broadcastExc;
let clientQueue;
let corrID = 0;

const addr = {};
let rpcPublic = {};

if (global.config.rabbit.url !== undefined) {
  addr.url = global.config.rabbit.url;
} else {
  addr.host = global.config.rabbit.host;
  addr.port = global.config.rabbit.port;
}

if (global.config.rabbit.heartbeat !== undefined) {
  addr.heartbeat = global.config.rabbit.heartbeat;
}

exports.setPublicRPC = (methods) => {
  rpcPublic = methods;
};

exports.connect = (callback) => {
  // Create the amqp connection to rabbitMQ server
  connection = amqp.createConnection(addr);
  connection.on('ready', () => {
    // Create a direct exchange
    rpcExc = connection.exchange('rpcExchange', { type: 'direct' }, (exchange) => {
      try {
        log.info(`message: rpcExchange open, exchangeName: ${exchange.name}`);

        // Create the queue for receiving messages
        clientQueue = connection.queue('', (q) => {
          log.info(`message: clientqueue open, queuename: ${q.name}`);

          clientQueue.bind('rpcExchange', clientQueue.name, callback);

          clientQueue.subscribe((message) => {
            try {
              log.debug(`message: message received, queueName: ${clientQueue.name}`,
                logger.objectToLog(message));

              if (map[message.corrID] !== undefined) {
                log.debug(`message: Callback, queueName: ${clientQueue.name}, messageType: ${message.type}`,
                  logger.objectToLog(message.data));

                clearTimeout(map[message.corrID].to);
                if (message.type === 'onReady') {
                  map[message.corrID].fn[message.type].call({});
                } else {
                  map[message.corrID].fn[message.type].call({}, message.data);
                }
                setTimeout(() => {
                  if (map[message.corrID] !== undefined) {
                    delete map[message.corrID];
                  }
                }, REMOVAL_TIMEOUT);
              }
            } catch (err) {
              log.error('message: error processing message, ' +
                `queueName: ${clientQueue.name}, error: ${err.message}`);
            }
          });
        });
      } catch (err) {
        log.error('message: exchange error, ' +
          `exchangeName: ${exchange.name}, error: ${err.message}`);
      }
    });

    // Create a fanout exchange
    broadcastExc = connection.exchange('broadcastExchange',
      { type: 'topic', autoDelete: false },
      (exchange) => {
        log.info(`message: exchange open, exchangeName: ${exchange.name}`);
      });
  });

  connection.on('error', (e) => {
    log.error('message: AMQP connection error killing process, ',
      logger.objectToLog(e));
    process.exit(1);
  });
};

exports.bind = (id, callback) => {
  // Create the queue for receive messages
  const q = connection.queue(id, () => {
    try {
      log.info(`message: queue open, queueName: ${q.name}`);

      q.bind('rpcExchange', id, callback);
      q.subscribe((message) => {
        try {
          log.debug(`message: message received, queueName: ${q.name}, `,
            logger.objectToLog(message));
          message.args = message.args || [];
          message.args.push((type, result) => {
            rpcExc.publish(message.replyTo,
              { data: result, corrID: message.corrID, type });
          });
          rpcPublic[message.method](...message.args);
        } catch (error) {
          log.error('message: error processing call, ' +
            `queueName: ${q.name}, error: ${error.message}`);
        }
      });
    } catch (err) {
      log.error('message: queue error, ' +
        `queueName: ${q.name}, error: ${err.message}`);
    }
  });
};

// Subscribe to 'topic'
exports.bindBroadcast = (id, callback) => {
  // Create the queue for receive messages
  const q = connection.queue('', () => {
    try {
      log.info(`message: broadcast queue open, queueName: ${q.name}`);

      q.bind('broadcastExchange', id);
      q.subscribe((body) => {
        let answer;
        if (body.replyTo) {
          answer = (result) => {
            rpcExc.publish(body.replyTo, { data: result,
              corrID: body.corrID,
              type: 'callback' });
          };
        }
        if (body.message.method && rpcPublic[body.message.method]) {
          body.message.args.push(answer);
          try {
            rpcPublic[body.message.method](...body.message.args);
          } catch (e) {
            log.warn('message: error processing call, error:', e.message);
          }
        } else {
          try {
            callback(body.message, answer);
          } catch (e) {
            log.warn('message: error processing callback, error:', e.message);
          }
        }
      });
    } catch (err) {
      log.error('message: exchange error, ' +
        `queueName: ${q.name}, error: ${err.message}`);
    }
  });
};

const callbackError = (errorCorrID) => {
  Object.keys(map[errorCorrID].fn).forEach((fnKey) => {
    map[errorCorrID].fn[fnKey]('timeout');
  });
  delete map[errorCorrID];
};

/*
 * Publish broadcast messages to 'topic'
 * If message has the format {method: String, args: Array}. it will execute the RPC
 */
exports.broadcast = (topic, message, callback) => {
  const body = { message };

  if (callback) {
    corrID += 1;
    map[corrID] = {};
    map[corrID].fn = { callback };
    map[corrID].to = setTimeout(callbackError, TIMEOUT, corrID);

    body.corrID = corrID;
    body.replyTo = clientQueue.name;
  }
  broadcastExc.publish(topic, body);
};

/*
 * Calls remotely the 'method' function defined in rpcPublic of 'to'.
 */
exports.callRpc = (to, method, args, callbacks = { callback: () => {} }, timeout = TIMEOUT) => {
  corrID += 1;
  map[corrID] = {};
  map[corrID].fn = callbacks;
  map[corrID].to = setTimeout(callbackError, timeout, corrID);
  rpcExc.publish(to, { method, args, corrID, replyTo: clientQueue.name });
};
