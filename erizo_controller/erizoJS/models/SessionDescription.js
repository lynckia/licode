/* global require */

// eslint-disable-next-line
const ConnectionDescription = require(`./../../../erizoAPI/build/Release/${global.config.erizo.addon}`)
  .ConnectionDescription;
const SdpInfo = require('./../../common/semanticSdp/SDPInfo');
const MediaInfo = require('./../../common/semanticSdp/MediaInfo');
const ICEInfo = require('./../../common/semanticSdp/ICEInfo');
const DTLSInfo = require('./../../common/semanticSdp/DTLSInfo');
const CodecInfo = require('./../../common/semanticSdp/CodecInfo');
const SourceInfo = require('./../../common/semanticSdp/SourceInfo');
const StreamInfo = require('./../../common/semanticSdp/StreamInfo');
const TrackInfo = require('./../../common/semanticSdp/TrackInfo');
const RIDInfo = require('./../../common/semanticSdp/RIDInfo');
const CandidateInfo = require('./../../common/semanticSdp/CandidateInfo');
const SimulcastInfo = require('./../../common/semanticSdp/SimulcastInfo');
const Direction = require('./../../common/semanticSdp/Direction');
const DirectionWay = require('./../../common/semanticSdp/DirectionWay');
const Setup = require('./../../common/semanticSdp/Setup');
const Helpers = require('./Helpers');

function addSsrc(sources, ssrc, sdp, media, msid = sdp.msidSemantic.token) {
  let source = sources.get(ssrc);
  if (!source) {
    source = new SourceInfo(ssrc);
    sources.set(ssrc, source);
  }
  source.setCName('erizo');
  source.setMSLabel(msid);
  source.setLabel(media.getId());
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
    track.mediaId = media.getId();
    stream.addTrack(track);
  }
  track.addSSRC(source);
}

