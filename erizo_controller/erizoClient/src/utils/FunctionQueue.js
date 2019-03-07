class FunctionQueue {
  constructor() {
    this._enqueuing = false;
    this._queuedArgs = [];
  }

  protectFunction(protectedFunction) {
    return this._protectedFunction.bind(this, protectedFunction);
  }

  isEnqueueing() {
    return this._enqueuing;
  }

  startEnqueuing() {
    this._enqueuing = true;
  }

  stopEnqueuing() {
    this._enqueuing = false;
  }

  nextInQueue() {
    if (this._queuedArgs.length > 0) {
      const { protectedFunction, args } = this._queuedArgs.shift();
      protectedFunction(...args);
    }
  }

  dequeueAll() {
    const queuedArgs = this._queuedArgs;
    this._queuedArgs = [];
    queuedArgs.forEach(({ protectedFunction, args }) => {
      protectedFunction(...args);
    });
  }

  _protectedFunction(protectedFunction, ...args) {
    if (this.isEnqueueing()) {
      this._enqueue(protectedFunction, ...args);
      return;
    }
    protectedFunction(...args);
  }

  _enqueue(protectedFunction, ...args) {
    this._queuedArgs.push({ protectedFunction, args });
  }
}

export default FunctionQueue;
