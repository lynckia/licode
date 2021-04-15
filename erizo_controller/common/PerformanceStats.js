/* global require, exports */

const Stats = require('fast-stats').Stats;
const { performance } = require('perf_hooks');

const logger = require('./logger').logger;

const log = logger.getLogger('PerformanceStats');

let performanceStats;

// This file provides a way to measure performance in Licode components
// by using NodeJS native Performance class to set marks (timestamps) and
// calculate measures (durations in milliseconds) and process stats with
// those mesaures (min, max, arithmetic mean, arithmetic standard deviation).
// It also provides mechanisms to ROV to export the resulting stats to
// Prometheus.

// Stats that FastStats will compute for each measure
// We will end up publishing the tuples <measure, stat> to Prometheus:
// Examples: subscribe_total_min, subscribe_total_amean
const StatMetrics = ['min', 'max', 'amean', 'stddev'];

// All Measures we will send to prometheus. A measure is a duration.
const Measures = {
  SUBSCRIBE_TOTAL: 'subscribe_total',
  SUBSCRIBE_STREAM_CREATED: 'subscribe_stream_started',
  SUBSCRIBE_CANDIDATES_GATHERED: 'subscribe_candidates_gathered',
  SUBSCRIBE_CONNECTION_INIT: 'subscribe_connection_init',
  SUBSCRIBE_CONNECTION_STARTED: 'subscribe_connection_started',
  SUBSCRIBE_CONNECTION_READY: 'subscribe_connection_ready',

  UNSUBSCRIBE_REMOVE_STREAM: 'unsubscribe_remove_stream',
  UNSUBSCRIBE_CLOSE_STREAM: 'unsubscribe_close_stream',
  UNSUBSCRIBE_REMOVE_INTERNAL_STREAM: 'unsubscribe_remove_internal_stream',
  UNSUBSCRIBE_CLOSE_INTERNAL_STREAM: 'unsubscribe_close_internal_stream',
  UNSUBSCRIBE_RESPONSE_SENT: 'unsubscribe_response_sent',
  UNSUBSCRIBE_TOTAL: 'unsubscribe_total',

  CONNECTION_NEGOTIATION_LOCAL_OFFER_TOTAL: 'connection_negotiation_local_offer_total',
  CONNECTION_NEGOTIATION_OFFER_CREATE: 'connection_negotiation_offer_create',
  CONNECTION_NEGOTIATION_OFFER_SEND: 'connection_negotiation_offer_send',
  CONNECTION_NEGOTIATION_ANSWER_RECEIVE: 'connection_negotiation_answer_receive',
  CONNECTION_NEGOTIATION_ANSWER_SET: 'connection_negotiation_answer_set',

  CONNECTION_NEGOTIATION_REMOTE_OFFER_TOTAL: 'connection_negotiation_remote_offer_total',
  CONNECTION_NEGOTIATION_OFFER_SET: 'connection_negotiation_offer_set',
  CONNECTION_NEGOTIATION_ANSWER_CREATE: 'connection_negotiation_answer_create',
  CONNECTION_NEGOTIATION_ANSWER_CREATED: 'connection_negotiation_answer_created',
  CONNECTION_NEGOTIATION_ANSWER_SEND: 'connection_negotiation_answer_send',
};