function getMediaInfoFromDescription(info, sdp, mediaType, sdpMediaInfo) {
  let mid = sdpMediaInfo.mid;
  mid = mid !== undefined ? mid : info.getMediaId(mediaType);

  const media = new MediaInfo(mid, 9, mediaType);
  media.rtcp = { port: 1, netType: 'IN', ipVer: 4, address: '0.0.0.0' };
  media.setConnection({ version: 4, ip: '0.0.0.0' });
  let direction = info.getDirection(mediaType);
  if (sdpMediaInfo) {
    direction = sdpMediaInfo.direction;
  }
  media.setDirection(Direction.byValue(direction.toUpperCase()));

  const ice = info.getICECredentials(mediaType);
  if (ice) {
    const thisIceInfo = new ICEInfo(ice[0], ice[1]);
    thisIceInfo.setEndOfCandidates('end-of-candidates');
    media.setICE(thisIceInfo);
  }

  const fingerprint = info.getFingerprint(mediaType);
  if (fingerprint) {
    const setup = Setup.byValue(info.getDtlsRole(mediaType));
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
  if (codecs && codecs.length > 0) {
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
  let audioDirection = info.getDirection('audio');
  let videoDirection = info.getDirection('video');
  if (sdpMediaInfo && sdpMediaInfo.mid) {
    audioDirection = sdpMediaInfo.direction;
    videoDirection = sdpMediaInfo.direction;
  }

  if (sdpMediaInfo) {
    if (sdpMediaInfo.ssrc) {
      addSsrc(sources, sdpMediaInfo.ssrc, sdp, media, sdpMediaInfo.senderStreamId, true);
    }
  } else if (mediaType === 'audio' && audioDirection !== 'recvonly' && audioDirection !== 'inactive') {
    const audioSsrcMap = info.getAudioSsrcMap();
    Object.keys(audioSsrcMap).forEach((streamLabel) => {
      addSsrc(sources, audioSsrcMap[streamLabel], sdp, media, streamLabel);
    });
  } else if (mediaType === 'video') {
    media.setBitrate(info.getVideoBandwidth());

    if (videoDirection !== 'recvonly' && videoDirection !== 'inactive') {
      const videoSsrcMap = info.getVideoSsrcMap();
      Object.keys(videoSsrcMap).forEach((streamLabel) => {
        videoSsrcMap[streamLabel].forEach((ssrc) => {
          addSsrc(sources, ssrc, sdp, media, streamLabel);
        });
      });
    }
  }

  if (mediaType === 'video') {
    const rids = info.getRids();
    let isSimulcast = false;
    const simulcast = new SimulcastInfo();
    const ridsData = [];
    let ridDirection;
    Object.keys(rids).forEach((id) => {
      isSimulcast = true;
      ridDirection = rids[id];
      ridsData.push(id);
      const ridInfo = new RIDInfo(id, DirectionWay.byValue(rids[id]));
      media.addRID(ridInfo);
    });

    if (isSimulcast) {
      simulcast.setSimulcastPlainString(`${ridDirection} ${ridsData.join(';')}`);
      media.simulcast_03 = simulcast;
    }
    if (info.getXGoogleFlag() && info.getXGoogleFlag() !== '') {
      media.setXGoogleFlag(info.getXGoogleFlag());
    }
  }
  return media;
}

function candidateToString(cand) {
  let str = `candidate:${cand.foundation} ${cand.componentId} ${cand.transport}` +
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
  constructor(sdp, mediaConfiguration = undefined) {
    if (mediaConfiguration) {
      this.sdp = sdp;
      this.mediaConfiguration = mediaConfiguration;
      this.processSdp();
    } else {
      this.connectionDescription = sdp;
    }
  }

  getSdp(sessionVersion = 0) {
    if (this.sdp) {
      this.sdp.setOrigin({
        username: '-',
        sessionId: 0,
        sessionVersion,
        netType: 'IN',
        ipVer: 4,
        address: '127.0.0.1' });
      return this.sdp;
    }
    const info = this.connectionDescription;
    const sdp = new SdpInfo();
    sdp.setVersion(0);
    sdp.setTiming({ start: 0, stop: 0 });
    sdp.setOrigin({
      username: '-',
      sessionId: 0,
      sessionVersion,
      netType: 'IN',
      ipVer: 4,
      address: '127.0.0.1' });
    sdp.name = 'LicodeMCU';

    sdp.msidSemantic = { semantic: 'WMS', token: '*' };
    const mediaInfoMap = info.getMediaInfoMap();
    Object.keys(mediaInfoMap).forEach((mediaInfoId) => {
      const mediaInfo = mediaInfoMap[mediaInfoId];
      const media = getMediaInfoFromDescription(info, sdp, mediaInfo.kind, mediaInfo);
      sdp.addMedia(media);
    });
    this.sdp = sdp;

    return this.sdp;
  }

  static getStreamInfo(info, stream) {
    const streamId = stream.getId();
    let videoSsrcList = [];
    let simulcastVideoSsrcList;

    stream.getTracks().forEach((track) => {
      if (track.getMedia() === 'audio') {
        if (track.getSSRCs().length > 0) {
          info.setAudioSsrc(streamId, track.getSSRCs()[0].getSSRC());
        }
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

    videoSsrcList = simulcastVideoSsrcList || videoSsrcList;
    info.setVideoSsrcList(streamId, videoSsrcList);
  }

  getICECredentials() {
    if (this.connectionDescription instanceof ConnectionDescription) {
      return this.connectionDescription.getICECredentials();
    }
    return ['', ''];
  }

  processSdp() {
    const info = new ConnectionDescription(Helpers.getMediaConfiguration(this.mediaConfiguration));
    const sdp = this.sdp;

    info.setRtcpMux(true); // TODO

    info.setProfile('SAVPF');
    this.profile = 'SAVPF';

    // we use the same field for both audio and video
    if (sdp.medias && sdp.medias.length > 0) {
      sdp.medias.forEach((media) => {
        if (media.getType() === 'audio') {
          info.setAudioDirection(Direction.toString(media.getDirection()));
        } else {
          info.setVideoDirection(Direction.toString(media.getDirection()));
        }
      });
    }

    info.setBundle(true); // TODO

    const sdpDtls = sdp.getDTLS();
    if (sdpDtls) {
      info.setFingerprint(sdpDtls.getFingerprint());
      info.setDtlsRole(Setup.toString(sdpDtls.getSetup()));
    }

    sdp.medias.forEach((media) => {
      const mediaDtls = media.getDTLS();
      if (mediaDtls) {
        info.setFingerprint(mediaDtls.getFingerprint());
        info.setDtlsRole(Setup.toString(mediaDtls.getSetup()));
      }

      if (media.protocol === 'UDP/TLS/RTP/SAVPF') {
        info.setProfile('SAVPF');
        this.profole = 'SAVPF';
      } else {
        info.setProfile('AVPF');
        this.profole = 'AVPF';
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

      const ice = media.getICE();
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
          const feedback = rtcpFb.subtype ? `${rtcpFb.type} ${rtcpFb.subtype}` : rtcpFb.type;
          info.addFeedback(codec.getType(), feedback);
        });
      });

      if (media.getBitrate() > 0) {
        info.setVideoBandwidth(media.getBitrate());
      }

      media.getExtensions().forEach((uri, value) => {
        info.addExtension(value, uri, media.getType());
      });

      if (media.getXGoogleFlag() && media.getXGoogleFlag() !== '') {
        info.setXGoogleFlag(media.getXGoogleFlag());
      }
    });

    const ice = sdp.getICE();
    if (ice && ice.getUfrag()) {
      info.setICECredentials(ice.getUfrag(), ice.getPwd(), 'audio');
      info.setICECredentials(ice.getUfrag(), ice.getPwd(), 'video');
    }

    sdp.getStreams().forEach((stream) => {
      SessionDescription.getStreamInfo(info, stream);
    });
    sdp.getStreams().forEach((entry) => {
      entry.tracks.forEach((track) => {
        sdp.medias[track.mediaId].streamId = entry.id;
      });
    });
    sdp.getMedias().forEach((media) => {
      info.addMediaInfo(media.streamId, '', media.id, media.getDirectionString(), media.type, '');
    });

    info.postProcessInfo();

    this.connectionDescription = info;
  }
}
module.exports = SessionDescription;
