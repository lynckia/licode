class CandidateInfo {
  constructor(foundation, componentId, transport, priority, address, port,
    type, tcptype, generation, raddr, rport) {
    this.foundation = foundation;
    this.componentId = componentId;
    this.transport = transport;
    this.priority = priority;
    this.address = address;
    this.port = port;
    this.type = type;
    this.tcptype = tcptype;
    this.generation = generation;
    this.raddr = raddr;
    this.rport = rport;
  }

  clone() {
    return new CandidateInfo(this.foundation, this.componentId, this.transport, this.priority,
      this.address, this.port, this.type, this.tcptype,
      this.generation, this.raddr, this.rport);
  }

  plain() {
    const plain = {
      foundation: this.foundation,
      componentId: this.componentId,
      transport: this.transport,
      priority: this.priority,
      address: this.address,
      port: this.port,
      type: this.type,
      generation: this.generation,
    };
    if (this.tcptype) plain.tcptype = this.tcptype;
    if (this.raddr) plain.raddr = this.raddr;
    if (this.rport) plain.rport = this.rport;
    return plain;
  }

  getFoundation() {
    return this.foundation;
  }

  getComponentId() {
    return this.componentId;
  }

  getTransport() {
    return this.transport;
  }

  getPriority() {
    return this.priority;
  }

  getAddress() {
    return this.address;
  }

  getPort() {
    return this.port;
  }

  getType() {
    return this.type;
  }

  getTcpType() {
    return this.tcptype;
  }

  getGeneration() {
    return this.generation;
  }

  getRelAddr() {
    return this.raddr;
  }

  getRelPort() {
    return this.rport;
  }
}

module.exports = CandidateInfo;
