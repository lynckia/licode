const SDPTransform = require('sdp-transform');  // eslint-disable-line
const CandidateInfo = require('./CandidateInfo');
const CodecInfo = require('./CodecInfo');
const DTLSInfo = require('./DTLSInfo');
const ICEInfo = require('./ICEInfo');
const MediaInfo = require('./MediaInfo');
const Setup = require('./Setup');
const Direction = require('./Direction');
const DirectionWay = require('./DirectionWay');
const SourceGroupInfo = require('./SourceGroupInfo');
const SourceInfo = require('./SourceInfo');
const StreamInfo = require('./StreamInfo');
const TrackInfo = require('./TrackInfo');
const TrackEncodingInfo = require('./TrackEncodingInfo');
const SimulcastInfo = require('./SimulcastInfo');
const SimulcastStreamInfo = require('./SimulcastStreamInfo');
const RIDInfo = require('./RIDInfo');

class SDPInfo {
  constructor(version) {
    this.version = version || 1;
    this.name = 'sdp-semantic';
    this.streams = new Map();
    this.medias = [];
    this.candidates = [];
    this.connection = null;
    this.ice = null;
    this.dtls = null;
  }

  clone() {
    const cloned = new SDPInfo(this.version);
    cloned.name = this.name;
    cloned.setConnection(this.connection);
    this.medias.forEach((media) => {
      cloned.addMedia(media.clone());
    });
    this.streams.forEach((stream) => {
      cloned.addStream(stream.clone());
    });
    this.candidates.forEach((candidate) => {
      cloned.addCandidate(candidate.clone());
    });
    cloned.setICE(this.ice.clone());
    cloned.setDTLS(this.dtls.clone());
    return cloned;
  }

  plain() {
    const plain = {
      version: this.version,
      name: this.name,
      streams: [],
      medias: [],
      candidates: [],
      connection: this.connection,
    };
    this.medias.forEach((media) => {
      plain.medias.push(media.plain());
    });
    this.streams.forEach((stream) => {
      plain.streams.push(stream.plain());
    });
    this.candidates.forEach((candidate) => {
      plain.candidates.push(candidate.plain());
    });
    plain.ice = this.ice && this.ice.plain();
    plain.dtls = this.dtls && this.dtls.plain();
    return plain;
  }

  setVersion(version) {
    this.version = version;
  }

  setOrigin(origin) {
    this.origin = origin;
  }

  setName(name) {
    this.name = name;
  }

  getConnection() {
    return this.connection;
  }

  setConnection(connection) {
    this.connection = connection;
  }

  setTiming(timing) {
    this.timing = timing;
  }

  addMedia(media) {
    this.medias.push(media);
  }

  getMedia(type) {
    let result;
    this.medias.forEach((media) => {
      if (media.getType().toLowerCase() === type.toLowerCase()) {
        result = media;
      }
    });
    return result;
  }

  getMedias(type) {
    if (!type) {
      return this.medias;
    }
    const medias = [];
    this.medias.forEach((media) => {
      if (media.getType().toLowerCase() === type.toLowerCase()) {
        medias.push(media);
      }
    });
    return medias;
  }

  getMediaById(msid) {
    let result;
    this.medias.forEach((media) => {
      if (media.getId().toLowerCase() === msid.toLowerCase()) {
        result = media;
      }
    });
    return result;
  }

  getVersion() {
    return this.version;
  }

  getDTLS() {
    return this.dtls;
  }

  setDTLS(dtlsInfo) {
    this.dtls = dtlsInfo;
  }

  getICE() {
    return this.ice;
  }

  setICE(iceInfo) {
    this.ice = iceInfo;
  }

  addCandidate(candidate) {
    this.candidates.push(candidate);
  }

  addCandidates(candidates) {
    candidates.forEach((candidate) => {
      this.addCandidate(candidate);
    });
  }

  getCandidates() {
    return this.candidates;
  }

  getStream(id) {
    return this.streams.get(id);
  }

