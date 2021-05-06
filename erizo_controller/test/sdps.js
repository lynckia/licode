/* global exports */

function getChromeVideoMedia(mid, label, ssrc1, ssrc2, direction, setup = 'actpass') {
  let result = `m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 102 121 127 120 125 107 108 109 124 119 123 118 114 115 116
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:nDB3
a=ice-pwd:tfQdK4j3DmPDkQ1FqrhmpfIc
a=ice-options:trickle
a=fingerprint:sha-256 DA:76:6B:34:F3:B4:57:94:51:FA:77:4D:4F:8E:B1:E9:BF:C0:FA:35:36:60:95:81:61:91:1D:BB:71:D6:F5:49
a=setup:${setup}
a=mid:${mid}
a=extmap:1 urn:ietf:params:rtp-hdrext:toffset
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 urn:3gpp:video-orientation
a=extmap:4 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:5 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
a=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/video-content-type
a=extmap:7 http://www.webrtc.org/experiments/rtp-hdrext/video-timing
a=extmap:8 http://www.webrtc.org/experiments/rtp-hdrext/color-space
a=extmap:9 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:10 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:11 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=${direction}`;
  if (label) {
    result += `
a=msid:${label} ${label}_video`;
  }
  result += `
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:96 VP8/90000
a=rtcp-fb:96 goog-remb
a=rtcp-fb:96 transport-cc
a=rtcp-fb:96 ccm fir
a=rtcp-fb:96 nack
a=rtcp-fb:96 nack pli
a=rtpmap:97 rtx/90000
a=fmtp:97 apt=96
a=rtpmap:98 VP9/90000
a=rtcp-fb:98 goog-remb
a=rtcp-fb:98 transport-cc
a=rtcp-fb:98 ccm fir
a=rtcp-fb:98 nack
a=rtcp-fb:98 nack pli
a=fmtp:98 profile-id=0
a=rtpmap:99 rtx/90000
a=fmtp:99 apt=98
a=rtpmap:100 VP9/90000
a=rtcp-fb:100 goog-remb
a=rtcp-fb:100 transport-cc
a=rtcp-fb:100 ccm fir
a=rtcp-fb:100 nack
a=rtcp-fb:100 nack pli
a=fmtp:100 profile-id=2
a=rtpmap:101 rtx/90000
a=fmtp:101 apt=100
a=rtpmap:102 H264/90000
a=rtcp-fb:102 goog-remb
a=rtcp-fb:102 transport-cc
a=rtcp-fb:102 ccm fir
a=rtcp-fb:102 nack
a=rtcp-fb:102 nack pli
a=fmtp:102 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42001f
a=rtpmap:121 rtx/90000
a=fmtp:121 apt=102
a=rtpmap:127 H264/90000
a=rtcp-fb:127 goog-remb
a=rtcp-fb:127 transport-cc
a=rtcp-fb:127 ccm fir
a=rtcp-fb:127 nack
a=rtcp-fb:127 nack pli
a=fmtp:127 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42001f
a=rtpmap:120 rtx/90000
a=fmtp:120 apt=127
a=rtpmap:125 H264/90000
a=rtcp-fb:125 goog-remb
a=rtcp-fb:125 transport-cc
a=rtcp-fb:125 ccm fir
a=rtcp-fb:125 nack
a=rtcp-fb:125 nack pli
a=fmtp:125 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
a=rtpmap:107 rtx/90000
a=fmtp:107 apt=125
a=rtpmap:108 H264/90000
a=rtcp-fb:108 goog-remb
a=rtcp-fb:108 transport-cc
a=rtcp-fb:108 ccm fir
a=rtcp-fb:108 nack
a=rtcp-fb:108 nack pli
a=fmtp:108 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f
a=rtpmap:109 rtx/90000
a=fmtp:109 apt=108
a=rtpmap:124 H264/90000
a=rtcp-fb:124 goog-remb
a=rtcp-fb:124 transport-cc
a=rtcp-fb:124 ccm fir
a=rtcp-fb:124 nack
a=rtcp-fb:124 nack pli
a=fmtp:124 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=4d0032
a=rtpmap:119 rtx/90000
a=fmtp:119 apt=124
a=rtpmap:123 H264/90000
a=rtcp-fb:123 goog-remb
a=rtcp-fb:123 transport-cc
a=rtcp-fb:123 ccm fir
a=rtcp-fb:123 nack
a=rtcp-fb:123 nack pli
a=fmtp:123 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=640032
a=rtpmap:118 rtx/90000
a=fmtp:118 apt=123
a=rtpmap:114 red/90000
a=rtpmap:115 rtx/90000
a=fmtp:115 apt=114
a=rtpmap:116 ulpfec/90000`;
  if (label && ssrc1 && ssrc2) {
    result = `${result}
a=ssrc-group:FID ${ssrc1} ${ssrc2}
a=ssrc:${ssrc1} cname:${label}
a=ssrc:${ssrc1} msid:${label} ${label}_video
a=ssrc:${ssrc1} mslabel:${label}
a=ssrc:${ssrc1} label:${label}_video
a=ssrc:${ssrc2} cname:${label}
a=ssrc:${ssrc2} msid:${label} ${label}_video
a=ssrc:${ssrc2} mslabel:${label}
a=ssrc:${ssrc2} label:${label}_video`;
  }
  return result;
}

