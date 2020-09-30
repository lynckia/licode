// const logger = require('./../../common/logger').logger;

// const log = logger.getLogger('NodeManager');

class NodeManager {
  constructor() {
    // streamId: Publisher
    this.publisherNodes = new Map();
  }

  addPublisherNode(streamId, publisherNode) {
    this.publisherNodes.set(streamId, publisherNode);
  }

  removePublisherNode(id) {
    return this.publisherNodes.delete(id);
  }

  forEachPublisherNode(doSomething) {
    this.publisherNodes.forEach((publisherNode) => {
      doSomething(publisherNode);
    });
  }

  getPublisherNodeById(id) {
    return this.publisherNodes.get(id);
  }

  getPublisherNodesByClientId(clientId) {
    const nodes = this.publisherNodes.values();
    const publisherNodes = Array.from(nodes).filter(node => node.clientId === clientId);
    return publisherNodes;
  }

  hasPublisherNode(id) {
    return this.publisherNodes.has(id);
  }

  getPublisherCount() {
    return this.publisherNodes.size;
  }
}

exports.NodeManager = NodeManager;

