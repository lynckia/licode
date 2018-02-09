/*global require*/
'use strict';
var ConnectionDescription = require('./../../../erizoAPI/build/Release/addon')
                                                          .ConnectionDescription;
var SdpInfo = require('./../../common/semanticSdp/SDPInfo');
var MediaInfo = require('./../../common/semanticSdp/MediaInfo');
var ICEInfo = require('./../../common/semanticSdp/ICEInfo');
var DTLSInfo = require('./../../common/semanticSdp/DTLSInfo');
var CodecInfo = require('./../../common/semanticSdp/CodecInfo');
var SourceInfo = require('./../../common/semanticSdp/SourceInfo');
var StreamInfo = require('./../../common/semanticSdp/StreamInfo');
var TrackInfo = require('./../../common/semanticSdp/TrackInfo');
var RIDInfo = require('./../../common/semanticSdp/RIDInfo');
var CandidateInfo = require('./../../common/semanticSdp/CandidateInfo');
var SimulcastInfo = require('./../../common/semanticSdp/SimulcastInfo');
var Direction = require('./../../common/semanticSdp/Direction');
var DirectionWay = require('./../../common/semanticSdp/DirectionWay');
var Setup = require('./../../common/semanticSdp/Setup');
var Helpers = require('./Helpers');

function getRandomArbitrary(min, max) {
  return Math.floor(Math.random() * (max - min) + min);
}

function generateRandom(length) {
  const alphanum = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz';
  const lastChar = alphanum.length - 1;
  let value = '';

  for (let i = 0; i < length; i += 1) {
    value += '' + alphanum[getRandomArbitrary(0, lastChar) % lastChar];
  }

  return value;
}

function addSsrc(sources, ssrc, sdp, media) {
  let source = sources.get(ssrc);
  const msid = sdp.msidSemantic.token;
  if (!source) {
    source = new SourceInfo(ssrc);
    sources.set(ssrc, source);
  }
  source.setCName('o/i14u9pJrxRKAsu');
  source.setMSLabel(msid);
  source.setLabel(msid);
  source.setStreamId(msid);
  source.setTrackId(media.getId());
  let stream = sdp.getStream(msid);
  if (!stream) {
    stream = new StreamInfo(msid);
    sdp.addStream(stream);
  }
  let track = stream.getTrack(media.getId());
  if (!track) {
    track = new TrackInfo(media.getType(), media.getId());
    stream.addTrack(track);
  }
  track.addSSRC(source);
}

function getMediaInfoFromDescription(info, sdp, mediaType) {
  let media = new MediaInfo(info.getMediaId(mediaType), 1, mediaType);
  media.rtcp = { port: 1, netType: 'IN', ipVer: 4, address: '0.0.0.0' };
  media.setConnection({ version: 4, ip: '0.0.0.0' });
  const direction = info.getDirection(mediaType);
  media.setDirection(Direction.byValue(direction.toUpperCase()));

  let ice = info.getICECredentials(mediaType);
  if (ice) {
    media.setICE(new ICEInfo(ice[0], ice[1]));
  }

  const fingerprint = info.getFingerprint(mediaType);
  if (fingerprint) {
    let setup = Setup.byValue(info.getDtlsRole(mediaType));
    media.setDTLS(new DTLSInfo(setup, 'sha-256', fingerprint));
  }

  const candidates = info.getCandidates();
  if (candidates) {
    candidates.forEach((candidate) => {
      media.addCandidate(new CandidateInfo(candidate.foundation, candidate.componentId,
        candidate.protocol, candidate.priority, candidate.hostIp, candidate.hostPort,
        candidate.hostType, 0, candidate.relayIp, candidate.relayPort));
    });
  }

  const apts = new Map();
  const codecs = info.getCodecs(mediaType);
  if (codecs) {
    codecs.forEach((codec) => {
      const type = codec.type;
      const codecName = codec.name;
      const rate = codec.rate;
      const encoding = codec.channels;

      let params = {};
      const feedback = [];

      if (codec.feedbacks) {
        codec.feedbacks.forEach((rtcpFb) => {
          const tokens = rtcpFb.split(' ');
          const fbType = tokens[0];
          const fbSubType = tokens[1];
          feedback.push({ type: fbType, subtype: fbSubType });
        });
      }

      if (codec.params) {
        params = codec.params;
      }

      if (codecName.toUpperCase() === 'RTX') {
        apts.set(parseInt(params.apt, 10), type);
      } else {
        media.addCodec(new CodecInfo(codecName, type, rate, encoding, params, feedback));
      }
    });
  }

  apts.forEach((apt, id) => {
    const codecInfo = media.getCodecForType(id);
    if (codecInfo) {
      codecInfo.setRTX(apt);
    }
  });

  const extmaps = info.getExtensions(mediaType);
  if (extmaps) {
    Object.keys(extmaps).forEach((value) => {
      media.addExtension(value, extmaps[value]);
    });
  }

  const sources = new Map();
  if (mediaType === 'audio') {
    addSsrc(sources, info.getAudioSsrc(), sdp, media);
  } else if (mediaType === 'video') {
    media.setBitrate(info.getVideoBandwidth());

    info.getVideoSsrcList().forEach((ssrc) => {
      addSsrc(sources, ssrc, sdp, media);
    });

    const rids = info.getRids();
    let isSimulcast = false;
    let simulcast = new SimulcastInfo();
    let ridsData = [];
    let direction;
    Object.keys(rids).forEach((id) => {
      isSimulcast = true;
      direction = rids[id];
      ridsData.push(id);
      const ridInfo = new RIDInfo(id, DirectionWay.byValue(rids[id]));
      media.addRID(ridInfo);
    });

    if (isSimulcast) {
      /*jshint camelcase: false */
      simulcast.setSimulcastPlainString(direction + ' rid=' + ridsData.join(';'));
      media.simulcast_03 = simulcast;
    }

  }
  return media;
}

