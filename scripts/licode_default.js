var config = {}

/*********************************************************
 COMMON CONFIGURATION
 It's used by Nuve, ErizoController, ErizoAgent and ErizoJS
**********************************************************/
config.rabbit = {};
config.rabbit.host = 'localhost'; //default value: 'localhost'
config.rabbit.port = 5672; //default value: 5672
// Sets the AQMP heartbeat timeout to detect dead TCP Connections
config.rabbit.heartbeat = 8; //default value: 8 seconds, 0 to disable
config.logger = {};
config.logger.configFile = '../log4js_configuration.json'; //default value: "../log4js_configuration.json"

/*********************************************************
 CLOUD PROVIDER CONFIGURATION
 It's used by Nuve and ErizoController
**********************************************************/
config.cloudProvider = {};
config.cloudProvider.name = '';

/*********************************************************
 NUVE CONFIGURATION
**********************************************************/
config.nuve = {};
config.nuve.dataBaseURL = "localhost/nuvedb"; // default value: 'localhost/nuvedb'
config.nuve.superserviceID = '_auto_generated_ID_'; // default value: ''
config.nuve.superserviceKey = '_auto_generated_KEY_'; // default value: ''
config.nuve.testErizoController = 'localhost:8080'; // default value: 'localhost:8080'
// Nuve Cloud Handler policies are in nuve/nuveAPI/ch_policies/ folder
config.nuve.cloudHandlerPolicy = 'default_policy.js'; // default value: 'default_policy.js'
config.nuve.port = 3000; // default value: 3000


/*********************************************************
 ERIZO CONTROLLER CONFIGURATION
**********************************************************/
config.erizoController = {};

// Use undefined to run clients without Ice Servers
//
// Stun servers format
// {
//     "url": url
// }
//
// Turn servers format
// {
//     "username": username,
//     "credential": password,
//     "url": url
// }
config.erizoController.iceServers = [{'url': 'stun:stun.l.google.com:19302'}]; // default value: [{'url': 'stun:stun.l.google.com:19302'}]

// Default and max video bandwidth parameters to be used by clients for both published and subscribed streams
config.erizoController.defaultVideoBW = 300; //default value: 300
config.erizoController.maxVideoBW = 300; //default value: 300

// Public erizoController IP for websockets (useful when behind NATs)
// Use '' to automatically get IP from the interface
config.erizoController.publicIP = ''; //default value: ''
config.erizoController.networkinterface = ''; //default value: ''

// This configuration is used by the clients to reach erizoController
// Use '' to use the public IP address instead of a hostname
config.erizoController.hostname = ''; //default value: ''
config.erizoController.port = 8080; //default value: 8080
// Use true if clients communicate with erizoController over SSL
config.erizoController.ssl = false; //default value: false

// This configuration is used by erizoController server to listen for connections
// Use true if erizoController listens in HTTPS.
config.erizoController.listen_ssl = false; //default value: false
config.erizoController.listen_port = 8080; //default value: 8080

// Custom location for SSL certificates. Default located in /cert
//config.erizoController.ssl_key = '/full/path/to/ssl.key';
//config.erizoController.ssl_cert = '/full/path/to/ssl.crt';
//config.erizoController.sslCaCerts = ['/full/path/to/ca_cert1.crt', '/full/path/to/ca_cert2.crt'];

// Use the name of the inferface you want to bind to for websockets
// config.erizoController.networkInterface = 'eth1' // default value: undefined

config.erizoController.exitOnNuveCheckFail = false;  // default value: false
config.erizoController.allowSinglePC = false;  // default value: false
config.erizoController.maxErizosUsedByRoom = 100;  // default value: 100

config.erizoController.warning_n_rooms = 15; // default value: 15
config.erizoController.limit_n_rooms = 20; // default value: 20
config.erizoController.interval_time_keepAlive = 1000; // default value: 1000

// Roles to be used by services
config.erizoController.roles =
{"presenter": {"publish": true, "subscribe": true, "record": true, "stats": true, "controlhandlers": true},
    "viewer": {"subscribe": true},
    "viewerWithData": {"subscribe": true, "publish": {"audio": false, "video": false, "screen": false, "data": true}}}; // default value: {"presenter":{"publish": true, "subscribe":true, "record":true}, "viewer":{"subscribe":true}, "viewerWithData":{"subscribe":true, "publish":{"audio":false,"video":false,"screen":false,"data":true}}}

// If true, erizoController sends report to rabbitMQ queue "report_handler"
config.erizoController.report = {
    session_events: false, 		// Session level events -- default value: false
    connection_events: false, 	// Connection (ICE) level events -- default value: false
    rtcp_stats: false				// RTCP stats -- default value: false
};

