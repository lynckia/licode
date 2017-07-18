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
config.nuve.dataBaseURL = "localhost/nuvedb"; // default value: 'localhost/nuvedb'
config.nuve.superserviceID = ''; // default value: ''
config.nuve.superserviceKey = ''; // default value: ''
config.nuve.testErizoController = 'localhost:8080'; // default value: 'localhost:8080'
// Nuve Cloud Handler policies are in nuve/nuveAPI/ch_policies/ folder
config.nuve.cloudHandlerPolicy = 'default_policy.js'; // default value: 'default_policy.js'

module.exports = config;
