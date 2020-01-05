/* global exports, require */


const serviceRegistry = require('./../mdb/serviceRegistry');
const logger = require('./../logger').logger;

// Logger
const log = logger.getLogger('ServicesResource');

/*
 * Gets the service and checks if it is superservice.
 * Only superservice can do actions about services.
 */
const doInit = (req) => {
  const currentService = req.service;
  // eslint-disable-next-line global-require
  const superService = require('./../mdb/dataBase').superService;
  currentService._id = `${currentService._id}`;
  return (currentService._id === superService);
};

/*
 * Post Service. Creates a new service.
 */
exports.create = (req, res) => {
  if (!doInit(req)) {
    log.info(`message: createService - unauthorized, serviceId: ${req.service._id}`);
    res.status(401).send('Service not authorized for this action');
    return;
  }

  serviceRegistry.addService(req.body, (result) => {
    log.info(`message: createService success, serviceName: ${req.body.name}`);
    res.send(result);
  });
};

/*
 * Get Service. Represents a determined service.
 */
exports.represent = (req, res) => {
  if (!doInit(req)) {
    log.info(`message: representService - not authorised, serviceId: ${req.service._id}`);
    res.status(401).send('Service not authorized for this action');
    return;
  }

  serviceRegistry.getList((list) => {
    log.info('message: representServices');
    res.send(list);
  });
};
