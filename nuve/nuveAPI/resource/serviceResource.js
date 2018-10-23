/* global exports, require */


const serviceRegistry = require('./../mdb/serviceRegistry');
const logger = require('./../logger').logger;

// Logger
const log = logger.getLogger('ServiceResource');

/*
 * Gets the service and checks if it is superservice.
 * Only superservice can do actions about services.
 */
const doInit = (req, callback) => {
  const service = req.service;
  // eslint-disable-next-line global-require
  const superService = require('./../mdb/dataBase').superService;

  service._id = `${service._id}`;
  if (service._id !== superService) {
    callback('error');
  } else {
    serviceRegistry.getService(req.params.service, (ser) => {
      callback(ser);
    });
  }
};

/*
 * Get Service. Represents a determined service.
 */
exports.represent = (req, res) => {
  doInit(req, (serv) => {
    if (serv === 'error') {
      log.info(`message: represent service - not authorized, serviceId: ${req.params.service}`);
      res.status(401).send('Service not authorized for this action');
      return;
    }
    if (serv === undefined) {
      res.status(404).send('Service not found');
      return;
    }
    log.info('Representing service ', serv._id);
    res.send(serv);
  });
};

/*
 * Delete Service. Removes a determined service from the data base.
 */
exports.deleteService = (req, res) => {
  doInit(req, (serv) => {
    if (serv === 'error') {
      log.info(`message: deleteService - not authorized, serviceId: ${req.params.service}`);
      res.status(401).send('Service not authorized for this action');
      return;
    }
    if (serv === undefined) {
      res.status(404).send('Service not found');
      return;
    }
    let id = '';
    id += serv._id;
    serviceRegistry.removeService(id);
    log.info(`message: deleteService success, serviceId: ${id}`);
    res.send('Service deleted');
  });
};
