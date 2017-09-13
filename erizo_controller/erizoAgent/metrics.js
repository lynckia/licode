const util = require('util');

const graphite = require('./common/graphite');
const logger = require('./common/logger').logger;

const db = require('./database').db;

const log = logger.getLogger('ErizoAgent');

function reportMetrics(erizoAgentId) {
    const now = Date.now();
    const interval = global.config.erizoAgent.statsUpdateInterval * 2;

    db.erizoJS.aggregate(
        [
            {
                $match: {
                    erizoAgentID: erizoAgentId,
                    lastUpdated: { $lte: new Date(now), $gte: new Date(now - interval) }
                }
            },
            { $sort: { lastUpdated: -1 } },
            {
                $group: {
                    _id: { erizoAgentID: '$erizoAgentID', erizoJSID: '$erizoJSID' },
                    publishersCount: { $first: '$publishersCount' },
                    subscribersCount: { $first: '$subscribersCount' },
                    stats: { $first: '$stats' }
                }
            },
            {
                $group: {
                    _id: '$_id.erizoAgentID',

                    publishersCount: { $sum: '$publishersCount' },
                    subscribersCount: { $sum: '$subscribersCount' },

                    video_fractionLost_min: { $min: '$stats.video.fractionLost.min' },
                    video_fractionLost_max: { $max: '$stats.video.fractionLost.max' },
                    video_fractionLost_total: { $sum: '$stats.video.fractionLost.total' },
                    video_fractionLost_count: { $sum: '$stats.video.fractionLost.count' },

                    video_jitter_min: { $min: '$stats.video.jitter.min' },
                    video_jitter_max: { $max: '$stats.video.jitter.max' },
                    video_jitter_total: { $sum: '$stats.video.jitter.total' },
                    video_jitter_count: { $sum: '$stats.video.jitter.count' }
                }
            }
        ],
        (err, docs) => {
            if (err) {
                log.warn('failed to collect erizoAgent metrics', err);
                return;
            }

            if (docs.length === 0) {
                return;
            }

            if (docs.length !== 1) {
                log.error(`expected single document result, got ${util.inspect(docs)}`);
                return;
            }

            const d = docs[0];

            graphite.put('publishers.count', d.publishersCount);
            graphite.put('subscribers.count', d.subscribersCount);

            if (d.video_fractionLost_count > 0) {
                graphite.put('video.fractionLost.min', d.video_fractionLost_min);
                graphite.put('video.fractionLost.max', d.video_fractionLost_max);
                graphite.put('video.fractionLost.avg', d.video_fractionLost_total / d.video_fractionLost_count);
            }

            if (d.video_jitter_count > 0) {
                graphite.put('video.jitter.min', d.video_jitter_min);
                graphite.put('video.jitter.max', d.video_jitter_max);
                graphite.put('video.jitter.avg', d.video_jitter_total / d.video_jitter_count);
            }

            log.debug('submitted erizoAgent metrics');
        }
    );
}

exports.startReporter = (erizoAgentId) => {
    setInterval(() => reportMetrics(erizoAgentId), global.config.erizoAgent.statsUpdateInterval);
};
