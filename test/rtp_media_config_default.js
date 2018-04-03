const mediaConfig = {};

const extMappings = [
  'urn:ietf:params:rtp-hdrext:ssrc-audio-level',
  'http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time',
  'urn:ietf:params:rtp-hdrext:toffset',
  'urn:3gpp:video-orientation',
  // 'http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01',
  'http://www.webrtc.org/experiments/rtp-hdrext/playout-delay',
  'urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id',
];

const vp8 = {
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
    // 'transport-cc',
  ],
};

const vp9 = {
  payloadType: 100,
  encodingName: 'VP9',
  clockRate: 90000,
  channels: 1,
  mediaType: 'video',
  feedbackTypes: [
    'ccm fir',
    'nack',
    'nack pli',
    'goog-remb',
    // 'transport-cc',
  ],
};

const h264 = {
  payloadType: 101,
  encodingName: 'H264',
  clockRate: 90000,
  channels: 1,
  mediaType: 'video',
  formatParameters: {
    'profile-level-id': '42e01f',
    'level-asymmetry-allowed': '1',
    'packetization-mode': '1',
  },
  feedbackTypes: [
    'ccm fir',
    'nack',
    'nack pli',
    'goog-remb',
    // 'transport-cc',
  ],
};

const red = {
  payloadType: 116,
  encodingName: 'red',
  clockRate: 90000,
  channels: 1,
  mediaType: 'video',
};

const rtx = {
  payloadType: 96,
  encodingName: 'rtx',
  clockRate: 90000,
  channels: 1,
  mediaType: 'video',
  formatParameters: {
    apt: '100',
  },
};

const ulpfec = {
  payloadType: 117,
  encodingName: 'ulpfec',
  clockRate: 90000,
  channels: 1,
  mediaType: 'video',
};

const opus = {
  payloadType: 111,
  encodingName: 'opus',
  clockRate: 48000,
  channels: 2,
  mediaType: 'audio',
};

const isac16 = {
  payloadType: 103,
  encodingName: 'ISAC',
  clockRate: 16000,
  channels: 1,
  mediaType: 'audio',
};

const isac32 = {
  payloadType: 104,
  encodingName: 'ISAC',
  clockRate: 32000,
  channels: 1,
  mediaType: 'audio',
};

const pcmu = {
  payloadType: 0,
  encodingName: 'PCMU',
  clockRate: 8000,
  channels: 1,
  mediaType: 'audio',
};

const pcma = {
  payloadType: 8,
  encodingName: 'PCMA',
  clockRate: 8000,
  channels: 1,
  mediaType: 'audio',
};

const cn8 = {
  payloadType: 13,
  encodingName: 'CN',
  clockRate: 8000,
  channels: 1,
  mediaType: 'audio',
};

const cn16 = {
  payloadType: 105,
  encodingName: 'CN',
  clockRate: 16000,
  channels: 1,
  mediaType: 'audio',
};

const cn32 = {
  payloadType: 106,
  encodingName: 'CN',
  clockRate: 32000,
  channels: 1,
  mediaType: 'audio',
};

const cn48 = {
  payloadType: 107,
  encodingName: 'CN',
  clockRate: 48000,
  channels: 1,
  mediaType: 'audio',
};

const telephoneevent = {
  payloadType: 126,
  encodingName: 'telephone-event',
  clockRate: 8000,
  channels: 1,
  mediaType: 'audio',
};

mediaConfig.codecConfigurations = {
  default: { rtpMappings: { vp8, opus }, extMappings },
  VP8_AND_OPUS: { rtpMappings: { vp8, opus }, extMappings },
  VP9_AND_OPUS: { rtpMappings: { vp9, opus }, extMappings },
  H264_AND_OPUS: { rtpMappings: { h264, opus }, extMappings },
};

var module = module || {};
module.exports = mediaConfig;
