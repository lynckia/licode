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
    console.log('RpcDuplex: calling write', resultString, 'This is type of ', (typeof chunk));
    this._flush(resultString);
    callback();
  }

  _flush(resultString) {
    this.resultString += resultString;
    if (this.requestTimeout) {
      clearTimeout(this.requestTimeout);
    }
    this.requestTimeout = setTimeout(() => {
      console.log('CallingTimeout');
      const rpcCallback = this.rpcCallbacks.pop();
      if (rpcCallback) {
        rpcCallback('callback', this.resultString);
      }
      this.resultString = '';
    }, 50);
  }

  onData(message, rpcCallback) {
    console.log('CAlling push!!!!!!!!', message);
//    this.push(null);
    this.rpcCallbacks.push(rpcCallback);
    this.push(Buffer.from(message + ' \n', 'utf8'));
  }
  _read(size) {
    console.log(`RpcDuplex: Calling _read with size: ${size}, callbacks size: ${this.rpcCallbacks.length}`);
  }
}

exports.RpcDuplexStream = RpcDuplexStream;