  getStreams() {
    return this.streams;
  }

  getFirstStream() {
    if (this.streams.values().length > 0) {
      return this.streams.values()[0];
    }
    return null;
  }

  addStream(stream) {
    this.streams.set(stream.getId(), stream);
  }

  removeStream(stream) {
    this.streams.delete(stream.getId());
  }

  toJSON() {
    const sdp = {
      version: 0,
      media: [],
    };

    sdp.version = this.version || 0;
    sdp.origin = this.origin || {
      username: '-',
      sessionId: (new Date()).getTime(),
      sessionVersion: this.version,
      netType: 'IN',
      ipVer: 4,
      address: '127.0.0.1',
    };

    sdp.name = this.name || 'semantic-sdp';

    sdp.connection = this.getConnection();
    sdp.timing = this.timing || { start: 0, stop: 0 };

    let ice = this.getICE();
    if (ice) {
      if (ice.isLite()) {
        sdp.icelite = 'ice-lite';
      }
      sdp.iceOptions = ice.getOpts();
      sdp.iceUfrag = ice.getUfrag();
      sdp.icePwd = ice.getPwd();
    }

    sdp.msidSemantic = this.msidSemantic || { semantic: 'WMS', token: '*' };
    sdp.groups = [];

    const bundle = { type: 'BUNDLE', mids: [] };
    let dtls = this.getDTLS();
    if (dtls) {
      sdp.fingerprint = {
        type: dtls.getHash(),
        hash: dtls.getFingerprint(),
      };

      sdp.setup = Setup.toString(dtls.getSetup());
    }

    this.medias.forEach((media) => {
      const md = {
        type: media.getType(),
        port: media.getPort(),
        protocol: 'UDP/TLS/RTP/SAVPF',
        fmtp: [],
        rtp: [],
        rtcpFb: [],
        ext: [],
        bandwidth: [],
        candidates: [],
        ssrcGroups: [],
        ssrcs: [],
        rids: [],
      };

      md.direction = Direction.toString(media.getDirection());

      md.rtcpMux = 'rtcp-mux';

      md.connection = media.getConnection();

      md.xGoogleFlag = media.getXGoogleFlag();

      md.mid = media.getId();

      bundle.mids.push(media.getId());
      md.rtcp = media.rtcp;

      if (media.getBitrate() > 0) {
        md.bandwidth.push({
          type: 'AS',
          limit: media.getBitrate(),
        });
      }

      const candidates = media.getCandidates();
      candidates.forEach((candidate) => {
        md.candidates.push({
          foundation: candidate.getFoundation(),
          component: candidate.getComponentId(),
          transport: candidate.getTransport(),
          priority: candidate.getPriority(),
          ip: candidate.getAddress(),
          port: candidate.getPort(),
          type: candidate.getType(),
          relAddr: candidate.getRelAddr(),
          relPort: candidate.getRelPort(),
          generation: candidate.getGeneration(),
        });
      });

      ice = media.getICE();
      if (ice) {
        if (ice.isLite()) {
          md.icelite = 'ice-lite';
        }
        md.iceOptions = ice.getOpts();
        md.iceUfrag = ice.getUfrag();
        md.icePwd = ice.getPwd();
        if (ice.isEndOfCandidates()) {
          md.endOfCandidates = ice.isEndOfCandidates();
        }
      }

      dtls = media.getDTLS();
      if (dtls) {
        md.fingerprint = {
          type: dtls.getHash(),
          hash: dtls.getFingerprint(),
        };

        md.setup = Setup.toString(dtls.getSetup());
      }

      if (media.setup) {
        md.setup = Setup.toString(media.setup);
      }

      media.getCodecs().forEach((codec) => {
        md.rtp.push({
          payload: codec.getType(),
          codec: codec.getCodec(),
          rate: codec.getRate(),
          encoding: codec.getEncoding(),
        });

        const params = codec.getParams();
        if (Object.keys(params).length > 0) {
          md.fmtp.push({
            payload: codec.getType(),
            config: Object.keys(params)
              .map(item => item + (params[item] ? `=${params[item]}` : ''))
              .join(';'),
          });
        }

        codec.getFeedback().forEach((rtcpFb) => {
          md.rtcpFb.push({
            payload: codec.getType(),
            type: rtcpFb.type,
            subtype: rtcpFb.subtype,
          });
        });

        if (codec.hasRTX()) {
          md.rtp.push({
            payload: codec.getRTX(),
            codec: 'rtx',
            rate: codec.getRate(),
            encoding: codec.getEncoding(),
          });
          md.fmtp.push({
            payload: codec.getRTX(),
            config: `apt=${codec.getType()}`,
          });
        }
      });
      const payloads = [];

      md.rtp.forEach((rtp) => {
        payloads.push(rtp.payload);
      });

      md.payloads = payloads.join(' ');

      media.getExtensions().forEach((uri, value) => {
        md.ext.push({
          value,
          uri,
        });
      });

      media.getRIDs().forEach((ridInfo) => {
        const rid = {
          id: ridInfo.getId(),
          direction: DirectionWay.toString(ridInfo.getDirection()),
          params: '',
        };
        if (ridInfo.getFormats().length) {
          rid.params = `pt=${ridInfo.getFormats().join(',')}`;
        }
        ridInfo.getParams().forEach((param, key) => {
          const prefix = rid.params.length ? ';' : '';
          rid.params += `${prefix}${key}=${param}`;
        });

        md.rids.push(rid);
      });

      const simulcast = media.getSimulcast();
      const simulcast03 = media.getSimulcast03();

      if (simulcast) {
        let index = 1;
        md.simulcast = {};
        const send = simulcast.getSimulcastStreams(DirectionWay.SEND);
        const recv = simulcast.getSimulcastStreams(DirectionWay.RECV);

        if (send && send.length) {
          let list = '';
          send.forEach((stream) => {
            let alternatives = '';
            stream.forEach((entry) => {
              alternatives +=
                (alternatives.length ? ',' : '') + (entry.isPaused() ? '~' : '') + entry.getId();
            });
            list += (list.length ? ';' : '') + alternatives;
          });
          md.simulcast[`dir${index}`] = 'send';
          md.simulcast[`list${index}`] = list;
          index += 1;
        }

        if (recv && recv.length) {
          let list = [];
          recv.forEach((stream) => {
            let alternatives = '';
            stream.forEach((entry) => {
              alternatives +=
                (alternatives.length ? ',' : '') + (entry.isPaused() ? '~' : '') + entry.getId();
            });
            list += (list.length ? ';' : '') + alternatives;
          });
          md.simulcast[`dir${index}`] = 'recv';
          md.simulcast[`list${index}`] = list;
          index += 1;
        }
      }

      if (simulcast03) {
        md.simulcast_03 = {
          value: simulcast03.getSimulcastPlainString(),
        };
      }

      sdp.media.push(md);
    });
    bundle.mids.sort();
    sdp.media.sort((m1, m2) => {
      if (m1.mid < m2.mid) return -1;
      if (m1.mid > m2.mid) return 1;
      return 0;
    });

    for (const stream of this.streams.values()) { // eslint-disable-line no-restricted-syntax
      for (const track of stream.getTracks().values()) { // eslint-disable-line no-restricted-syntax
        for (const md of sdp.media) { // eslint-disable-line no-restricted-syntax
          // Check if it is unified or plan B
          if (track.getMediaId()) {
            // Unified, check if it is bounded to an specific line
            if (track.getMediaId() === md.mid) {
              track.getSourceGroups().forEach((group) => {
                md.ssrcGroups.push({
                  semantics: group.getSemantics(),
                  ssrcs: group.getSSRCs().join(' '),
                });
              });

              track.getSSRCs().forEach((source) => {
                md.ssrcs.push({
                  id: source.ssrc,
                  attribute: 'cname',
                  value: source.getCName(),
                });
              });
              if (stream.getId() && track.getId()) {
                md.msid = `${stream.getId()} ${track.getId()}`;
              }
              break;
            }
          } else if (md.type.toLowerCase() === track.getMedia().toLowerCase()) {
            // Plan B
            track.getSourceGroups().forEach((group) => {
              md.ssrcGroups.push({
                semantics: group.getSemantics(),
                ssrcs: group.getSSRCs().join(' '),
              });
            });

            track.getSSRCs().forEach((source) => {
              md.ssrcs.push({
                id: source.ssrc,
                attribute: 'cname',
                value: source.getCName(),
              });
              if (source.getStreamId() && source.getTrackId()) {
                md.ssrcs.push({
                  id: source.ssrc,
                  attribute: 'msid',
                  value: `${source.getStreamId()} ${source.getTrackId()}`,
                });
              }
              if (source.getMSLabel()) {
                md.ssrcs.push({
                  id: source.ssrc,
                  attribute: 'mslabel',
                  value: source.getMSLabel(),
                });
              }
              if (source.getLabel()) {
                md.ssrcs.push({
                  id: source.ssrc,
                  attribute: 'label',
                  value: source.getLabel(),
                });
              }
            });
            break;
          }
        }
      }
    }

    bundle.mids = bundle.mids.join(' ');
    sdp.groups.push(bundle);

    return sdp;
  }

