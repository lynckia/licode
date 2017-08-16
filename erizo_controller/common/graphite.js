var graphite = require('graphite-tcp');
var config = require('config');
var logger = require('./logger').logger;

var log = logger.getLogger('GRAPHITE');

var GRAPHITE_CONFIG = config.get('graphite');
var options = Object.assign({}, GRAPHITE_CONFIG, {
    callback: function(err, metricsSent) {
        if (err) {
            log.error('Failed to sent metrics to Graphite: ' + logger.objectToLog(err));
        }
        log.debug('Metrics has been sent to Graphite: ' + metricsSent);
    }
})
var graphiteClient = graphite.createClient(options);

module.exports = graphiteClient;