const config = require('config');
const opts = require('./options');

// Configuration default values
global.config = {};
global.config.erizoAgent = config.get('erizoAgent');

opts.processArgv();

// Load submodules with updated config
const logger = require('./common/logger').logger;
const amqper = require('./common/amqper');

const metrics = require('./metrics');
const mgr = require('./erizoJSManager');

const log = logger.getLogger('ErizoAgent');

mgr.start();

// TODO: get metadata from a file
const reporter = require('./erizoAgentReporter').Reporter({
    id: mgr.erizoAgentId,
    metadata: opts.metadata
});

const api = {
    createErizoJS(callback) {
        try {
            mgr.createErizoJS(callback);
        } catch (error) {
            log.error('message: error creating ErizoJS, error:', error);
        }
    },

    deleteErizoJS(id, callback) {
        try {
            mgr.dropErizoJS(id, callback);
        } catch (error) {
            log.error('message: error stopping ErizoJS, error:', error);
        }
    },

    getErizoAgents: reporter.getErizoAgent
};

// Will clean all erizoJS on those signals
const onSignal = () => {
    mgr.cleanErizos();
    process.exit(0);
};

process.on('SIGINT', onSignal);
process.on('SIGTERM', onSignal);

metrics.startReporter(mgr.erizoAgentId);

amqper.connect(() => {
    amqper.setPublicRPC(api);
    amqper.bind('ErizoAgent');
    amqper.bind(`ErizoAgent_${mgr.erizoAgentId}`);
    amqper.bindBroadcast('ErizoAgent', () => {
        log.warn('message: amqp no method defined');
    });
});
