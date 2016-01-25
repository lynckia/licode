/*global require, exports, console */
var db = require('./../mdb/dataBase').db;
var serviceRegistry = require('./../mdb/serviceRegistry');
var mauthParser = require('./mauthParser');

var logger = require('./../logger').logger;

// Logger
var log = logger.getLogger("NuveAuthenticator");

var cache = {};

var checkTimestamp = function (ser, params) {
    "use strict";

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
        log.info('Last timestamp: ', lastTS, ' and new: ', newTS);
        log.info('Last cnonce: ', lastC, ' and new: ', newC);
        return false;
    }

    return true;
};

var checkSignature = function (params, key) {
    "use strict";

    if (params.signature_method !== 'HMAC_SHA1') {
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
 * If the authentication success exports the service and the user and role (if needed). Else send back 
 * a response with an authentication request to the client.
 */
exports.authenticate = function (req, res, next) {
    "use strict";

    var authHeader = req.header('Authorization'),
        challengeReq = 'MAuth realm="http://marte3.dit.upm.es"',
        params;

    if (authHeader !== undefined) {

        params = mauthParser.parseHeader(authHeader);

        // Get the service from the data base.
        serviceRegistry.getService(params.serviceid, function (serv) {
            if (serv === undefined || serv === null) {
                log.info('[Auth] Unknow service:', params.serviceid);
                res.status(401).send({'WWW-Authenticate': challengeReq});
                return;
            }

            var key = serv.key;

            // Check if timestam and cnonce are valids in order to avoid duplicate requests.
            if (!checkTimestamp(serv, params)) {
                log.info('[Auth] Invalid timestamp or cnonce');
                res.status(401).send({'WWW-Authenticate': challengeReq});
                return;
            }

            // Check if the signature is valid. 
            if (checkSignature(params, key)) {

                if (params.username !== undefined && params.role !== undefined) {
                    exports.user = params.username;
                    exports.role = params.role;
                }

                cache[serv.name] =  params;
                exports.service = serv;

                // If everything in the authentication is valid continue with the request.
                next();

            } else {
                log.info('[Auth] Wrong credentials');
                res.status(401).send({'WWW-Authenticate': challengeReq});
                return;
            }

        });

    } else {
        log.info('[Auth] MAuth header not presented');
        res.status(401).send({'WWW-Authenticate': challengeReq});
        return;
    }
};