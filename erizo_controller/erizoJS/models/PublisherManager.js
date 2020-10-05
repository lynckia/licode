class PublisherManager {
  constructor() {
    // streamId: Publisher
    this.publisherNodes = new Map();
  }

  add(streamId, publisherNode) {
    this.publisherNodes.set(streamId, publisherNode);
  }

  remove(id) {
    return this.publisherNodes.delete(id);
  }

  forEach(doSomething) {
    this.publisherNodes.forEach((publisherNode) => {
      doSomething(publisherNode);
    });
  }

  getPublisherById(id) {
    return this.publisherNodes.get(id);
  }

  getPublishersByClientId(clientId) {
    const nodes = this.publisherNodes.values();
    const publisherNodes = Array.from(nodes).filter(node => node.clientId === clientId);
    return publisherNodes;
  }

  has(id) {
    return this.publisherNodes.has(id);
  }

  getPublisherCount() {
    return this.publisherNodes.size;
  }
}

exports.PublisherManager = PublisherManager;