// Timestamp marks that will take part of the Measures.
const Marks = {
  SUBSCRIBE_REQUEST_RECEIVED: 'subscribe_request_received',
  SUBSCRIBE_STREAM_CREATED: 'subscribe_stream_created',
  SUBSCRIBE_CONNECTION_INIT: 'subscribe_connection_init',
  SUBSCRIBE_CONNECTION_STARTED: 'subscribe_connection_started',
  SUBSCRIBE_CONNECTION_READY: 'subscribe_connection_ready',
  SUBSCRIBE_CANDIDATES_GATHERED: 'subscribe_candidates_gathered',
  SUBSCRIBE_RESPONSE_SENT: 'subscribe_response_sent',

  UNSUBSCRIBE_REQUEST_RECEIVED: 'unsubscribe_request_received',
  UNSUBSCRIBE_RESPONSE_SENT: 'unsubscribe_response_sent',

  REMOVING_NATIVE_STREAM: 'native_stream_removing',
  NATIVE_STREAM_REMOVED: 'native_stream_removed',
  NATIVE_STREAM_CLOSED: 'native_stream_closed',

  CONNECTION_STREAM_CLOSED: 'connection_stream_closed',
  CONNECTION_STREAM_REMOVED: 'connection_stream_removed',

  CONNECTION_NEGOTIATION_OFFER_CREATING: 'connection_negotiation_offer_creating',
  CONNECTION_NEGOTIATION_OFFER_CREATED: 'connection_negotiation_offer_created',
  CONNECTION_NEGOTIATION_OFFER_SENT: 'connection_negotiation_offer_sent',
  CONNECTION_NEGOTIATION_ANSWER_RECEIVED: 'connection_negotiation_answer_received',
  CONNECTION_NEGOTIATION_ANSWER_SET: 'connection_negotiation_answer_set',

  CONNECTION_NEGOTIATION_OFFER_RECEIVED: 'connection_negotiation_offer_received',
  CONNECTION_NEGOTIATION_OFFER_SET: 'connection_negotiation_offer_set',
  CONNECTION_NEGOTIATION_ANSWER_CREATING: 'connection_negotiation_answer_creating',
  CONNECTION_NEGOTIATION_ANSWER_CREATED: 'connection_negotiation_answer_created',
  CONNECTION_NEGOTIATION_ANSWER_SENT: 'connection_negotiation_answer_sent',
};

const COMPUTE_MEASURES_INTERVAL = 5000;
const MAX_ID_LIFETIME = 3 * 60 * 1000;

// This class creates a Measure (duration) from two Marks (timestamps).
class PerformanceMeasure {
  constructor(measure, from, to) {
    this.measure = measure;
    this.from = from;
    this.to = to;
  }
}

