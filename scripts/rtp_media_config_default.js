var mediaConfig = {};

mediaConfig.extMappings = [
  "urn:ietf:params:rtp-hdrext:ssrc-audio-level",
  "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time",
  "urn:ietf:params:rtp-hdrext:toffset",
  "urn:3gpp:video-orientation",
  // "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01",
  "http://www.webrtc.org/experiments/rtp-hdrext/playout-delay",
  "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id"
];

mediaConfig.rtpMappings = {};

mediaConfig.rtpMappings.vp8 = {
    payloadType: 100,
    encodingName: 'VP8',
    clockRate: 90000,
    channels: 1,
    mediaType: 'video',
    feedbackTypes: [
        'ccm fir',
        'nack',
        'nack pli',
        'goog-remb',
//        'transport-cc',
    ],
};
/*
mediaConfig.rtpMappings.red = {
    payloadType: 116,
    encodingName: 'red',
    clockRate: 90000,
    channels: 1,
    mediaType: 'video',
};

mediaConfig.rtpMappings.rtx = {
    payloadType: 96,
    encodingName: 'rtx',
    clockRate: 90000,
    channels: 1,
    mediaType: 'video',
    formatParameters: {
        apt: '100'
    },
};

mediaConfig.rtpMappings.ulpfec = {
    payloadType: 117,
    encodingName: 'ulpfec',
    clockRate: 90000,
    channels: 1,
    mediaType: 'video',
};

mediaConfig.rtpMappings.opus = {
    payloadType: 111,
    encodingName: 'opus',
    clockRate: 48000,
    channels: 2,
    mediaType: 'audio',
};

mediaConfig.rtpMappings.isac16 = {
    payloadType: 103,
    encodingName: 'ISAC',
    clockRate: 16000,
    channels: 1,
    mediaType: 'audio',
};

mediaConfig.rtpMappings.isac32 = {
    payloadType: 104,
    encodingName: 'ISAC',
    clockRate: 32000,
    channels: 1,
    mediaType: 'audio',
};

*/
mediaConfig.rtpMappings.pcmu = {
    payloadType: 0,
    encodingName: 'PCMU',
    clockRate: 8000,
    channels: 1,
    mediaType: 'audio',
};
/*
mediaConfig.rtpMappings.pcma = {
    payloadType: 8,
    encodingName: 'PCMA',
    clockRate: 8000,
    channels: 1,
    mediaType: 'audio',
};

mediaConfig.rtpMappings.cn8 = {
    payloadType: 13,
    encodingName: 'CN',
    clockRate: 8000,
    channels: 1,
    mediaType: 'audio',
};

mediaConfig.rtpMappings.cn16 = {
    payloadType: 105,
    encodingName: 'CN',
    clockRate: 16000,
    channels: 1,
    mediaType: 'audio',
};

mediaConfig.rtpMappings.cn32 = {
    payloadType: 106,
    encodingName: 'CN',
    clockRate: 32000,
    channels: 1,
    mediaType: 'audio',
};

mediaConfig.rtpMappings.cn48 = {
    payloadType: 107,
    encodingName: 'CN',
    clockRate: 48000,
    channels: 1,
    mediaType: 'audio',
};
*/
mediaConfig.rtpMappings.telephoneevent = {
    payloadType: 126,
    encodingName: 'telephone-event',
    clockRate: 8000,
    channels: 1,
    mediaType: 'audio',
};

var module = module || {};
module.exports = mediaConfig;