function getChromeAudioMedia(mid, label, ssrc1, direction, setup = 'actpass') {
  let result = `m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 126
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:nDB3
a=ice-pwd:tfQdK4j3DmPDkQ1FqrhmpfIc
a=ice-options:trickle
a=fingerprint:sha-256 5D:CA:35:43:7F:7F:1A:D6:1F:44:F7:56:D0:28:E6:81:24:41:70:1C:28:C4:39:9B:4C:A5:87:E2:0A:09:75:45
a=setup:${setup}
a=mid:${mid}
a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:2 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id
a=extmap:6 urn:ietf:params:rtp-hdrext:sdes:repaired-rtp-stream-id
a=${direction}`;
  if (label) {
    result += `
a=msid:${label} ${label}_audio`;
  }
  result += `
a=rtcp-mux
a=rtpmap:111 opus/48000/2
a=rtcp-fb:111 transport-cc
a=fmtp:111 minptime=10;useinbandfec=1
a=rtpmap:103 ISAC/16000
a=rtpmap:104 ISAC/32000
a=rtpmap:9 G722/8000
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:106 CN/32000
a=rtpmap:105 CN/16000
a=rtpmap:13 CN/8000
a=rtpmap:110 telephone-event/48000
a=rtpmap:112 telephone-event/32000
a=rtpmap:113 telephone-event/16000
a=rtpmap:126 telephone-event/8000`;
  if (label && ssrc1) {
    result = `${result}
a=ssrc:${ssrc1} cname:${label}
a=ssrc:${ssrc1} msid:${label} ${label}_audio
a=ssrc:${ssrc1} mslabel:${label}
a=ssrc:${ssrc1} label:${label}_audio`;
  }
  return result;
}

function getChromeBundleGroups(medias) {
  let result = '';
  medias.forEach((media) => {
    result = `${result} ${media.mid}`;
  });
  return result.trim();
}

function getChromeSdpMedias(medias) {
  let result = '';
  medias.forEach((media) => {
    if (media.kind === 'video') {
      result = `${result}\n${getChromeVideoMedia(media.mid, media.label, media.ssrc1, media.ssrc2, media.direction, media.setup)}`;
    } else {
      result = `${result}\n${getChromeAudioMedia(media.mid, media.label, media.ssrc1, media.direction, media.setup)}`;
    }
  });
  return result;
}

// Argument medias is an ordered list of medias, being media:
// [ { mid, label, ssrc1, ssrc2, kind }]
exports.getChromePublisherSdp = medias => `v=0
o=- 2288640127182668253 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE ${getChromeBundleGroups(medias)}
a=msid-semantic: WMS${getChromeSdpMedias(medias)}`;