// Definition of all the Measures in Licode components
const PerformanceMeasures = [
  // ErizoJS addSubscriber requests
  new PerformanceMeasure(Measures.SUBSCRIBE_TOTAL,
    Marks.SUBSCRIBE_REQUEST_RECEIVED,
    Marks.SUBSCRIBE_RESPONSE_SENT),
  new PerformanceMeasure(Measures.SUBSCRIBE_STREAM_CREATED,
    Marks.SUBSCRIBE_REQUEST_RECEIVED,
    Marks.SUBSCRIBE_STREAM_CREATED),
  new PerformanceMeasure(Measures.SUBSCRIBE_CONNECTION_INIT,
    Marks.SUBSCRIBE_STREAM_CREATED,
    Marks.SUBSCRIBE_CONNECTION_INIT),
  new PerformanceMeasure(Measures.SUBSCRIBE_CANDIDATES_GATHERED,
    Marks.SUBSCRIBE_CONNECTION_INIT,
    Marks.SUBSCRIBE_CANDIDATES_GATHERED),
  new PerformanceMeasure(Measures.SUBSCRIBE_CONNECTION_STARTED,
    Marks.SUBSCRIBE_CONNECTION_INIT,
    Marks.SUBSCRIBE_CONNECTION_STARTED),
  new PerformanceMeasure(Measures.SUBSCRIBE_CONNECTION_READY,
    Marks.SUBSCRIBE_CONNECTION_INIT,
    Marks.SUBSCRIBE_CONNECTION_READY),

  // ErizoJS removeSubscriber requests
  new PerformanceMeasure(Measures.UNSUBSCRIBE_TOTAL,
    Marks.UNSUBSCRIBE_REQUEST_RECEIVED,
    Marks.UNSUBSCRIBE_RESPONSE_SENT),
  new PerformanceMeasure(Measures.UNSUBSCRIBE_CLOSE_STREAM,
    Marks.UNSUBSCRIBE_REQUEST_RECEIVED,
    Marks.CONNECTION_STREAM_CLOSED),
  new PerformanceMeasure(Measures.UNSUBSCRIBE_REMOVE_INTERNAL_STREAM,
    Marks.REMOVING_NATIVE_STREAM,
    Marks.NATIVE_STREAM_REMOVED),
  new PerformanceMeasure(Measures.UNSUBSCRIBE_CLOSE_INTERNAL_STREAM,
    Marks.REMOVING_NATIVE_STREAM,
    Marks.NATIVE_STREAM_CLOSED),
  new PerformanceMeasure(Measures.UNSUBSCRIBE_REMOVE_STREAM,
    Marks.CONNECTION_STREAM_CLOSED,
    Marks.CONNECTION_STREAM_REMOVED),
  new PerformanceMeasure(Measures.UNSUBSCRIBE_RESPONSE_SENT,
    Marks.CONNECTION_STREAM_REMOVED,
    Marks.UNSUBSCRIBE_RESPONSE_SENT),


  // ErizoJS Client Negotiation time (erizo initiates)
  new PerformanceMeasure(Measures.CONNECTION_NEGOTIATION_LOCAL_OFFER_TOTAL,
    Marks.CONNECTION_NEGOTIATION_OFFER_CREATING,
    Marks.CONNECTION_NEGOTIATION_ANSWER_SET),
  new PerformanceMeasure(Measures.CONNECTION_NEGOTIATION_OFFER_CREATE,
    Marks.CONNECTION_NEGOTIATION_OFFER_CREATING,
    Marks.CONNECTION_NEGOTIATION_OFFER_CREATED),
  new PerformanceMeasure(Measures.CONNECTION_NEGOTIATION_OFFER_SEND,
    Marks.CONNECTION_NEGOTIATION_OFFER_CREATED,
    Marks.CONNECTION_NEGOTIATION_OFFER_SENT),
  new PerformanceMeasure(Measures.CONNECTION_NEGOTIATION_ANSWER_RECEIVE,
    Marks.CONNECTION_NEGOTIATION_OFFER_SENT,
    Marks.CONNECTION_NEGOTIATION_ANSWER_RECEIVED),
  new PerformanceMeasure(Measures.CONNECTION_NEGOTIATION_ANSWER_SET,
    Marks.CONNECTION_NEGOTIATION_ANSWER_RECEIVED,
    Marks.CONNECTION_NEGOTIATION_ANSWER_SET),

  // ErizoJS Client Negotiation time (client initiates)
  new PerformanceMeasure(Measures.CONNECTION_NEGOTIATION_REMOTE_OFFER_TOTAL,
    Marks.CONNECTION_NEGOTIATION_OFFER_RECEIVED,
    Marks.CONNECTION_NEGOTIATION_ANSWER_SENT),
  new PerformanceMeasure(Measures.CONNECTION_NEGOTIATION_OFFER_SET,
    Marks.CONNECTION_NEGOTIATION_OFFER_RECEIVED,
    Marks.CONNECTION_NEGOTIATION_OFFER_SET),
  new PerformanceMeasure(Measures.CONNECTION_NEGOTIATION_ANSWER_CREATE,
    Marks.CONNECTION_NEGOTIATION_OFFER_SET,
    Marks.CONNECTION_NEGOTIATION_ANSWER_CREATING),
  new PerformanceMeasure(Measures.CONNECTION_NEGOTIATION_ANSWER_CREATED,
    Marks.CONNECTION_NEGOTIATION_ANSWER_CREATING,
    Marks.CONNECTION_NEGOTIATION_ANSWER_CREATED),
  new PerformanceMeasure(Measures.CONNECTION_NEGOTIATION_ANSWER_SEND,
    Marks.CONNECTION_NEGOTIATION_ANSWER_CREATED,
    Marks.CONNECTION_NEGOTIATION_ANSWER_SENT),
];

// Class that will be executed in Licode components
class PerformanceStats {
  constructor() {
    this.measures = new Map();
    this.marks = new Map();
    this.ids = new Map();
    // eslint-disable-next-line no-restricted-syntax
    for (const measure of PerformanceMeasures) {
      const performanceMeasure = Object.assign({}, measure);
      performanceMeasure.stats = new Stats();
      this.measures.set(performanceMeasure.measure, performanceMeasure);
    }

    // We can loss some measures when calculating through intervals, but it looks
    // like a reasonable trade-off to process marks and measures.
    this.computeMeasuresInterval = setInterval(() => {
      this.computeAllMeasures();
    }, COMPUTE_MEASURES_INTERVAL);
  }

  computeAllMeasures() {
    const now = performance.now();
    this.ids.forEach((createdAt, id) => {
      if (now - createdAt >= MAX_ID_LIFETIME) {
        this.computeMeasuresWithId(id);
      }
    });
  }

  forEachMeasure(func = () => {}) {
    this.measures.forEach((performanceMeasure, measure) => {
      func(measure, performanceMeasure);
    });
  }

  // eslint-disable-next-line class-methods-use-this
  markWithId(id, mark) {
    if (id) {
      const now = performance.now();
      log.debug(`message: Marking with id: ${id}, mark: ${mark}`);
      this.ids.set(id, now);
      this.marks.set(`${id}_${mark}`, now);
    }
  }

