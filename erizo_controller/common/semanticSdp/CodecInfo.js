class CodecInfo {
  constructor(codec, type, rate, encoding, params, feedback) {
    this.codec = codec;
    this.type = type;
    this.rate = rate;
    this.encoding = encoding;
    this.params = params || {};
    this.feedback = feedback || [];
  }

  clone() {
    const cloned = new CodecInfo(this.codec, this.type, this.rate, this.encoding,
      this.params, this.feedback);
    if (this.rtx) {
      cloned.setRTX(this.rtx);
    }
    return cloned;
  }


  plain() {
    return {
      codec: this.codec,
      type: this.type,
      rate: this.rate,
      encoding: this.encoding,
      params: this.params,
      feedback: this.feedback,
    };
  }

  setRTX(rtx) {
    this.rtx = rtx;
  }

  getType() {
    return this.type;
  }

  setType(type) {
    this.type = type;
  }

  getCodec() {
    return this.codec;
  }

  getParams() {
    return this.params;
  }

  setParam(paramName, value) {
    this.params[paramName] = value;
  }

  hasRTX() {
    return this.rtx;
  }

  getRTX() {
    return this.rtx;
  }

  getRate() {
    return this.rate;
  }

  getEncoding() {
    return this.encoding;
  }

  getFeedback() {
    return this.feedback;
  }
}

CodecInfo.mapFromNames = (names, rtx) => {
  const codecs = new Map();

  let dyn = 96;
  names.forEach((nameWithUpperCases) => {
    let pt;
    const name = nameWithUpperCases.toLowerCase();
    if (name === 'pcmu') pt = 0;
    else if (name === 'pcma') pt = 8;
    else {
      dyn += 1;
      pt = dyn;
    }
    const codec = new CodecInfo(name, pt);
    if (rtx && name !== 'ulpfec' && name !== 'flexfec-03' && name !== 'red') {
      dyn += 1;
      codec.setRTX(dyn);
    }
    codecs.set(codec.getCodec().toLowerCase(), codec);
  });
  return codecs;
};

module.exports = CodecInfo;