  toString() {
    const sdp = this.toJSON();
    return SDPTransform.write(sdp);
  }
}

function getFormats(mediaInfo, md) {
  const apts = new Map();

  md.rtp.forEach((fmt) => {
    const type = fmt.payload;
    const codec = fmt.codec;
    const rate = fmt.rate;
    const encoding = fmt.encoding;

    const params = {};
    const feedback = [];

    md.fmtp.forEach((fmtp) => {
      if (fmtp.payload === type) {
        const list = fmtp.config.split(';');
        list.forEach((entry) => {
          const param = entry.split('=');
          params[param[0].trim()] = (param[1] || '').trim();
        });
      }
    });
    if (md.rtcpFb) {
      md.rtcpFb.forEach((rtcpFb) => {
        if (rtcpFb.payload === type) {
          feedback.push({ type: rtcpFb.type, subtype: rtcpFb.subtype });
        }
      });
    }
    if (codec.toUpperCase() === 'RTX') {
      apts.set(parseInt(params.apt, 10), type);
    } else {
      mediaInfo.addCodec(new CodecInfo(codec, type, rate, encoding, params, feedback));
    }
  });

  apts.forEach((apt, id) => {
    const codecInfo = mediaInfo.getCodecForType(id);
    if (codecInfo) {
      codecInfo.setRTX(apt);
    }
  });
}