  computeMeasuresWithId(id) {
    // Compute times
    const marksToDelete = [];
    this.measures.forEach((performanceMeasure) => {
      const to = `${id}_${performanceMeasure.to}`;
      const from = `${id}_${performanceMeasure.from}`;
      if (this.marks.has(to) && this.marks.has(from)) {
        const value = this.marks.get(to) - this.marks.get(from);
        performanceMeasure.stats.push(value);
      } else {
        log.debug(`message: failed when measuring between marks, id: ${id}, measure: ${performanceMeasure.measure}, ` +
          `from: ${performanceMeasure.from} (${this.marks.has(from)}), to: ${performanceMeasure.to} (${this.marks.has(to)})`);
      }
      marksToDelete.push(to);
      marksToDelete.push(from);
    });
    marksToDelete.forEach(mark => this.marks.delete(mark));
    this.ids.delete(id);
  }

  resetStats() {
    this.measures.forEach((performanceMeasure) => {
      performanceMeasure.stats.reset();
    });
  }

  process() {
    const result = {};
    this.measures.forEach((performanceMeasure) => {
      // eslint-disable-next-line no-restricted-syntax
      for (const stat of StatMetrics) {
        try {
          const statFunction = performanceMeasure.stats[stat];
          let value;
          if (statFunction instanceof Function) {
            value = statFunction.apply(performanceMeasure.stats);
          } else {
            value = statFunction || NaN;
          }
          result[`${performanceMeasure.measure}_${stat}`] = value;
        } catch (e) {
          log.error(`message: Error computing stat, mesasure: ${performanceMeasure.measure}, stat: ${stat}, error: ${e.stack}`);
        }
      }
    });
    this.resetStats();
    log.debug(`message: Processing results, results: ${JSON.stringify(result)}`);
    return result;
  }
}

// Class that will be executed in RovMetricsServer to gather all
// measures obtained by ROV from Licode components, compute the max among them
// and set them to Prometheus Client.
class PrometheusExporter {
  constructor(prometheusClient, getNameWithPrefix) {
    this.promClient = prometheusClient;
    this.metrics = new Map();
    this.getNameWithPrefix = getNameWithPrefix;
    // eslint-disable-next-line no-restricted-syntax
    for (const measure of PerformanceMeasures) {
      StatMetrics.forEach((stat) => {
        this.defineMetricFromMeasure(measure.measure, stat);
      });
    }
  }

  defineMetricFromMeasure(measure, stat) {
    const finalMeasure = `${measure}_${stat}`;
    this.metrics.set(finalMeasure, {
      metric: new this.promClient.Gauge({
        name: this.getNameWithPrefix(finalMeasure),
        help: finalMeasure }),
      max: 0,
    });
  }

  exportToPrometheus(results) {
    // Calculate Max value from all results
    results.forEach((data) => {
      const result = JSON.parse(data);
      Object.getOwnPropertyNames(result).forEach((measureName) => {
        if (this.metrics.has(measureName)) {
          const metric = this.metrics.get(measureName);
          metric.max = Math.max(metric.max, result[measureName]);
        } else {
          log.error('Does not have metric', this.getNameWithPrefix(measureName));
        }
      });
    });

    // Export it to prometheus and reset for the next iteration
    this.metrics.forEach((metric) => {
      metric.metric.set(metric.max);
      // eslint-disable-next-line no-param-reassign
      metric.max = 0;
    });
  }
}

exports.init = () => {
  performanceStats = new PerformanceStats();
};

exports.mark = (id, mark) => {
  log.debug(`message: Premarking with id: ${id}, mark: ${mark}, contains: ${Object.values(Marks).indexOf(mark)}`);
  if (id && performanceStats && Object.values(Marks).indexOf(mark) !== -1) {
    performanceStats.markWithId(id, mark);
  }
};

exports.computeMeasuresWithId = (id) => {
  if (performanceStats) {
    performanceStats.computeMeasuresWithId(id);
  }
};

exports.process = () => {
  if (performanceStats) {
    return performanceStats.process();
  }
  return {};
};

exports.PerformanceMeasures = PerformanceMeasures;
exports.PrometheusExporter = PrometheusExporter;
exports.Marks = Marks;
exports.Measures = Measures;
