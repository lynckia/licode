/*global require, exports */
'use strict';
var serviceRegistry = require('./../mdb/serviceRegistry');
var mauthParser = require('./mauthParser');

var logger = require('./../logger').logger;

// Logger
var log = logger.getLogger('NuveAuthenticator');

var cache = {};
var checkTimestampRequired = true;
if (process.env.NUVE_AUTH_CHECK_TIMESTAMP === 'false') {
    checkTimestampRequired = false;
}

var checkTimestamp = function (ser, params) {
    var lastParams = cache[ser.name],
        lastTS,
        newTS,
        lastC,
        newC;

    if (lastParams === undefined) {
        return true;
    }

    lastTS = lastParams.timestamp;
    newTS = params.timestamp;
    lastC = lastParams.cnonce;
    newC = params.cnonce;

    if (newTS < lastTS || (lastTS === newTS && lastC === newC)) {
        log.debug('message: checkTimestamp lastTimestamp: ' + lastTS + ', newTimestamp: ' + newTS +
            ', lastCnonce: ' + lastC + ', newCnonce: ' + newC);
        return false;
    }

    return true;
};

var checkSignature = function (params, key) {
    if (params.signature_method !== 'HMAC_SHA1') {  // jshint ignore:line
        return false;
    }

    var calculatedSignature = mauthParser.calculateClientSignature(params, key);

    if (calculatedSignature !== params.signature) {
        return false;
    } else {
        return true;
    }
};

/*
 * This function has the logic needed for authenticate a nuve request.
 * If the authentication success exports the service and the user and role (if needed).
 * Else send back a response with an authentication request to the client.
 */
exports.authenticate = function (req, res, next) {
    var authHeader = req.header('Authorization'),
        challengeReq = 'MAuth realm="http://marte3.dit.upm.es"',
        params;

    if (authHeader !== undefined) {

        params = mauthParser.parseHeader(authHeader);

        // Get the service from the data base.
        serviceRegistry.getService(params.serviceid, function (serv) {
            if (serv === undefined || serv === null) {
                log.info('message: authenticate fail - unknown service, serviceId: ' +
                    params.serviceid);
                res.status(401).send({'WWW-Authenticate': challengeReq});
                return;
            }

            var key = serv.key;

            // Check if timestam and cnonce are valids in order to avoid duplicate requests.
            if (checkTimestampRequired && !checkTimestamp(serv, params)) {
                log.info('message: authenticate fail - Invalid timestamp or cnonce');
                res.status(401).send({'WWW-Authenticate': challengeReq});
                return;
            }

            // Check if the signature is valid.
            if (checkSignature(params, key)) {

                if (params.username !== undefined && params.role !== undefined) {
                    req.user = params.username;
                    req.role = params.role;
                }

                cache[serv.name] =  params;
                req.service = serv;

                // If everything in the authentication is valid continue with the request.
                next();

            } else {
                log.info('message: authenticate fail - wrong credentials');
                res.status(401).send({'WWW-Authenticate': challengeReq});
                return;
            }

        });

    } else {
        log.info('message: authenticate fail - MAuth header not present');
        res.status(401).send({'WWW-Authenticate': challengeReq});
        return;
    }
};
