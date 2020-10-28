// The MIT License (MIT)
//
// Copyright (c) 2015-2016 Capriza Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software
// and associated documentation files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

class ReliableSocket {
  /**
   * socket.io guaranteed delivery socket wrapper.
   * if the socket gets disconnected at any point,
   * it's up to the application to set a new socket to continue
   * handling messages.
   * calling 'setSocket' causes all messages that have not received an ack to be sent again.
   * @constructor
   */
  constructor(socket, lastAcked) {
    this._pending = [];
    this._events = {};
    this._id = 0;
    this._enabled = true;
    this._onAckCB = this._onAck.bind(this);
    this._onReconnectCB = this._onReconnect.bind(this);
    this.setLastAcked(lastAcked);
    this.setSocket(socket);
  }

  /**
   * set the id
   * @param id
   */
  setId(id) {
    this._id = id >= 0 ? id : 0;
  }

  /**
   * return the id
   */
  id() {
    return this._id;
  }

  /**
   * set the last message id that an ack was sent for
   * @param lastAcked
   */
  setLastAcked(lastAcked) {
    this._lastAcked = lastAcked >= 0 ? lastAcked : -1;
  }

  /**
   * get the last acked message id
   */
  lastAcked() {
    return this._lastAcked;
  }

  /**
   * replace the underlying socket.io socket with a new socket. useful in case of a socket getting
   * disconnected and a new socket is used to continue with the communications
   * @param socket
   */
  setSocket(socket) {
    this._cleanup();
    this._socket = socket;

    if (this._socket) {
      // We setup all listeners again
      Object.keys(this._events).forEach((event) => {
        for (let i = 0; i < this._events[event].length; i += 1) {
          this._socket.on(event, this._events[event][i].wrapped);
        }
      });
      this._socket.on('reconnect', this._onReconnectCB);
      this._socket.on('socketgd_ack', this._onAckCB);

      this.sendPending();
    }
  }

  /**
   * send all pending messages that have not received an ack
   */
  sendPending() {
    // send all pending messages that haven't been acked yet
    this._pending.forEach((message) => {
      this._sendOnSocket(message);
    });
  }

  /**
   * clear out any pending messages
   */
  clearPending() {
    this._pending = [];
  }

  getNumberOfPending() {
    return this._pending.length;
  }

  /**
   * enable or disable sending message with gd.
   * if disabled, then messages will be sent without guaranteeing delivery
   * in case of socket disconnection/reconnection.
   */
  enable(enabled) {
    this._enabled = enabled;
  }

  /**
   * get the underlying socket
   */
  socket() {
    return this._socket;
  }

  /**
   * cleanup socket stuff
   * @private
   */
  _cleanup() {
    if (!this._socket) {
      return;
    }

    this._socket.removeListener('reconnect', this._onReconnectCB);
    this._socket.removeListener('socketgd_ack', this._onAckCB);
    Object.keys(this._events).forEach((event) => {
      for (let i = 0; i < this._events[event].length; i += 1) {
        this._socket.removeListener(event, this._events[event][i].wrapped);
      }
    });
  }

  /**
   * invoked when an ack arrives
   * @param ack
   * @private
   */
  _onAck(ack) {
    // got an ack for a message, remove all messages pending
    // an ack up to (and including) the acked message.
    while (this._pending.length > 0 && this._pending[0].id <= ack.id) {
      this._pending.shift();
    }
  }

  /**
   * invoked when an a reconnect event occurs on the underlying socket
   * @private
   */
  _onReconnect() {
    this.sendPending();
  }

  /**
   * send an ack for a message
   * @private
   */
  _sendAck(id) {
    if (!this._socket) {
      return undefined;
    }

    this._lastAcked = id;
    this._socket.emit('socketgd_ack', { id });
    return this._lastAcked;
  }

