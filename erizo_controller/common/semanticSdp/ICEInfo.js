function randomIntInc(low, high) {
  const range = (high - low) + 1;
  const random = Math.random() * range;
  return Math.floor(random + low);
}

function randomBytes(size) {
  const numbers = new Uint8Array(size);

  for (let i = 0; i < numbers.length; i += 1) {
    numbers[i] = randomIntInc(0, 255);
  }
  return numbers;
}

function buf2hex(buffer) {
  return Array.prototype.map.call(new Uint8Array(buffer), (x) => {
    const hexValue = x.toString(16);
    return `00${hexValue}`.slice(-2);
  }).join('');
}

class ICEInfo {
  constructor(ufrag, pwd, opts) {
    this.ufrag = ufrag;
    this.pwd = pwd;
    this.opts = opts;
    this.lite = false;
    this.endOfCandidates = false;
  }

  clone() {
    const cloned = new ICEInfo(this.ufrag, this.pwd, this.opts);
    cloned.setLite(this.lite);
    cloned.setEndOfCandidates(this.endOfCandidates);
    return cloned;
  }

  plain() {
    const plain = {
      ufrag: this.ufrag,
      pwd: this.pwd,
    };
    if (this.lite) plain.lite = this.lite;
    if (this.endOfCandidates) plain.endOfCandidates = this.endOfCandidates;
    return plain;
  }

  getUfrag() {
    return this.ufrag;
  }

  getPwd() {
    return this.pwd;
  }

  isLite() {
    return this.lite;
  }

  getOpts() {
    return this.opts;
  }

  setLite(lite) {
    this.lite = lite;
  }

  isEndOfCandidates() {
    return this.endOfCandidates;
  }

  setEndOfCandidates(endOfCandidates) {
    this.endOfCandidates = endOfCandidates;
  }
}

ICEInfo.generate = () => {
  const info = new ICEInfo();
  const frag = randomBytes(8);
  const pwd = randomBytes(24);
  info.ufrag = buf2hex(frag);
  info.pwd = buf2hex(pwd);
  return info;
};

module.exports = ICEInfo;
