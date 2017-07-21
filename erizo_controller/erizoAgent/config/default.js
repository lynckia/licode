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

// Custom log directory for agent instance log files.
// If useIndividualLogFiles is enabled, files will go here
// Default is [licode_path]/erizo_controller/erizoAgent
config.erizoAgent.instanceLogDir = '.';


module.exports = config;
