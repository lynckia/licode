'use strict';
const EventEmitter = require('events');

var guid = (function() {
  function s4() {
    return Math.floor((1 + Math.random()) * 0x10000)
               .toString(16)
               .substring(1);
  }
  return function() {
    return s4() + s4() + '-' + s4() + '-' + s4() + '-' +
           s4() + '-' + s4() + s4() + s4();
  };
})();

class ErizoList extends EventEmitter {
  constructor(prerunErizos, maxErizos) {
    super();
    this.prerunErizos = prerunErizos;
    this.maxErizos = maxErizos;
    this.currentPosition = 0;
    this.clear();
  }

  get idle() {
    return this.erizos.filter(erizo => erizo.idle);
  }

  get firstIdle() {
    return this.erizos.find(erizo => erizo.idle);
  }

  get running() {
    return this.erizos.filter(erizo => erizo.started);
  }

  get firstStopped() {
    return this.erizos.find(erizo => !erizo.started);
  }

  getErizo(internalId) {
    let erizo;
    if (internalId !== undefined && internalId !== null && internalId < this.maxErizos) {
      erizo = this.erizos[internalId];
      if (!erizo.started) {
        this.emit('launch-erizo', erizo);
      }
      return erizo;
    }

    erizo = this.firstIdle;
    const erizoId = erizo && erizo.id;
    if (!erizoId) {
      if (!this.areAllRunning()) {
        this.create();
        return this.getErizo();
      } else {
        erizo = this.erizos[this.currentPosition];
        this.currentPosition = (this.currentPosition + 1) % this.maxErizos;
      }
    }
    erizo.idle = false;
    return erizo;
  }

  forEach(task) {
    return this.erizos.forEach(task);
  }

  findById(id) {
    return this.erizos.find(erizo => erizo.id === id);
  }

  findByPosition(position) {
    return this.erizos.find(erizo => erizo.position === position);
  }

  areAllRunning() {
    return this.running.length >= this.maxErizos;
  }

  areMinimumRunning() {
    return this.idle.length >= this.prerunErizos;
  }

  needToStartMore() {
    return !this.areAllRunning() && !this.areMinimumRunning();
  }

  create() {
    const erizo = this.firstStopped;
    if (erizo) {
      erizo.id = guid();
      erizo.started = true;
      erizo.idle = true;
      this.emit('launch-erizo', erizo);
    }
    return erizo;
  }

  delete(id) {
    const erizo = this.findById(id);
    let process;
    if (erizo) {
      erizo.started = false;
      erizo.idle = false;
      erizo.id = undefined;
      process = erizo.process;
      erizo.process = undefined;
    }
    return process;
  }

  clear() {
    let pos = 0;
    this.erizos = (new Array(this.maxErizos)).fill(1).map(() => {
      return {
        started: false,
        idle: false,
        id: undefined,
        position: pos++,
        process: undefined,
      };
    });
  }

  fill() {
    if (this.needToStartMore()) {
      this.create();
      this.fill();
    }
  }
}

exports.ErizoList = ErizoList;
