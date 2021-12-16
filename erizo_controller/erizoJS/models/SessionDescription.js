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

function addSsrc(sources, ssrc, semanticSdpInfo, media, msid = semanticSdpInfo.msidSemantic.token) {
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
  let stream = semanticSdpInfo.getStream(msid);
  if (!stream) {
    stream = new StreamInfo(msid);
    semanticSdpInfo.addStream(stream);
  }
  let track = stream.getTrack(media.getId());
  if (!track) {
    track = new TrackInfo(media.getType(), media.getId());
    track.mediaId = media.getId();
    stream.addTrack(track);
  }
  track.addSSRC(source);
}

function getMediaInfoFromDescription(connectionDescription, semanticSdpInfo, mediaType, sdpMediaInfo) {
  let mid = sdpMediaInfo.mid;
  mid = mid !== undefined ? mid : connectionDescription.getMediaId(mediaType);

  let direction = connectionDescription.getDirection(mediaType);
  if (sdpMediaInfo) {
    direction = sdpMediaInfo.direction;
  }

  const port = (sdpMediaInfo.stopped) ? 0 : 9;

  const media = new MediaInfo(mid, port, mediaType);
  media.rtcp = { port: 1, netType: 'IN', ipVer: 4, address: '0.0.0.0' };
  media.setConnection({ version: 4, ip: '0.0.0.0' });

  media.setDirection(Direction.byValue(direction.toUpperCase()));

  const ice = connectionDescription.getICECredentials(mediaType);
  if (ice) {
    const thisIceInfo = new ICEInfo(ice[0], ice[1]);
    thisIceInfo.setLite(connectionDescription.isIceLite());
    thisIceInfo.setEndOfCandidates('end-of-candidates');
    media.setICE(thisIceInfo);
  }

  const fingerprint = connectionDescription.getFingerprint(mediaType);
  const setupValue = (sdpMediaInfo.justAddedInOffer || sdpMediaInfo.stopped) ? 'actpass' : connectionDescription.getDtlsRole(mediaType);
  const setup = Setup.byValue(setupValue);
  media.setDTLS(new DTLSInfo(setup, 'sha-256', fingerprint));

  const candidates = connectionDescription.getCandidates();
  if (candidates) {
    candidates.forEach((candidate) => {
      media.addCandidate(new CandidateInfo(candidate.foundation, candidate.componentId,
        candidate.protocol, candidate.priority, candidate.hostIp, candidate.hostPort,
        candidate.hostType, 0, candidate.relayIp, candidate.relayPort));
    });
  }

  const apts = new Map();
  const codecs = connectionDescription.getCodecs(mediaType);
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

  const extmaps = connectionDescription.getExtensions(mediaType);
  if (extmaps) {
    Object.keys(extmaps).forEach((value) => {
      media.addExtension(value, extmaps[value]);
    });
  }

  const sources = new Map();
  let audioDirection = connectionDescription.getDirection('audio');
  let videoDirection = connectionDescription.getDirection('video');
  if (sdpMediaInfo && sdpMediaInfo.mid) {
    audioDirection = sdpMediaInfo.direction;
    videoDirection = sdpMediaInfo.direction;
  }

  if (sdpMediaInfo) {
    if (sdpMediaInfo.ssrc) {
      addSsrc(sources, sdpMediaInfo.ssrc, semanticSdpInfo, media, sdpMediaInfo.senderStreamId, true);
    }
  } else if (mediaType === 'audio' && audioDirection !== 'recvonly' && audioDirection !== 'inactive') {
    const audioSsrcMap = connectionDescription.getAudioSsrcMap();
    Object.keys(audioSsrcMap).forEach((streamLabel) => {
      addSsrc(sources, audioSsrcMap[streamLabel], semanticSdpInfo, media, streamLabel);
    });
  } else if (mediaType === 'video') {
    media.setBitrate(connectionDescription.getVideoBandwidth());

    if (videoDirection !== 'recvonly' && videoDirection !== 'inactive') {
      const videoSsrcMap = connectionDescription.getVideoSsrcMap();
      Object.keys(videoSsrcMap).forEach((streamLabel) => {
        videoSsrcMap[streamLabel].forEach((ssrc) => {
          addSsrc(sources, ssrc, semanticSdpInfo, media, streamLabel);
        });
      });
    }
  }

  if (mediaType === 'video' && port > 0) {
    const rids = connectionDescription.getRids();
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
    if (connectionDescription.getXGoogleFlag() && connectionDescription.getXGoogleFlag() !== '') {
      media.setXGoogleFlag(connectionDescription.getXGoogleFlag());
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
      this.semanticSdpInfo = sdp;
      this.mediaConfiguration = mediaConfiguration;
      this.getConnectionDescription();
    } else {
      this.connectionDescription = sdp;
    }
  }

  getSemanticSdpInfo(sessionVersion = 0) {
    if (this.semanticSdpInfo) {
      this.semanticSdpInfo.setOrigin({
        username: '-',
        sessionId: 0,
        sessionVersion,
        netType: 'IN',
        ipVer: 4,
        address: '127.0.0.1' });
      return this.semanticSdpInfo;
    }
    const connectionDescription = this.connectionDescription;
    const semanticSdpInfo = new SdpInfo();
    semanticSdpInfo.setVersion(0);
    semanticSdpInfo.setTiming({ start: 0, stop: 0 });
    semanticSdpInfo.setOrigin({
      username: '-',
      sessionId: 0,
      sessionVersion,
      netType: 'IN',
      ipVer: 4,
      address: '127.0.0.1' });
    semanticSdpInfo.name = 'LicodeMCU';

    semanticSdpInfo.msidSemantic = { semantic: 'WMS', token: '*' };
    const mediaInfos = connectionDescription.getMediaInfos();
    mediaInfos.forEach((mediaInfo) => {
      const media = getMediaInfoFromDescription(connectionDescription, semanticSdpInfo, mediaInfo.kind, mediaInfo);
      semanticSdpInfo.addMedia(media);
    });
    this.semanticSdpInfo = semanticSdpInfo;

    return this.semanticSdpInfo;
  }

  static getStreamInfo(connectionDescription, stream) {
    const streamId = stream.getId();
    let videoSsrcList = [];
    let simulcastVideoSsrcList;

    stream.getTracks().forEach((track) => {
      if (track.getMedia() === 'audio') {
        if (track.getSSRCs().length > 0) {
          connectionDescription.setAudioSsrc(streamId, track.getSSRCs()[0].getSSRC());
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
    connectionDescription.setVideoSsrcList(streamId, videoSsrcList);
  }

  getICECredentials() {
    if (this.connectionDescription instanceof ConnectionDescription) {
      return this.connectionDescription.getICECredentials();
    }
    return ['', ''];
  }

  getConnectionDescription() {
    const connectionDescription = new ConnectionDescription(Helpers.getMediaConfiguration(this.mediaConfiguration));
    const semanticSdpInfo = this.semanticSdpInfo;

    connectionDescription.setRtcpMux(true); // TODO

    connectionDescription.setProfile('SAVPF');
    this.profile = 'SAVPF';

    // we use the same field for both audio and video
    if (semanticSdpInfo.medias && semanticSdpInfo.medias.length > 0) {
      semanticSdpInfo.medias.forEach((media) => {
        if (media.getType() === 'audio') {
          connectionDescription.setAudioDirection(Direction.toString(media.getDirection()));
        } else {
          connectionDescription.setVideoDirection(Direction.toString(media.getDirection()));
        }
      });
    }

    connectionDescription.setBundle(true); // TODO

    const sdpDtls =semanticSdpInfo.getDTLS();
    if (sdpDtls) {
      connectionDescription.setFingerprint(sdpDtls.getFingerprint());
      connectionDescription.setDtlsRole(Setup.toString(sdpDtls.getSetup()));
    }

   semanticSdpInfo.medias.forEach((media) => {
      const mediaDtls = media.getDTLS();
      if (mediaDtls && mediaDtls.getFingerprint()) {
        connectionDescription.setFingerprint(mediaDtls.getFingerprint());
      }
      if (mediaDtls && mediaDtls.getSetup()) {
        connectionDescription.setDtlsRole(Setup.toString(mediaDtls.getSetup()));
      }

      if (media.protocol === 'UDP/TLS/RTP/SAVPF') {
        connectionDescription.setProfile('SAVPF');
        this.profile = 'SAVPF';
      } else {
        connectionDescription.setProfile('AVPF');
        this.profile = 'AVPF';
      }

      connectionDescription.addBundleTag(media.getId(), media.getType());

      const candidates = media.getCandidates();
      candidates.forEach((candidate) => {
        const candidateString = candidateToString(candidate);
        connectionDescription.addCandidate(media.getType(), candidate.getFoundation(), candidate.getComponentId(),
          candidate.getTransport(), candidate.getPriority(), candidate.getAddress(),
          candidate.getPort(), candidate.getType(), candidate.getRelAddr(), candidate.getRelPort(),
          candidateString);
      });

      const ice = media.getICE();
      if (ice && ice.getUfrag()) {
        connectionDescription.setICECredentials(ice.getUfrag(), ice.getPwd(), media.getType());
      }

      media.getRIDs().forEach((ridInfo) => {
        connectionDescription.addRid(ridInfo.getId(), DirectionWay.toString(ridInfo.getDirection()));
      });

      media.getCodecs().forEach((codec) => {
        connectionDescription.addPt(codec.getType(), codec.getCodec(), codec.getRate(), media.getType());

        const params = codec.getParams();
        Object.keys(params).forEach((option) => {
          connectionDescription.addParameter(codec.getType(), option, params[option]);
        });

        codec.getFeedback().forEach((rtcpFb) => {
          const feedback = rtcpFb.subtype ? `${rtcpFb.type} ${rtcpFb.subtype}` : rtcpFb.type;
          connectionDescription.addFeedback(codec.getType(), feedback);
        });
      });

      if (media.getBitrate() > 0) {
        connectionDescription.setVideoBandwidth(media.getBitrate());
      }

      media.getExtensions().forEach((uri, value) => {
        connectionDescription.addExtension(value, uri, media.getType());
      });

      if (media.getXGoogleFlag() && media.getXGoogleFlag() !== '') {
        connectionDescription.setXGoogleFlag(media.getXGoogleFlag());
      }
    });

    const ice =semanticSdpInfo.getICE();
    if (ice && ice.getUfrag()) {
      connectionDescription.setICECredentials(ice.getUfrag(), ice.getPwd(), 'audio');
      connectionDescription.setICECredentials(ice.getUfrag(), ice.getPwd(), 'video');
    }

   semanticSdpInfo.getStreams().forEach((stream) => {
      SessionDescription.getStreamInfo(connectionDescription, stream);
    });
   semanticSdpInfo.getMedias().forEach((media) => {
      connectionDescription.addMediaInfo(media.streamId ? media.streamId : '', '', media.id, media.getDirectionString(), media.type, '', media.port === 0);
    });

    connectionDescription.postProcessInfo();

    this.connectionDescription = connectionDescription;
  }
}
module.exports = SessionDescription;
