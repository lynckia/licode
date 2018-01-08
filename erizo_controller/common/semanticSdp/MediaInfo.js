const SimulcastInfo = require('./SimulcastInfo');
const Direction = require('./Direction');
const DirectionWay = require('./DirectionWay');

class MediaInfo {
  constructor(id, port, type) {
    this.id = id;
    this.type = type;
    this.port = port;
    this.direction = Direction.SENDRECV;
    this.extensions = new Map();
    this.codecs = new Map();
    this.rids = new Map();
    this.simulcast = null;
    this.simulcast_03 = null;
    this.bitrate = 0;
    this.ice = null;
    this.dtls = null;
    this.connection = null;
    this.candidates = [];
  }

  clone() {
    const cloned = new MediaInfo(this.id, this.port, this.type);
    cloned.setDirection(this.direction);
    cloned.setBitrate(this.bitrate);
    cloned.setConnection(this.connection);
    this.codecs.forEach((codec) => {
      cloned.addCodec(codec.clone());
    });
    this.extensions.forEach((extension, id) => {
      cloned.addExtension(id, extension);
    });
    this.rids.forEach((rid, id) => {
      cloned.addRID(id, rid.clone());
    });
    if (this.simulcast) {
      cloned.setSimulcast(this.simulcast.clone());
    }
    if (this.xGoogleFlag) {
      cloned.setXGoogleFlag(this.xGoogleFlag);
    }
    if (this.ice) {
      cloned.setICE(this.ice.clone());
    }
    if (this.dtls) {
      cloned.setDTLS(this.dtls.clone());
    }
    this.candidates.forEach((candidate) => {
      cloned.addCandidate(candidate.clone());
    });
    if (this.setup) {
      cloned.setSetup(this.setup);
    }
    return cloned;
  }

  plain() {
    const plain = {
      id: this.id,
      port: this.port,
      type: this.type,
      connection: this.connection,
      direction: Direction.toString(this.direction),
      xGoogleFlag: this.xGoogleFlag,
      extensions: {},
      rids: [],
      codecs: [],
      candidates: [],
    };
    if (this.bitrate) {
      plain.bitrate = this.bitrate;
    }
    this.codecs.forEach((codec) => {
      plain.codecs.push(codec.plain());
    });
    this.extensions.forEach((extension) => {
      plain.extensions.push(extension.plain());
    });
    this.rids.forEach((rid) => {
      plain.rids.push(rid.plain());
    });
    if (this.simulcast) {
      plain.simulcast = this.simulcast.plain();
    }
    this.candidates.forEach((candidate) => {
      plain.candidates.push(candidate.plain());
    });
    plain.ice = this.ice && this.ice.plain();
    plain.dtls = this.dtls && this.dtls.plain();
    return plain;
  }

  getType() {
    return this.type;
  }

  getPort() {
    return this.port;
  }

  getId() {
    return this.id;
  }

  addExtension(id, name) {
    this.extensions.set(id, name);
  }

  addRID(ridInfo) {
    this.rids.set(ridInfo.getId(), ridInfo);
  }

  addCodec(codecInfo) {
    this.codecs.set(codecInfo.getType(), codecInfo);
  }

  getCodecForType(type) {
    return this.codecs.get(type);
  }

  getCodec(codec) {
    let result;
    this.codecs.forEach((info) => {
      if (info.getCodec().toLowerCase() === codec.toLowerCase()) {
        result = info;
      }
    });
    return result;
  }

  hasCodec(codec) {
    return this.getCodec(codec) !== null;
  }

  getCodecs() {
    return this.codecs;
  }

  getExtensions() {
    return this.extensions;
  }

  getRIDs() {
    return this.rids;
  }

  getRID(id) {
    return this.rids.get(id);
  }

  getBitrate() {
    return this.bitrate;
  }

  setBitrate(bitrate) {
    this.bitrate = bitrate;
  }

  getDirection() {
    return this.direction;
  }

  setDirection(direction) {
    this.direction = direction;
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

  setXGoogleFlag(value) {
    this.xGoogleFlag = value;
  }

  getXGoogleFlag() {
    return this.xGoogleFlag;
  }

  getConnection() {
    return this.connection;
  }

  setConnection(connection) {
    this.connection = connection;
  }

  answer(supported) {
    const answer = new MediaInfo(this.id, this.port, this.type);

    answer.setDirection(Direction.reverse(this.direction));

    if (supported.codecs) {
      this.codecs.forEach((codec) => {
        if (supported.codecs.has(codec.getCodec().toLowerCase())) {
          const cloned = supported.codecs.get(codec.getCodec().toLowerCase()).clone();
          cloned.setType(codec.getType());
          if (cloned.hasRTX()) {
            cloned.setRTX(codec.getRTX());
          }
          answer.addCodec(cloned);
        }
      });
    }

    this.extensions.forEach((extension, id) => {
      if (supported.extensions.has(id)) {
        answer.addExtension(id, extension);
      }
    });

    if (supported.simulcast && this.simulcast) {
      const simulcast = new SimulcastInfo();
      const sendEntries = this.simulcast.getSimulcastStreams(DirectionWay.SEND);
      if (sendEntries) {
        sendEntries.forEach((sendStreams) => {
          const alternatives = [];
          sendStreams.forEach((sendStream) => {
            alternatives.push(sendStream.clone());
          });
          simulcast.addSimulcastAlternativeStreams(DirectionWay.RECV, alternatives);
        });
      }

      const recvEntries = this.simulcast.getSimulcastStreams(DirectionWay.RECV);
      if (recvEntries) {
        recvEntries.forEach((recvStreams) => {
          const alternatives = [];
          recvStreams.forEach((recvStream) => {
            alternatives.push(recvStream.clone());
          });
          simulcast.addSimulcastAlternativeStreams(DirectionWay.SEND, alternatives);
        });
      }

      this.rids.forEach((rid) => {
        const reversed = rid.clone();
        reversed.setDirection(DirectionWay.reverse(rid.getDirection()));
        answer.addRID(reversed);
      });

      answer.setSimulcast(simulcast);
    }
    return answer;
  }

  getSimulcast() {
    return this.simulcast;
  }

  setSimulcast(simulcast) {
    this.simulcast = simulcast;
  }

  getSimulcast03() {
    return this.simulcast_03;
  }

  setSimulcast03(simulcast) {
    this.simulcast_03 = simulcast;
  }

  getSetup() {
    return this.setup;
  }

  setSetup(setup) {
    this.setup = setup;
  }
}

module.exports = MediaInfo;