// Subscriptions to rtcp_stats via AMQP
config.erizoController.reportSubscriptions = {
	maxSubscriptions: 10,	// per ErizoJS -- set 0 to disable subscriptions -- default 10
	minInterval: 1, 		// in seconds -- default 1
	maxTimeout: 60			// in seconds -- default 60
};

// If undefined, the path will be /tmp/
config.erizoController.recording_path = undefined; // default value: undefined

// Erizo Controller Cloud Handler policies are in erizo_controller/erizoController/ch_policies/ folder
config.erizoController.cloudHandlerPolicy = 'default_policy.js'; // default value: 'default_policy.js'

/*********************************************************
 ERIZO AGENT CONFIGURATION
**********************************************************/
config.erizoAgent = {};

// Max processes that ErizoAgent can run
config.erizoAgent.maxProcesses 	  = 1; // default value: 1
// Number of precesses that ErizoAgent runs when it starts. Always lower than or equals to maxProcesses.
config.erizoAgent.prerunProcesses = 1; // default value: 1

// Public erizoAgent IP for ICE candidates (useful when behind NATs)
// Use '' to automatically get IP from the interface
config.erizoAgent.publicIP = ''; //default value: ''
config.erizoAgent.networkinterface = ''; //default value: ''

// Use the name of the inferface you want to bind for ICE candidates
// config.erizoAgent.networkInterface = 'eth1' // default value: undefined

//Use individual log files for each of the started erizoJS processes
//This files will be named erizo-ERIZO_ID_HASH.log
config.erizoAgent.useIndividualLogFiles = false;

// If true this Agent will launch Debug versions of ErizoJS
config.erizoAgent.launchDebugErizoJS = false;

// Custom log directory for agent instance log files.
// If useIndividualLogFiles is enabled, files will go here
// Default is [licode_path]/erizo_controller/erizoAgent
// config.erizoAgent.instanceLogDir = '/path/to/dir';


/*********************************************************
 ERIZO JS CONFIGURATION
**********************************************************/
config.erizo = {};

//Erizo Logs are piped through erizoAgent by default
//you can control log levels in [licode_path]/erizo_controller/erizoAgent/log4cxx.properties

// Number of workers that will be used to handle WebRtcConnections
config.erizo.numWorkers = 24;

// Number of workers what will be used for IO (including ICE logic)
config.erizo.numIOWorkers = 1;

// the max amount of time in days a process is allowed to be up after the first publisher is added
config.erizo.activeUptimeLimit = 7;
// the max time in hours since last publish or subscribe operation where a erizoJS process can be killed
config.erizo.maxTimeSinceLastOperation = 3;
// Interval to check uptime in seconds
config.erizo.checkUptimeInterval = 1800;

//STUN server IP address and port to be used by the server.
//if '' is used, the address is discovered locally
//Please note this is only needed if your server does not have a public IP
config.erizo.stunserver = ''; // default value: ''
config.erizo.stunport = 0; // default value: 0

//TURN server IP address and port to be used by the server.
//Please note this is not needed in most cases, setting TURN in erizoController for the clients
//is the recommended configuration
//if '' is used, no relay for the server is used
config.erizo.turnserver = ''; // default value: ''
config.erizo.turnport = 0; // default value: 0
config.erizo.turnusername = '';
config.erizo.turnpass = '';
config.erizo.networkinterface = ''; //default value: ''

//note, this won't work with all versions of libnice. With 0 all the available ports are used
config.erizo.minport = 0; // default value: 0
config.erizo.maxport = 0; // default value: 0

config.erizo.useConnectionQualityCheck = true; // default value: false

config.erizo.disabledHandlers = []; // there are no handlers disabled by default

/*********************************************************
 ROV CONFIGURATION
**********************************************************/
config.rov = {};
// The stats gathering period in ms
config.rov.statsPeriod = 20000;
// The port to expose the stats to prometheus
config.rov.serverPort = 3005;
// A prefix for the prometheus stats
config.rov.statsPrefix = "licode_";

/*********************************************************
 BASIC EXAMPLE CONFIGURATION
**********************************************************/
config.basicExample = {};
config.basicExample.port = 3001;  // default value: 3001
config.basicExample.tlsPort = 3004; // default value: 3004
config.basicExample.nuveUrl = 'http://localhost:3000/';
config.basicExample.logger = {};
config.basicExample.logger.configFile = './log4js_configuration.json'; // default value: "./log4js_configuration.json"

/***** END *****/
// Following lines are always needed.
var module = module || {};
module.exports = config;
