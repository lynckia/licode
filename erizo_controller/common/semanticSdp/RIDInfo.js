const DirectionWay = require('./DirectionWay');

class RIDInfo {
  constructor(id, direction) {
    this.id = id;
    this.direction = direction;
    this.formats = [];
    this.params = new Map();
  }

  clone() {
    const cloned = new RIDInfo(this.id, this.direction);
    cloned.setFormats(this.formats);
    cloned.setParams(this.params);
    return cloned;
  }

  plain() {
    const plain = {
      id: this.id,
      direction: DirectionWay.toString(this.direction),
      formats: this.formats,
      params: {},
    };
    this.params.forEach((param, id) => {
      plain.params[id] = param;
    });
    return plain;
  }

  getId() {
    return this.id;
  }

  getDirection() {
    return this.direction;
  }

  setDirection(direction) {
    this.direction = direction;
  }

  getFormats() {
    return this.formats;
  }

  setFormats(formats) {
    this.formats = [];
    formats.forEach((format) => {
      this.formats.push(parseInt(format, 10));
    });
  }

  getParams() {
    return this.params;
  }

  setParams(params) {
    this.params = new Map(params);
  }
}

module.exports = RIDInfo;
