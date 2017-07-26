/* global module */

var config = {};

/*********************************************************
 COMMON CONFIGURATION
 It's used by Nuve, ErizoController, ErizoAgent and ErizoJS
 **********************************************************/
config.rabbit = {};
config.rabbit.host = 'localhost'; //default value: 'localhost'
config.rabbit.port = 5672; //default value: 5672
// Sets the AQMP heartbeat timeout to detect dead TCP Connections
config.rabbit.heartbeat = 8; //default value: 8 seconds, 0 to disable
config.rabbit.login = 'guest';
config.rabbit.password = 'guest';
config.logger = {};
config.logger.configFile = './log4js_configuration.json'; //default value: "./log4js_configuration.json"

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
config.nuve.superserviceKey = ''; // default value: ''

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

// Default and max video bandwidth parameters to be used by clients
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

module.exports = config;
