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
  SUBSCRIBE_STREAM_CREATED: 'subscribe_connection_started',
  SUBSCRIBE_CANDIDATES_GATHERED: 'subscribe_candidates_gathered',
  SUBSCRIBE_CONNECTION_INIT: 'subscribe_connection_init',
  SUBSCRIBE_OFFER_QUEUE: 'subscribe_offer_queue',
  SUBSCRIBE_OFFER_CREATED: 'subscribe_offer_created',
  SUBSCRIBE_OFFER_SENT: 'subscribe_offer_sent',

  UNSUBSCRIBE_OFFER_QUEUE: 'unsubscribe_offer_queue',
  UNSUBSCRIBE_OFFER_CREATED: 'unsubscribe_offer_created',
  UNSUBSCRIBE_OFFER_SENT: 'unsubscribe_offer_sent',
  UNSUBSCRIBE_REMOVE_STREAM: 'unsubscribe_remove_stream',
  UNSUBSCRIBE_CLOSE_STREAM: 'unsubscribe_close_stream',
  UNSUBSCRIBE_TOTAL: 'unsubscribe_total',
};

// Timestamp marks that will take part of the Measures.
const Marks = {
  SUBSCRIBE_REQUEST_RECEIVED: 'subscribe_request_received',
  SUBSCRIBE_STREAM_CREATED: 'subscribe_stream_created',
  SUBSCRIBE_CONNECTION_INIT: 'subscribe_connection_init',
  SUBSCRIBE_CANDIDATES_GATHERED: 'subscribe_candidates_gathered',
  SUBSCRIBE_RESPONSE_SENT: 'subscribe_response_sent',

  UNSUBSCRIBE_REQUEST_RECEIVED: 'unsubscribe_request_received',
  UNSUBSCRIBE_RESPONSE_SENT: 'unsubscribe_response_sent',

  CONNECTION_OFFER_SENT: 'connection_offer_sent',
  CONNECTION_OFFER_ENQUEUED: 'connection_offer_enqueued',
  CONNECTION_OFFER_DEQUEUED: 'connection_offer_dequeued',
  CONNECTION_OFFER_CREATED: 'connection_offer_created',
  CONNECTION_STREAM_CLOSED: 'connection_stream_closed',
  CONNECTION_STREAM_REMOVED: 'connection_stream_removed',
  CONNECTION_STREAM_REMOVED_AND_CLOSED: 'connection_stream_closed_and_removed',
};

const COMPUTE_MEASURES_INTERVAL = 5000;
const MAX_ID_LIFETIME = 3 * 60 * 1000;

// This class creates a Measure (duration) from two Marks (timestamps).
class PerfromanceMeasure {
  constructor(measure, from, to) {
    this.measure = measure;
    this.from = from;
    this.to = to;
  }
}

// Definition of all the Measures in Licode components
const PerformanceMeasures = [
  // ErizoJS addSubscriber requests
  new PerfromanceMeasure(Measures.SUBSCRIBE_TOTAL,
    Marks.SUBSCRIBE_REQUEST_RECEIVED,
    Marks.SUBSCRIBE_RESPONSE_SENT),
  new PerfromanceMeasure(Measures.SUBSCRIBE_STREAM_CREATED,
    Marks.SUBSCRIBE_REQUEST_RECEIVED,
    Marks.SUBSCRIBE_STREAM_CREATED),
  new PerfromanceMeasure(Measures.SUBSCRIBE_CONNECTION_INIT,
    Marks.SUBSCRIBE_STREAM_CREATED,
    Marks.SUBSCRIBE_CONNECTION_INIT),
  new PerfromanceMeasure(Measures.SUBSCRIBE_CANDIDATES_GATHERED,
    Marks.SUBSCRIBE_CONNECTION_INIT,
    Marks.SUBSCRIBE_CANDIDATES_GATHERED),
  new PerfromanceMeasure(Measures.SUBSCRIBE_OFFER_QUEUE,
    Marks.CONNECTION_OFFER_ENQUEUED,
    Marks.CONNECTION_OFFER_DEQUEUED),
  new PerfromanceMeasure(Measures.SUBSCRIBE_OFFER_CREATED,
    Marks.CONNECTION_OFFER_DEQUEUED,
    Marks.CONNECTION_OFFER_CREATED),
  new PerfromanceMeasure(Measures.SUBSCRIBE_OFFER_SENT,
    Marks.CONNECTION_OFFER_CREATED,
    Marks.CONNECTION_OFFER_SENT),

  // ErizoJS removeSubscriber requests
  new PerfromanceMeasure(Measures.UNSUBSCRIBE_TOTAL,
    Marks.UNSUBSCRIBE_REQUEST_RECEIVED,
    Marks.UNSUBSCRIBE_RESPONSE_SENT),
  new PerfromanceMeasure(Measures.UNSUBSCRIBE_REMOVE_STREAM,
    Marks.UNSUBSCRIBE_REQUEST_RECEIVED,
    Marks.CONNECTION_STREAM_REMOVED),
  new PerfromanceMeasure(Measures.UNSUBSCRIBE_CLOSE_STREAM,
    Marks.UNSUBSCRIBE_REQUEST_RECEIVED,
    Marks.CONNECTION_STREAM_CLOSED),
  new PerfromanceMeasure(Measures.UNSUBSCRIBE_OFFER_QUEUE,
    Marks.CONNECTION_OFFER_ENQUEUED,
    Marks.CONNECTION_OFFER_DEQUEUED),
  new PerfromanceMeasure(Measures.UNSUBSCRIBE_OFFER_CREATED,
    Marks.CONNECTION_OFFER_DEQUEUED,
    Marks.CONNECTION_OFFER_CREATED),
  new PerfromanceMeasure(Measures.UNSUBSCRIBE_OFFER_SENT,
    Marks.CONNECTION_OFFER_CREATED,
    Marks.CONNECTION_OFFER_SENT),

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
      this.marks.delete(to);
      this.marks.delete(from);
    });
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
