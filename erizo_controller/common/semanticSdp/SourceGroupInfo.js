class SourceGroupInfo {
  constructor(semantics, ssrcs) {
    this.semantics = semantics;
    this.ssrcs = [];
    ssrcs.forEach((ssrc) => {
      this.ssrcs.push(parseInt(ssrc, 10));
    });
  }

  clone() {
    return new SourceGroupInfo(this.semantics, this.ssrcs);
  }

  plain() {
    const plain = {
      semantics: this.semantics,
      ssrcs: [],
    };
    plain.ssrcs = this.ssrcs;
    return plain;
  }

  getSemantics() {
    return this.semantics;
  }

  getSSRCs() {
    return this.ssrcs;
  }
}

module.exports = SourceGroupInfo;