function candidateToString(cand) {
  var str = `candidate:${cand.foundation} ${cand.componentId} ${cand.transport}` +
            ` ${cand.priority} ${cand.address} ${cand.port} typ ${cand.type}`;

  str += (cand.relAddr != null) ? ` raddr ${cand.relAddr} rport ${cand.relPort}` : '';

  str += (cand.tcptype != null) ? ` tcptype ${cand.tcptype}` : '';

  if (cand.generation != null) {
    str += ` generation ${cand.generation}`;
  }

  str += (cand['network-id'] != null) ? ` network-id ${cand['network-id']}` : '';
  str += (cand['network-cost'] != null) ? ` network-cost ${cand['network-cost']}` : '';
  return str;
}

class SessionDescription {
  constructor(sdp, mediaConfiguration) {
    if (mediaConfiguration) {
      this.sdp = sdp;
      this.mediaConfiguration = mediaConfiguration;
      this.processSdp();
    } else {
      this.connectionDescription = sdp;
    }
  }

  getSdp() {
    if (this.sdp) {
      return this.sdp;
    }
    const info = this.connectionDescription;
    const sdp = new SdpInfo();
    sdp.setVersion(0);
    sdp.setTiming({ start: 0, stop: 0 });
    sdp.setOrigin({
      username: '-',
      sessionId: 0,
      sessionVersion: 0,
      netType: 'IN',
      ipVer: 4,
      address: '127.0.0.1' });
    sdp.name = 'LicodeMCU';

    sdp.msidSemantic = { semantic: 'WMS', token: generateRandom(10) };

    if (info.getFirstMediaReceived() === 'audio') {
      if (info.hasAudio()) {
        const media = getMediaInfoFromDescription(info, sdp, 'audio');
        sdp.addMedia(media);
      }
      if (info.hasVideo()) {
        const media = getMediaInfoFromDescription(info, sdp, 'video');
        sdp.addMedia(media);
      }
    } else {
      if (info.hasVideo()) {
        const media = getMediaInfoFromDescription(info, sdp, 'video');
        sdp.addMedia(media);
      }
      if (info.hasAudio()) {
        const media = getMediaInfoFromDescription(info, sdp, 'audio');
        sdp.addMedia(media);
      }
    }


    this.sdp = sdp;

    return this.sdp;
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

    sdp.medias.forEach((media, index) => {
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
      if (index === 0) {
        info.setFirstMediaReceived(media.getType());
      }
      info.addBundleTag(media.getId(), media.getType());

      const candidates = media.getCandidates();
      candidates.forEach((candidate) => {
        const candidateString = candidateToString(candidate);
        info.addCandidate(media.getType(), candidate.getFoundation(), candidate.getComponentId(),
          candidate.getTransport(), candidate.getPriority(), candidate.getAddress(),
          candidate.getPort(), candidate.getType(), candidate.getRelAddr(), candidate.getRelPort(),
          candidateString);
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