function getRIDs(mediaInfo, md) {
  const rids = md.rids;
  if (!rids) {
    return;
  }
  rids.forEach((rid) => {
    const ridInfo = new RIDInfo(rid.id, DirectionWay.byValue(rid.direction));
    let formats = [];
    const params = new Map();
    if (rid.params) {
      const list = SDPTransform.parseParams(rid.params);
      Object.keys(list).forEach((key) => {
        if (key === 'pt') {
          formats = list[key].split(',');
        } else {
          params.set(key, list[key]);
        }
      });
      ridInfo.setFormats(formats);
      ridInfo.setParams(params);
    }
    mediaInfo.addRID(ridInfo);
  });
}

function getSimulcastDir(index, md, simulcast) {
  const simulcastDir = md.simulcast[`dir${index}`];
  const simulcastList = md.simulcast[`list${index}`];
  if (simulcastDir) {
    const direction = DirectionWay.byValue(simulcastDir);
    const list = SDPTransform.parseSimulcastStreamList(simulcastList);
    list.forEach((stream) => {
      const alternatives = [];
      stream.forEach((entry) => {
        alternatives.push(new SimulcastStreamInfo(entry.scid, entry.paused));
      });
      simulcast.addSimulcastAlternativeStreams(direction, alternatives);
    });
  }
}

