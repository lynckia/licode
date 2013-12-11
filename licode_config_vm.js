var config = {}

config.rabbit = {};
config.nuve = {};
config.erizoController = {};
config.cloudProvider = {};
config.erizo = {};

config.minervaHost = "av-local.minervaproject.com";		

config.rabbit.host = 'localhost';
config.rabbit.port = 5672;

config.nuve.dataBaseURL = "localhost/nuvedb";
config.nuve.superserviceID = '5265b9a26c002eae466693e8';
config.nuve.superserviceKey = '13263';
config.nuve.testErizoController = 'localhost:8080';

//Use undefined to run clients without Stun 
config.erizoController.stunServerUrl = 'stun:stun.l.google.com:19302';
config.erizoController.warning_n_rooms = 15;
config.erizoController.limit_n_rooms = 20;
config.erizoController.interval_time_keepAlive = 1000;

//STUN server IP address and port to be used by the server.
//if '' is used, the address is discovered locally
config.erizo.stunserver = 'stun:stun.l.google.com';
config.erizo.stunport = 19302;

//note, this won't work with all versions of libnice. With 0 all the available ports are used
config.erizo.minport = 0;
config.erizo.maxport = 0;

config.cloudProvider.name = '';
//In Amazon Ec2 instances you can specify the zone host. By default is 'ec2.us-east-1a.amazonaws.com' 
config.cloudProvider.host = '';
config.cloudProvider.accessKey = '';
config.cloudProvider.secretAccessKey = '';

module.exports = config;
