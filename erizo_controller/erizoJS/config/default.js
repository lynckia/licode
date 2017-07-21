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
 ERIZO JS CONFIGURATION
 **********************************************************/
config.erizo = {};

//Erizo Logs are piped through erizoAgent by default
//you can control log levels in [licode_path]/erizo_controller/erizoAgent/log4cxx.properties

// Number of workers that will be used to handle WebRtcConnections
config.erizo.numWorkers = 24;

// Number of workers what will be used for IO (including ICE logic)
config.erizo.numIOWorkers = 1;

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

//Use of internal nICEr library instead of libNice.
config.erizo.useNicer = false;  // default value: false

config.erizo.disabledHandlers = []; // there are no handlers disabled by default

config.erizo.erizoAPIAddonPath = '../erizoAPI/addon';


config.erizoController = {};

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

/*********************************************************
 MEDIA CONFIG
 **********************************************************/
config.mediaConfig = {};
config.mediaConfig.extMappings = [
  "urn:ietf:params:rtp-hdrext:ssrc-audio-level",
  "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time",
  "urn:ietf:params:rtp-hdrext:toffset",
  "urn:3gpp:video-orientation",
  "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay",
  "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id"
];

config.mediaConfig.rtpMappings = {};

config.mediaConfig.rtpMappings.h264 = {
  payloadType: 100,
  encodingName: 'H264',
  clockRate: 90000,
  channels: 1,
  mediaType: 'video',
  feedbackTypes: [
    'ccm fir',
    'nack',
    'nack pli',
    'goog-remb',
  ],
};

config.mediaConfig.rtpMappings.vp8 = {
  payloadType: 101,
  encodingName: 'VP8',
  clockRate: 90000,
  channels: 1,
  mediaType: 'video',
  feedbackTypes: [
    'ccm fir',
    'nack',
    'nack pli',
    'goog-remb',
  ],
};

config.mediaConfig.rtpMappings.opus = {
  payloadType: 111,
  encodingName: 'opus',
  clockRate: 48000,
  channels: 2,
  mediaType: 'audio',
};

config.mediaConfig.rtpMappings.pcmu = {
  payloadType: 0,
  encodingName: 'PCMU',
  clockRate: 8000,
  channels: 1,
  mediaType: 'audio',
};

config.mediaConfig.rtpMappings.telephoneevent = {
  payloadType: 126,
  encodingName: 'telephone-event',
  clockRate: 8000,
  channels: 1,
  mediaType: 'audio',
};

module.exports = config;
