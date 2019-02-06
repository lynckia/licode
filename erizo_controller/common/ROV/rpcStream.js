const Duplex = require('stream').Duplex;

class RpcDuplexStream extends Duplex {
  constructor(options) {
    super(options);
    this.rpcCallbacks = [];
    this.resultString = '';
    this.requestTimeout = undefined;
  }

  _write(chunk, encoding, callback) {
    // The underlying source only deals with strings
    let resultString = '';
    if (Buffer.isBuffer(chunk)) {
      resultString = chunk.toString();
    }
    this._flush(resultString);
    callback();
  }

  _flush(resultString) {
    this.resultString += resultString;
    if (this.requestTimeout) {
      clearTimeout(this.requestTimeout);
    }
    this.requestTimeout = setTimeout(() => {
      const rpcCallback = this.rpcCallbacks.pop();
      if (rpcCallback) {
        rpcCallback('callback', this.resultString);
      }
      this.resultString = '';
    }, 50);
  }

  onData(message, rpcCallback) {
    this.rpcCallbacks.push(rpcCallback);
    this.push(Buffer.from(`${message} \n`, 'utf8'));
  }
  // eslint-disable-next-line 
  _read(size) {
  }
}

exports.RpcDuplexStream = RpcDuplexStream;