  /**
   * send a message on the underlying socket.io socket
   * @param message
   * @private
   */
  _sendOnSocket(msg) {
    const message = msg;
    if (this._enabled && message.id === undefined) {
      message.id = this._id;
      this._id += 1;
      message.gd = true;
      this._pending.push(message);
    }

    if (!this._socket) {
      return;
    }

    if (this._enabled) {
      switch (message.type) {
        case 'send':
          this._socket.send(`socketgd:${message.id}:${message.msg}`, message.ack);
          break;
        case 'emit':
          this._socket.emit(message.event, { socketgd: message.id, msg: message.msg }, message.ack);
          break;
        default:
          break;
      }
    } else {
      switch (message.type) {
        case 'send':
          this._socket.send(message.msg, message.ack);
          break;
        case 'emit':
          this._socket.emit(message.event, message.msg, message.ack);
          break;
        default:
          break;
      }
    }
  }

  /**
   * send a message with gd. this means that if an ack is not received
   * and a new connection is established (by
   * calling setSocket), the message will be sent again.
   * @param message
   * @param ack
   */
  send(msg, ack) {
    this._sendOnSocket({ type: 'send', msg, ack });
  }

  /**
   * emit an event with gd. this means that if an
   * ack is not received and a new connection is established (by
   * calling setSocket), the event will be emitted again.
   * @param event
   * @param message
   * @param ack
   */
  emit(event, msg, ack) {
    this._sendOnSocket({ type: 'emit', event, msg, ack });
  }

  /**
   * disconnect the socket
   */
  disconnect(close) {
    if (this._socket) {
      this._socket.disconnect(close);
    }
    this._cleanup();
    this._socket = null;
  }

  /**
   * disconnectSync the socket
   */
  disconnectSync() {
    if (this._socket) {
      this._socket.disconnectSync();
    }
    this._cleanup();
    this._socket = null;
  }

  /**
   * listen for events on the socket. this replaces
   * calling the 'on' method directly on the socket.io socket.
   * here we take care of acking messages.
   * @param event
   * @param cb
   */
  on(event, cb) {
    this._events[event] = this._events[event] || [];

    const cbData = {
      cb,
      wrapped: (data, ack) => {
        if (data && event === 'message') {
          // parse the message
          if (data.indexOf('socketgd:') !== 0) {
            cb(data, ack);
            return;
          }
          // get the id (skipping the socketgd prefix)
          const index = data.indexOf(':', 9);
          if (index === -1) {
            cb(data, ack);
            return;
          }

          const id = parseInt(data.substring(9, index), 10);
          if (id <= this._lastAcked) {
            // discard the message since it was already handled and acked
            return;
          }

          const message = data.substring(index + 1);
          // the callback must call the 'ack' function so we can send an ack for the message
          if (cb) {
            try {
              cb(message, ack);
            } catch (e) {
              // TODO(javier): print an error
            }
          }
          this._sendAck(id);
        } else if (data && typeof data === 'object' && data.socketgd !== undefined) {
          if (data.socketgd <= this._lastAcked) {
            // discard the message since it was already handled and acked
            return;
          }
          if (cb) {
            try {
              cb(data.msg, ack);
            } catch (e) {
              // TODO(javier): print an error
            }
          }
          this._sendAck(data.socketgd);
        } else {
          cb(data, ack);
        }
      },
    };

    this._events[event].push(cbData);

    if (this._socket) {
      this._socket.on(event, cbData.wrapped);
    }
  }

  /**
   * remove a previously set callback for the specified event
   */
  off(event, cb) {
    if (!this._events[event]) {
      return;
    }

    // find the callback to remove
    for (let i = 0; i < this._events[event].length; i += 1) {
      if (this._events[event][i].cb === cb) {
        if (this._socket) {
          this._socket.removeListener(event, this._events[event][i].wrapped);
        }
        this._events[event].splice(i, 1);
      }
    }
  }
}

module.exports = { ReliableSocket };

