const Setup = require('./Setup');

class DTLSInfo {
  constructor(setup, hash, fingerprint) {
    this.setup = setup;
    this.hash = hash;
    this.fingerprint = fingerprint;
  }

  clone() {
    return new DTLSInfo(this.setup, this.hash, this.fingerprint);
  }

  plain() {
    return {
      setup: Setup.toString(this.setup),
      hash: this.hash,
      fingerprint: this.fingerprint,
    };
  }

  getFingerprint() {
    return this.fingerprint;
  }

  getHash() {
    return this.hash;
  }

  getSetup() {
    return this.setup;
  }

  setSetup(setup) {
    this.setup = setup;
  }
}

module.exports = DTLSInfo;
