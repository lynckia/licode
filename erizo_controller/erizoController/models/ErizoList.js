
const EventEmitter = require('events').EventEmitter;

const MAX_ERIZOS_PER_ROOM = 100;

class ErizoList extends EventEmitter {
  constructor(maxErizos = MAX_ERIZOS_PER_ROOM) {
    super();
    if (maxErizos > 0) {
      this.maxErizos = maxErizos;
    } else {
      this.maxErizos = MAX_ERIZOS_PER_ROOM;
    }
    this.erizos = new Array(maxErizos);
    this.erizos.fill(1);
    this.erizos = this.erizos.map(() => ({
      pending: false,
      erizoId: undefined,
      agentId: undefined,
      erizoIdForAgent: undefined,
      kaCount: 0,
    }));
  }

  findById(erizoId) {
    return this.erizos.find(erizo => erizo.erizoId === erizoId);
  }

  onErizoReceived(position, callback) {
    this.once(this.getInternalPosition(position), callback);
  }

  getInternalPosition(position) {
    return position % this.maxErizos;
  }

  isPending(position) {
    return this.erizos[this.getInternalPosition(position)].pending;
  }

  get(position) {
    return this.erizos[this.getInternalPosition(position)];
  }

  forEachUniqueErizo(task) {
    const uniqueIds = new Set();
    this.erizos.forEach((erizo) => {
      if (erizo.erizoId && !uniqueIds.has(erizo.erizoId)) {
        uniqueIds.add(erizo.erizoId);
        task(erizo);
      }
    });
  }

  deleteById(erizoId) {
    const erizo = this.findById(erizoId);
    erizo.pending = false;
    erizo.erizoId = undefined;
    erizo.agentId = undefined;
    erizo.erizoIdForAgent = undefined;
    erizo.kaCount = 0;
  }

  markAsPending(position) {
    this.erizos[this.getInternalPosition(position)].pending = true;
  }

  set(position, erizoId, agentId, erizoIdForAgent) {
    const erizo = this.erizos[this.getInternalPosition(position)];
    erizo.pending = false;
    erizo.erizoId = erizoId;
    erizo.agentId = agentId;
    erizo.erizoIdForAgent = erizoIdForAgent;
    this.emit(this.getInternalPosition(position), erizo);
  }
}

exports.ErizoList = ErizoList;
