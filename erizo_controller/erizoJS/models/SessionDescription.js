/*global require*/
'use strict';
var ConnectionDescription = require('./../../../erizoAPI/build/Release/addon')
                                                          .ConnectionDescription;
var Direction = require('./../../common/semanticSdp/Direction');
var DirectionWay = require('./../../common/semanticSdp/DirectionWay');
var Setup = require('./../../common/semanticSdp/Setup');
var Helpers = require('./Helpers');

class SessionDescription {
  constructor(sdp, mediaConfiguration) {
    this.sdp = sdp;
    this.mediaConfiguration = mediaConfiguration;
    this.processSdp();
  }

  processSdp() {
    const info = new ConnectionDescription(Helpers.getMediaConfiguration(this.mediaConfiguration));
    const sdp = this.sdp;
    let audio;
    let video;

    info.setRtcpMux(true);  // TODO

    // we use the same field for both audio and video
    if (sdp.medias && sdp.medias.length > 0) {
      info.setDirection(Direction.toString(sdp.medias[0].getDirection()));
    }

    info.setProfile('UDP/TLS/RTP/SAVPF'); // TODO

    info.setBundle(true);  // TODO

    const dtls = sdp.getDTLS();
    if (dtls) {
      info.setFingerprint(dtls.getFingerprint());
      info.setDtlsRole(Setup.toString(dtls.getSetup()));
    }

    sdp.medias.forEach((media) => {
      const dtls = media.getDTLS();
      if (dtls) {
        info.setFingerprint(dtls.getFingerprint());
        info.setDtlsRole(Setup.toString(dtls.getSetup()));
      }
      if (media.getType() === 'audio') {
        audio = media;
      } else if (media.getType() === 'video') {
        video = media;
      }
      info.addBundleTag(media.getId(), media.getType());

      const candidates = media.getCandidates();
      candidates.forEach((candidate) => {
        info.addCandidate(media.getType(), candidate.getFoundation(), candidate.getComponentId(),
          candidate.getTransport(), candidate.getPriority(), candidate.getAddress(),
          candidate.getPort(), candidate.getType(), candidate.getRelAddr(), candidate.getRelPort());
      });

      let ice = media.getICE();
      if (ice && ice.getUfrag()) {
        info.setICECredentials(ice.getUfrag(), ice.getPwd(), media.getType());
      }

      media.getRIDs().forEach((ridInfo) => {
        info.addRid(ridInfo.getId(), DirectionWay.toString(ridInfo.getDirection()));
      });

      media.getCodecs().forEach((codec) => {
        info.addPt(codec.getType(), codec.getCodec(), codec.getRate(), media.getType());

        const params = codec.getParams();
        Object.keys(params).forEach((option) => {
          info.addParameter(codec.getType(), option, params[option]);
        });

        codec.getFeedback().forEach((rtcpFb) => {
          const feedback = rtcpFb.subtype ? rtcpFb.type + ' ' + rtcpFb.subtype : rtcpFb.type;
          info.addFeedback(codec.getType(), feedback);
        });
      });

      if (media.getBitrate() > 0) {
        info.setVideoBandwidth(media.getBitrate());
      }

      media.getExtensions().forEach((uri, value) => {
        info.addExtension(value, uri, media.getType());
      });
    });
    info.setAudioAndVideo(audio !== undefined, video !== undefined);

    let ice = sdp.getICE();
    if (ice && ice.getUfrag()) {
      info.setICECredentials(ice.getUfrag(), ice.getPwd(), 'audio');
      info.setICECredentials(ice.getUfrag(), ice.getPwd(), 'video');
    }

    let videoSsrcList = [];
    let simulcastVideoSsrcList;
    sdp.getStreams().forEach((stream) => {
      stream.getTracks().forEach((track) => {
        if (track.getMedia() === 'audio') {
          info.setAudioSsrc(track.getSSRCs()[0].getSSRC());
        } else if (track.getMedia() === 'video') {
          track.getSSRCs().forEach((ssrc) => {
            videoSsrcList.push(ssrc.getSSRC());
          });
        }

        // Google's simulcast
        track.getSourceGroups().forEach((group) => {
          if (group.getSemantics().toUpperCase() === 'SIM') {
            simulcastVideoSsrcList = group.getSSRCs();
          }
        });
      });
    });

    videoSsrcList = simulcastVideoSsrcList || videoSsrcList;
    info.setVideoSsrcList(videoSsrcList);

    info.postProcessInfo();

    this.connectionDescription = info;
  }

}
module.exports = SessionDescription;
