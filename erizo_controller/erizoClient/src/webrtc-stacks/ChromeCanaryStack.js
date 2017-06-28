/* global L, window, RTCSessionDescription, webkitRTCPeerConnection, Erizo*/
this.Erizo = this.Erizo || {};

Erizo.ChromeCanaryStack = (specInput) => {
  const that = {};
  const WebkitRTCPeerConnection = webkitRTCPeerConnection;
  const spec = specInput;

  that.pcConfig = {
    iceServers: [],
  };

  that.con = { optional: [{ DtlsSrtpKeyAgreement: true }] };

  if (spec.stunServerUrl !== undefined) {
    that.pcConfig.iceServers.push({ url: spec.stunServerUrl });
  }

  if ((spec.turnServer || {}).url) {
    that.pcConfig.iceServers.push({ username: spec.turnServer.username,
      credential: spec.turnServer.password,
      url: spec.turnServer.url });
  }

  if (spec.audio === undefined || spec.nop2p) {
    spec.audio = true;
  }

  if (spec.video === undefined || spec.nop2p) {
    spec.video = true;
  }

  that.mediaConstraints = {
    mandatory: {
      OfferToReceiveVideo: spec.video,
      OfferToReceiveAudio: spec.audio,
    },
  };

  that.roapSessionId = 103;

  that.peerConnection = new WebkitRTCPeerConnection(that.pcConfig, that.con);

  that.peerConnection.onicecandidate = (event) => {
    L.Logger.debug('PeerConnection: ', spec.sessionId);
    if (!event.candidate) {
            // At the moment, we do not renegotiate when new candidates
            // show up after the more flag has been false once.
      L.Logger.debug(`State: ${that.peerConnection.iceGatheringState}`);

      if (that.ices === undefined) {
        that.ices = 0;
      }
      that.ices += 1;
      if (that.ices >= 1 && that.moreIceComing) {
        that.moreIceComing = false;
        that.markActionNeeded();
      }
    } else {
      that.iceCandidateCount += 1;
    }
  };

    // L.Logger.debug('Created webkitRTCPeerConnnection with config \"' +
    //                                  JSON.stringify(that.pcConfig) + '\".');

  const setMaxBW = (sdpInput) => {
    let r;
    let a;
    let sdp = sdpInput;
    if (spec.maxVideoBW) {
      a = sdp.match(/m=video.*\r\n/);
      if (a && (a.length > 0)) {
        r = `${a[0]}b=AS:${spec.maxVideoBW}\r\n`;
        sdp = sdp.replace(a[0], r);
      }
    }

    if (spec.maxAudioBW) {
      a = sdp.match(/m=audio.*\r\n/);
      if (a && (a.length > 0)) {
        r = `${a[0]}b=AS:${spec.maxAudioBW}\r\n`;
        sdp = sdp.replace(a[0], r);
      }
    }

    return sdp;
  };

    /**
     * This function processes signalling messages from the other side.
     * @param {string} msgstring JSON-formatted string containing a ROAP message.
     */
  that.processSignalingMessage = (msgstring) => {
        // Offer: Check for glare and resolve.
        // Answer/OK: Remove retransmit for the msg this is an answer to.
        // Send back "OK" if this was an Answer.
    L.Logger.debug(`Activity on conn ${that.sessionId}`);
    const msg = JSON.parse(msgstring);
    let sd;
    that.incomingMessage = msg;

    if (that.state === 'new') {
      if (msg.messageType === 'OFFER') {
                // Initial offer.
        sd = {
          sdp: msg.sdp,
          type: 'offer',
        };
        that.peerConnection.setRemoteDescription(new RTCSessionDescription(sd));

        that.state = 'offer-received';
                // Allow other stuff to happen, then reply.
        that.markActionNeeded();
      } else {
        that.error(`Illegal message for this state: ${msg.messageType
                           } in state ${that.state}`);
      }
    } else if (that.state === 'offer-sent') {
      if (msg.messageType === 'ANSWER') {
                // regExp = new RegExp(/m=video[\w\W]*\r\n/g);

                // exp = msg.sdp.match(regExp);
                // L.Logger.debug(exp);

                // msg.sdp = msg.sdp.replace(regExp, exp + "b=AS:100\r\n");

        sd = {
          sdp: msg.sdp,
          type: 'answer',
        };
        L.Logger.debug('Received ANSWER: ', sd.sdp);

        sd.sdp = setMaxBW(sd.sdp);

        that.peerConnection.setRemoteDescription(new RTCSessionDescription(sd));
        that.sendOK();
        that.state = 'established';
      } else if (msg.messageType === 'pr-answer') {
        sd = {
          sdp: msg.sdp,
          type: 'pr-answer',
        };
        that.peerConnection.setRemoteDescription(new RTCSessionDescription(sd));

                // No change to state, and no response.
      } else if (msg.messageType === 'offer') {
                // Glare processing.
        that.error('Not written yet');
      } else {
        that.error(`Illegal message for this state: ${msg.messageType
                           } in state ${that.state}`);
      }
    } else if (that.state === 'established') {
      if (msg.messageType === 'OFFER') {
                // Subsequent offer.
        sd = {
          sdp: msg.sdp,
          type: 'offer',
        };
        that.peerConnection.setRemoteDescription(new RTCSessionDescription(sd));

        that.state = 'offer-received';
                // Allow other stuff to happen, then reply.
        that.markActionNeeded();
      } else {
        that.error(`Illegal message for this state: ${msg.messageType
                           } in state ${that.state}`);
      }
    }
  };

    /**
     * Adds a stream - this causes signalling to happen, if needed.
     * @param {MediaStream} stream The outgoing MediaStream to add.
     */
  that.addStream = (stream) => {
    that.peerConnection.addStream(stream);
    that.markActionNeeded();
  };

    /**
     * Removes a stream.
     * @param {MediaStream} stream The MediaStream to remove.
     */
  that.removeStream = () => {
//        var i;
//        for (i = 0; i < that.peerConnection.localStreams.length; ++i) {
//            if (that.localStreams[i] === stream) {
//                that.localStreams[i] = null;
//            }
//        }
    that.markActionNeeded();
  };

    /**
     * Closes the connection.
     */
  that.close = () => {
    that.state = 'closed';
    that.peerConnection.close();
  };

    /**
     * Internal function: Mark that something happened.
     */
  that.markActionNeeded = () => {
    that.actionNeeded = true;
    that.doLater(() => {
      that.onstablestate();
    });
  };

    /**
     * Internal function: Do something later (not on this stack).
     * @param {function} what Callback to be executed later.
     */
  that.doLater = (what) => {
        // Post an event to myself so that I get called a while later.
        // (needs more JS/DOM info. Just call the processing function on a delay
        // for now.)
    window.setTimeout(what, 1);
  };

    /**
     * Internal function called when a stable state
     * is entered by the browser (to allow for multiple AddStream calls or
     * other interesting actions).
     * This function will generate an offer or answer, as needed, and send
     * to the remote party using our onsignalingmessage function.
     */
  that.onstablestate = () => {
    let mySDP;
    if (that.actionNeeded) {
      if (that.state === 'new' || that.state === 'established') {
                // See if the current offer is the same as what we already sent.
                // If not, no change is needed.

        that.peerConnection.createOffer((description) => {
          const sessionDescription = description;
          sessionDescription.sdp = setMaxBW(sessionDescription.sdp);
          L.Logger.debug('Changed', sessionDescription.sdp);

          const newOffer = sessionDescription.sdp;

          if (newOffer !== that.prevOffer) {
            that.peerConnection.setLocalDescription(sessionDescription);

            that.state = 'preparing-offer';
            that.markActionNeeded();
          } else {
            L.Logger.debug('Not sending a new offer');
          }
        }, null, that.mediaConstraints);
      } else if (that.state === 'preparing-offer') {
                // Don't do anything until we have the ICE candidates.
        if (that.moreIceComing) {
          return;
        }


                // Now able to send the offer we've already prepared.
        that.prevOffer = that.peerConnection.localDescription.sdp;
        L.Logger.debug(`Sending OFFER: ${that.prevOffer}`);
                // L.Logger.debug('Sent SDP is ' + that.prevOffer);
        that.sendMessage('OFFER', that.prevOffer);
                // Not done: Retransmission on non-response.
        that.state = 'offer-sent';
      } else if (that.state === 'offer-received') {
        that.peerConnection.createAnswer((sessionDescription) => {
          that.peerConnection.setLocalDescription(sessionDescription);
          that.state = 'offer-received-preparing-answer';

          if (!that.iceStarted) {
            const now = new Date();
            L.Logger.debug(`${now.getTime()}: Starting ICE in responder`);
            that.iceStarted = true;
          } else {
            that.markActionNeeded();
          }
        }, null, that.mediaConstraints);
      } else if (that.state === 'offer-received-preparing-answer') {
        if (that.moreIceComing) {
          return;
        }

        mySDP = that.peerConnection.localDescription.sdp;

        that.sendMessage('ANSWER', mySDP);
        that.state = 'established';
      } else {
        that.error(`Dazed and confused in state ${that.state}, stopping here`);
      }
      that.actionNeeded = false;
    }
  };

    /**
     * Internal function to send an "OK" message.
     */
  that.sendOK = () => {
    that.sendMessage('OK');
  };

    /**
     * Internal function to send a signalling message.
     * @param {string} operation What operation to signal.
     * @param {string} sdp SDP message body.
     */
  that.sendMessage = (operation, sdp) => {
    const roapMessage = {};
    roapMessage.messageType = operation;
    roapMessage.sdp = sdp; // may be null or undefined
    if (operation === 'OFFER') {
      roapMessage.offererSessionId = that.sessionId;
      roapMessage.answererSessionId = that.otherSessionId; // may be null
      that.sequenceNumber += 1;
      roapMessage.seq = that.sequenceNumber;
            // The tiebreaker needs to be neither 0 nor 429496725.
      roapMessage.tiebreaker = Math.floor((Math.random() * 429496723) + 1);
    } else {
      roapMessage.offererSessionId = that.incomingMessage.offererSessionId;
      roapMessage.answererSessionId = that.sessionId;
      roapMessage.seq = that.incomingMessage.seq;
    }
    that.onsignalingmessage(JSON.stringify(roapMessage));
  };

    /**
     * Internal something-bad-happened function.
     * @param {string} text What happened - suitable for logging.
     */
  that.error = (text) => {
    throw new Error(`Error in RoapOnJsep: ${text}`);
  };
  that.roapSessionId += 1;
  that.sessionId = that.roapSessionId;
  that.sequenceNumber = 0; // Number of last ROAP message sent. Starts at 1.
  that.actionNeeded = false;
  that.iceStarted = false;
  that.moreIceComing = true;
  that.iceCandidateCount = 0;
  that.onsignalingmessage = spec.callback;

  that.peerConnection.onopen = () => {
    if (that.onopen) {
      that.onopen();
    }
  };

  that.peerConnection.onaddstream = (stream) => {
    if (that.onaddstream) {
      that.onaddstream(stream);
    }
  };

  that.peerConnection.onremovestream = (stream) => {
    if (that.onremovestream) {
      that.onremovestream(stream);
    }
  };

  that.peerConnection.oniceconnectionstatechange = (e) => {
    if (that.oniceconnectionstatechange) {
      that.oniceconnectionstatechange(e.currentTarget.iceConnectionState);
    }
  };

    // Variables that are part of the public interface of PeerConnection
    // in the 28 January 2012 version of the webrtc specification.
  that.onaddstream = null;
  that.onremovestream = null;
  that.state = 'new';
    // Auto-fire next events.
  that.markActionNeeded();
  return that;
};
