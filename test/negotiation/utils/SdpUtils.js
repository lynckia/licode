const expect = require('chai').expect;
const SdpInfo = require('../../../erizo_controller/common/semanticSdp/SDPInfo');

class SdpChecker {
  constructor(sdp) {
    this.sdp = sdp;
    if (this.sdp.type !== 'candidate') {
      this.sdpInfo = SdpInfo.processString(this.sdp.sdp);
    }
  }

  expectType(expectedType) {
    expect(this.sdp).to.have.property('type', expectedType);
  }

  expectVersion(expectedVersion) {
    expect(this.sdpInfo).to.have.property('origin');
    expect(this.sdpInfo.origin).to.have.property('sessionVersion', expectedVersion);
  }

  doNotExpectToIncludeCandidates() {
    expect(this.sdpInfo).to.have.property('medias').not.empty;
    expect(this.sdpInfo.medias[0]).to.have.property('candidates').empty;
  }

  expectToIncludeCandidates() {
    expect(this.sdpInfo).to.have.property('medias').not.empty;
    expect(this.sdpInfo.medias[0]).to.have.property('candidates').not.empty;
  }

  expectToHaveStream(stream) {
    expect(this.sdpInfo).to.have.property('streams').to.be.an.instanceof(Map);
    const streamInSdp = this.sdpInfo.streams.get(stream.label);
    if (!stream.addedToConnection) {
      expect(streamInSdp).to.not.exist;
      return
    }
    expect(streamInSdp).to.exist;
    expect(streamInSdp).to.have.property('tracks').to.be.an.instanceof(Map);
    expect(streamInSdp.tracks.size).to.be.equal(stream.audio + stream.video);
  }

  expectToHaveStreams(streams) {
    const streamIds = Object.keys(streams);
    for (const streamId of streamIds) {
      this.expectToHaveStream(streams[streamId]);
    }
  }
}

module.exports = SdpChecker;
