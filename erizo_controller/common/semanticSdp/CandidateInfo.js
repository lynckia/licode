class CandidateInfo {
  constructor(foundation, componentId, transport, priority, address, port,
    type, generation, relAddr, relPort) {
    this.foundation = foundation;
    this.componentId = componentId;
    this.transport = transport;
    this.priority = priority;
    this.address = address;
    this.port = port;
    this.type = type;
    this.generation = generation;
    this.relAddr = relAddr;
    this.relPort = relPort;
  }

  clone() {
    return new CandidateInfo(this.foundation, this.componentId, this.transport, this.priority,
      this.address, this.port, this.type, this.generation, this.relAddr, this.relPort);
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
    if (this.relAddr) plain.relAddr = this.relAddr;
    if (this.relPort) plain.relPort = this.relPort;
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

  getGeneration() {
    return this.generation;
  }

  getRelAddr() {
    return this.relAddr;
  }

  getRelPort() {
    return this.relPort;
  }
}

module.exports = CandidateInfo;
