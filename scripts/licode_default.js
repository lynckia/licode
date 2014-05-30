var config = {}

/*********************************************************
 COMMON CONFIGURATION
 It's used by Nuve, ErizoController, ErizoAgent and ErizoJS
**********************************************************/
config.rabbit = {};
config.rabbit.host = 'localhost'; //default value: 'localhost'
config.rabbit.port = 5672; //default value: 5672
config.logger = {};
config.logger.config_file = '../log4js_configuration.json'; //default value: "../log4js_configuration.json"

/*********************************************************
 CLOUD PROVIDER CONFIGURATION
 It's used by Nuve and ErizoController
**********************************************************/
config.cloudProvider = {};
config.cloudProvider.name = '';
//In Amazon Ec2 instances you can specify the zone host. By default is 'ec2.us-east-1a.amazonaws.com' 
config.cloudProvider.host = '';
config.cloudProvider.accessKey = '';
config.cloudProvider.secretAccessKey = '';

/*********************************************************
 NUVE CONFIGURATION
**********************************************************/
config.nuve = {};
config.nuve.dataBaseURL = "localhost/nuvedb"; // default value: 'localhost/nuvedb'
config.nuve.superserviceID = '_auto_generated_ID_'; // default value: ''
config.nuve.superserviceKey = '_auto_generated_KEY_'; // default value: ''
config.nuve.testErizoController = 'localhost:8080'; // default value: 'localhost:8080'

/*********************************************************
 ERIZO CONTROLLER CONFIGURATION
**********************************************************/
config.erizoController = {};

//Use undefined to run clients without Stun 
config.erizoController.stunServerUrl = 'stun:stun.l.google.com:19302'; // default value: 'stun:stun.l.google.com:19302'

// Default and max video bandwidth parameters to be used by clients
config.erizoController.defaultVideoBW = 300; //default value: 300
config.erizoController.maxVideoBW = 300; //default value: 300

// Public erizoController IP for websockets (useful when behind NATs)
// Use '' to automatically get IP from the interface
config.erizoController.publicIP = ''; //default value: ''
// Use '' to use the public IP address instead of a hostname
config.erizoController.hostname = ''; //default value: ''
config.erizoController.port = 8080; //default value: 8080
// Use true if clients communicate with erizoController over SSL
config.erizoController.ssl = false; //default value: false

// Use the name of the inferface you want to bind to for websockets
// config.erizoController.networkInterface = 'eth1' // default value: undefined

//Use undefined to run clients without Turn
config.erizoController.turnServer = {}; // default value: undefined
config.erizoController.turnServer.url = ''; // default value: null
config.erizoController.turnServer.username = ''; // default value: null
config.erizoController.turnServer.password = ''; // default value: null

config.erizoController.warning_n_rooms = 15; // default value: 15
config.erizoController.limit_n_rooms = 20; // default value: 20
config.erizoController.interval_time_keepAlive = 1000; // default value: 1000

// Roles to be used by services
config.erizoController.roles = {"presenter":["publish", "subscribe", "record"], "viewer":["subscribe"]}; // default value: {"presenter":["publish", "subscribe", "record"], "viewer":["subscribe"]}

// If true, erizoController sends stats to rabbitMQ queue "stats_handler" 
config.erizoController.sendStats = false; // default value: false

// If undefined, the path will be /tmp/
config.erizoController.recording_path = undefined; // default value: undefined

/*********************************************************
 ERIZO AGENT CONFIGURATION
**********************************************************/
config.erizoAgent = {};

// Max processes that ErizoAgent can run
config.erizoAgent.maxProcesses 	  = 1; // default value: 1
// Number of precesses that ErizoAgent runs when it starts. Always lower than or equals to maxProcesses.
config.erizoAgent.prerunProcesses = 1; // default value: 1

/*********************************************************
 ERIZO JS CONFIGURATION
**********************************************************/
config.erizo = {};

//STUN server IP address and port to be used by the server.
//if '' is used, the address is discovered locally
config.erizo.stunserver = ''; // default value: ''
config.erizo.stunport = 0; // default value: 0

//note, this won't work with all versions of libnice. With 0 all the available ports are used
config.erizo.minport = 0; // default value: 0
config.erizo.maxport = 0; // default value: 0

/***** END *****/
// Following lines are always needed.
var module = module || {};
module.exports = config;