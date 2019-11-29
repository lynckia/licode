const DirectionWay = require('./DirectionWay');

class SimulcastInfo {
  constructor() {
    this.send = [];
    this.recv = [];
    this.plainString = null;
  }

  clone() {
    const cloned = new SimulcastInfo();
    this.send.forEach((sendStreams) => {
      const alternatives = [];
      sendStreams.forEach((sendStream) => {
        alternatives.push(sendStream.clone());
      });
      cloned.addSimulcastAlternativeStreams(DirectionWay.SEND, alternatives);
    });
    this.recv.forEach((recvStreams) => {
      const alternatives = [];
      recvStreams.forEach((recvStream) => {
        alternatives.push(recvStream.clone());
      });
      cloned.addSimulcastAlternativeStreams(DirectionWay.RECV, alternatives);
    });
    return cloned;
  }

  plain() {
    const plain = {
      send: [],
      recv: [],
    };
    this.send.forEach((sendStreams) => {
      const alternatives = [];
      sendStreams.forEach((sendStream) => {
        alternatives.push(sendStream.plain());
      });
      plain.send.push(alternatives);
    });
    this.recv.forEach((recvStreams) => {
      const alternatives = [];
      recvStreams.forEach((recvStream) => {
        alternatives.push(recvStream.plain());
      });
      plain.recv.push(alternatives);
    });
    return plain;
  }

  addSimulcastAlternativeStreams(direction, streams) {
    if (direction === DirectionWay.SEND) {
      return this.send.push(streams);
    }
    return this.recv.push(streams);
  }

  addSimulcastStream(direction, stream) {
    if (direction === DirectionWay.SEND) {
      return this.send.push([stream]);
    }
    return this.recv.push([stream]);
  }

  getSimulcastStreams(direction) {
    if (direction === DirectionWay.SEND) {
      return this.send;
    }
    return this.recv;
  }

  setSimulcastPlainString(string) {
    this.plainString = string;
  }

  getSimulcastPlainString() {
    return this.plainString;
  }
}

module.exports = SimulcastInfo;
