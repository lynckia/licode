/*global exports, require, console*/
var cloudHandler = require('../cloudHandler');

/*
 * Get Erizo Controllers
 */
exports.represent = function (req, res) {
    "use strict";

    cloudHandler.getErizoControllers(function (controllers) {
        res.send(controllers);
    });
};
