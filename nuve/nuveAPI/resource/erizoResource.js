/*global exports, require, console*/
var cloudHandler = require('../cloudHandler');

/*
 * Get Erizo Controller Information
 */
exports.getStats = function (req, res) {
    "use strict";

    cloudHandler.getStatsForErizoController(req.params.erizo, function (stats) {
        res.send(stats);
    });
};
