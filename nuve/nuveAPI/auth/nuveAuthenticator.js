/* global require, exports */


const serviceRegistry = require('./../mdb/serviceRegistry');
const mauthParser = require('./mauthParser');

const logger = require('./../logger').logger;

// Logger
const log = logger.getLogger('NuveAuthenticator');

const cache = {};

const checkTimestamp = (ser, params) => {
  const lastParams = cache[ser.name];

  if (lastParams === undefined) {
    return true;
  }

  const lastTS = lastParams.timestamp;
  const newTS = params.timestamp;
  const lastC = lastParams.cnonce;
  const newC = params.cnonce;

  if (newTS < lastTS || (lastTS === newTS && lastC === newC)) {
    log.debug(`message: checkTimestamp lastTimestamp: ${lastTS}, newTimestamp: ${newTS
    }, lastCnonce: ${lastC}, newCnonce: ${newC}`);
    return false;
  }

  return true;
};

const checkSignature = (params, key) => {
  if (params.signature_method !== 'HMAC_SHA1') {
    return false;
  }

  const calculatedSignature = mauthParser.calculateClientSignature(params, key);

  if (calculatedSignature !== params.signature) {
    return false;
  }
  return true;
};

/*
 * This function has the logic needed for authenticate a nuve request.
 * If the authentication success exports the service and the user and role (if needed).
 * Else send back a response with an authentication request to the client.
 */
exports.authenticate = (req, res, next) => {
  const authHeader = req.header('Authorization');
  const challengeReq = 'MAuth realm="http://marte3.dit.upm.es"';
  let params;

  if (authHeader !== undefined) {
    params = mauthParser.parseHeader(authHeader);

    // Get the service from the data base.
    serviceRegistry.getService(params.serviceid, (serv) => {
      if (serv === undefined || serv === null) {
        log.info(`message: authenticate fail - unknown service, serviceId: ${
          params.serviceid}`);
        res.status(401).send({ 'WWW-Authenticate': challengeReq });
        return;
      }

      const key = serv.key;

      // Check if timestam and cnonce are valids in order to avoid duplicate requests.
      if (!checkTimestamp(serv, params)) {
        log.info('message: authenticate fail - Invalid timestamp or cnonce');
        res.status(401).send({ 'WWW-Authenticate': challengeReq });
        return;
      }

      // Check if the signature is valid.
      if (checkSignature(params, key)) {
        if (params.username !== undefined && params.role !== undefined) {
          req.user = params.username;
          req.role = params.role;
        }

        cache[serv.name] = params;
        req.service = serv;

        // If everything in the authentication is valid continue with the request.
        next();
      } else {
        log.info('message: authenticate fail - wrong credentials');
        res.status(401).send({ 'WWW-Authenticate': challengeReq });
      }
    });
  } else {
    log.info('message: authenticate fail - MAuth header not present');
    res.status(401).send({ 'WWW-Authenticate': challengeReq });
  }
};