function getSimulcast3Dir(md, simulcast) {
  simulcast.setSimulcastPlainString(md.simulcast_03.value);
}

function getSimulcast(mediaInfo, md) {
  const encodings = [];
  if (md.simulcast) {
    const simulcast = new SimulcastInfo();
    getSimulcastDir('1', md, simulcast);
    getSimulcastDir('2', md, simulcast);

    simulcast.getSimulcastStreams(DirectionWay.SEND).forEach((streams) => {
      const alternatives = [];
      streams.forEach((stream) => {
        const encoding = new TrackEncodingInfo(stream.getId(), stream.isPaused());
        const ridInfo = mediaInfo.getRID(encoding.getId());
        if (ridInfo) {
          const formats = ridInfo.getFormats();
          formats.forEach((format) => {
            const codecInfo = mediaInfo.getCodecForType(format);
            if (codecInfo) {
              encoding.addCodec(codecInfo);
            }
          });
          encoding.setParams(ridInfo.getParams());
          alternatives.push(encoding);
        }
      });
      if (alternatives.length) {
        encodings.push(alternatives);
      }
    });

    mediaInfo.setSimulcast(simulcast);
  }
  if (md.simulcast_03) {
    const simulcast = new SimulcastInfo();
    getSimulcast3Dir(md, simulcast);
    mediaInfo.setSimulcast03(simulcast);
  }
  return encodings;
}

function getTracks(encodings, sdpInfo, md) {
  const sources = new Map();
  const media = md.type;
  if (md.ssrcs) {
    let track;
    let stream;
    let source;
    md.ssrcs.forEach((ssrcAttr) => {
      const ssrc = ssrcAttr.id;
      const key = ssrcAttr.attribute;
      const value = ssrcAttr.value;
      source = sources.get(ssrc);
      if (!source) {
        source = new SourceInfo(ssrc);
        sources.set(source.getSSRC(), source);
      }
      if (key.toLowerCase() === 'cname') {
        source.setCName(value);
      } else if (key.toLowerCase() === 'mslabel') {
        source.setMSLabel(value);
      } else if (key.toLowerCase() === 'label') {
        source.setLabel(value);
      } else if (key.toLowerCase() === 'msid') {
        const ids = value.split(' ');
        const streamId = ids[0];
        const trackId = ids[1];
        source.setStreamId(streamId);
        source.setTrackId(trackId);
        stream = sdpInfo.getStream(streamId);
        if (!stream) {
          stream = new StreamInfo(streamId);
          sdpInfo.addStream(stream);
        }
        track = stream.getTrack(trackId);
        if (!track) {
          track = new TrackInfo(media, trackId);
          track.setEncodings(encodings);
          stream.addTrack(track);
        }
        track.addSSRC(source);
      }
    });
  }

  if (md.msid) {
    const ids = md.msid.split(' ');
    const streamId = ids[0];
    const trackId = ids[1];

    let stream = sdpInfo.getStream(streamId);
    if (!stream) {
      stream = new StreamInfo(streamId);
      sdpInfo.addStream(stream);
    }
    let track = stream.getTrack(trackId);
    if (!track) {
      track = new TrackInfo(media, trackId);
      track.setMediaId(md.mid);
      track.setEncodings(encodings);
      stream.addTrack(track);
    }

    sources.forEach((key, ssrc) => {
      const source = sources.get(ssrc);
      if (!source.getStreamId()) {
        source.setStreamId(streamId);
        source.setTrackId(trackId);
        track.addSSRC(source);
      }
    });
  }

  if (md.ssrcGroups) {
    md.ssrcGroups.forEach((ssrcGroupAttr) => {
      const ssrcs = ssrcGroupAttr.ssrcs.split(' ');
      const group = new SourceGroupInfo(ssrcGroupAttr.semantics, ssrcs);
      const source = sources.get(parseInt(ssrcs[0], 10));
      sdpInfo
        .getStream(source.getStreamId())
        .getTrack(source.getTrackId())
        .addSourceGroup(group);
    });
  }
}

SDPInfo.processString = (string) => {
  const sdp = SDPTransform.parse(string);
  return SDPInfo.process(sdp);
};


SDPInfo.process = (sdp) => {
  const sdpInfo = new SDPInfo();

  sdpInfo.setVersion(sdp.version);
  sdpInfo.setTiming(sdp.timing);
  sdpInfo.setConnection(sdp.connection);
  sdpInfo.setOrigin(sdp.origin);
  sdpInfo.msidSemantic = sdp.msidSemantic;
  sdpInfo.name = sdp.name;

  let ufrag = sdp.iceUfrag;
  let pwd = sdp.icePwd;
  let iceOptions = sdp.iceOptions;
  if (ufrag || pwd || iceOptions) {
    sdpInfo.setICE(new ICEInfo(ufrag, pwd, iceOptions));
  }

  let fingerprintAttr = sdp.fingerprint;
  if (fingerprintAttr) {
    const remoteHash = fingerprintAttr.type;
    const remoteFingerprint = fingerprintAttr.hash;
    let setup = null;
    if (sdp.setup) {
      setup = Setup.byValue(sdp.setup);
    }

    sdpInfo.setDTLS(new DTLSInfo(setup, remoteHash, remoteFingerprint));
  }

  sdp.media.forEach((md) => {
    const media = md.type;
    const mid = md.mid;
    const port = md.port;
    const mediaInfo = new MediaInfo(mid, port, media);
    mediaInfo.setXGoogleFlag(md.xGoogleFlag);
    mediaInfo.rtcp = md.rtcp;
    mediaInfo.setConnection(md.connection);

    if (md.bandwidth && md.bandwidth.length > 0) {
      md.bandwidth.forEach((bandwidth) => {
        if (bandwidth.type === 'AS') {
          mediaInfo.setBitrate(bandwidth.limit);
        }
      });
    }

    ufrag = md.iceUfrag;
    pwd = md.icePwd;
    iceOptions = md.iceOptions;
    if (ufrag || pwd || iceOptions) {
      const thisIce = new ICEInfo(ufrag, pwd, iceOptions);
      if (md.endOfCandidates) {
        thisIce.setEndOfCandidates('end-of-candidates');
      }
      mediaInfo.setICE(thisIce);
    }

    fingerprintAttr = md.fingerprint;
    if (fingerprintAttr) {
      const remoteHash = fingerprintAttr.type;
      const remoteFingerprint = fingerprintAttr.hash;
      let setup = Setup.ACTPASS;
      if (md.setup) {
        setup = Setup.byValue(md.setup);
      }

      mediaInfo.setDTLS(new DTLSInfo(setup, remoteHash, remoteFingerprint));
    }

    if (md.setup) {
      mediaInfo.setSetup(Setup.byValue(md.setup));
    }

    let direction = Direction.SENDRECV;

    if (md.direction) {
      direction = Direction.byValue(md.direction.toUpperCase());
    }

    mediaInfo.setDirection(direction);

    const candidates = md.candidates;
    if (candidates) {
      candidates.forEach((candidate) => {
        mediaInfo.addCandidate(new CandidateInfo(candidate.foundation, candidate.component,
          candidate.transport, candidate.priority, candidate.ip, candidate.port, candidate.type,
          candidate.generation, candidate.relAddr, candidate.relPort));
      });
    }

    getFormats(mediaInfo, md);

    const extmaps = md.ext;
    if (extmaps) {
      extmaps.forEach((extmap) => {
        mediaInfo.addExtension(extmap.value, extmap.uri);
      });
    }

    getRIDs(mediaInfo, md);

    const encodings = getSimulcast(mediaInfo, md);

    getTracks(encodings, sdpInfo, md);

    sdpInfo.addMedia(mediaInfo);
  });
  return sdpInfo;
};

module.exports = SDPInfo;
