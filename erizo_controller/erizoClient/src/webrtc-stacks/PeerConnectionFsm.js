/* global */

import StateMachine from '../../lib/state-machine';
import Logger from '../utils/Logger';

const log = Logger.module('PeerConnectionFsm');
const activeStates = ['initial', 'failed', 'stable'];
const HISTORY_SIZE_LIMIT = 200;
// FSM
const PeerConnectionFsm = StateMachine.factory({
  init: 'initial',
  transitions: [
    { name: 'create-offer', from: activeStates, to: 'stable' },
    { name: 'add-ice-candidate', from: activeStates, to: function nextState() { return this.state; } },
    { name: 'process-answer', from: activeStates, to: 'stable' },
    { name: 'process-offer', from: activeStates, to: 'stable' },
    { name: 'negotiate-max-bw', from: activeStates, to: 'stable' },
    { name: 'add-stream', from: activeStates, to: function nextState() { return this.state; } },
    { name: 'remove-stream', from: activeStates, to: function nextState() { return this.state; } },
    { name: 'close', from: activeStates, to: 'closed' },
    { name: 'error', from: '*', to: 'failed' },
  ],
  data: function data(baseStackCalls) {
    return { baseStackCalls,
      history: [],
    };
  },
  methods: {
    getHistory: function getHistory() {
      return this.history;
    },

    onBeforeClose: function onBeforeClose(lifecycle) {
      log.debug(`mesage: onBeforeClose, from: ${lifecycle.from}, to: ${lifecycle.to}`);
      return this.baseStackCalls.protectedClose();
    },

    onBeforeAddIceCandidate: function onBeforeAddIceCandidate(lifecycle, candidate) {
      log.debug(`message: onBeforeAddIceCandidate, from: ${lifecycle.from}, to: ${lifecycle.to}`);
      return this.baseStackCalls.protectedAddIceCandidate(candidate);
    },

    onBeforeAddStream: function onBeforeAddStream(lifecycle, stream) {
      log.debug(`message: onBeforeAddStream, from: ${lifecycle.from}, to: ${lifecycle.to}`);
      return this.baseStackCalls.protectedAddStream(stream);
    },

    onBeforeRemoveStream: function onBeforeRemoveStream(lifecycle, stream) {
      log.debug(`message: onBeforeRemoveStream, from: ${lifecycle.from}, to: ${lifecycle.to}`);
      return this.baseStackCalls.protectedRemoveStream(stream);
    },

    onBeforeCreateOffer: function onBeforeCreateOffer(lifecycle, isSubscribe) {
      log.debug(`message: onBeforeCreateOffer, from: ${lifecycle.from}, to: ${lifecycle.to}`);
      return this.baseStackCalls.protectedCreateOffer(isSubscribe);
    },

    onBeforeProcessOffer:
    function onBeforeProcessOffer(lifecycle, message) {
      log.debug(`message: onBeforeProcessOffer, from: ${lifecycle.from}, to: ${lifecycle.to}`);
      return this.baseStackCalls.protectedProcessOffer(message);
    },

    onBeforeProcessAnswer:
    function onBeforeProcessAnswer(lifecycle, message) {
      log.debug(`message: onBeforeProcessAnswer, from: ${lifecycle.from}, to: ${lifecycle.to}`);
      return this.baseStackCalls.protectedProcessAnswer(message);
    },

    onBeforeNegotiateMaxBW:
    function onBeforeNegotiateMaxBW(lifecycle, configInput, callback) {
      log.debug(`message: onBeforeNegotiateMaxBW, from: ${lifecycle.from}, to: ${lifecycle.to}`);
      return this.baseStackCalls.protectedNegotiateMaxBW(configInput, callback);
    },

    onStable: function onStable(lifecycle) {
      log.debug(`message: reached STABLE, from: ${lifecycle.from}, to: ${lifecycle.to}`);
    },

    onClosed: function onClosed(lifecycle) {
      log.debug(`message: reached close, from: ${lifecycle.from}, to: ${lifecycle.to}`);
    },

    onTransition: function saveToHistory(lifecycle) {
      log.info(`message: onTransition, transition: ${lifecycle.transition}, from: ${lifecycle.from}, to: ${lifecycle.to}`);
      this.history.push(
        { from: lifecycle.from, to: lifecycle.to, transition: lifecycle.transition });
      if (this.history.length > HISTORY_SIZE_LIMIT) {
        this.history.shift();
      }
    },

    onError: function onError(lifecycle, message) {
      log.warning(`message: Error Transition Failed, message: ${message}, from: ${lifecycle.from}, to: ${lifecycle.to}, printing history`);
      this.history.forEach((item) => {
        log.warning(`message: Error Transition Failed continuation, item: ${JSON.stringify(item)}`);
      });
    },

    onInvalidTransition: function onInvalidTransition(transition, from, to) {
      if (from === 'closed') {
        log.debug(`message:Trying to transition a closed state, transition: ${transition}, from: ${from}, to: ${to}`);
        return;
      }
      log.warning(`message: Error Invalid transition, transition: ${transition}, from: ${from}, to: ${to}`);
    },

    onPendingTransition: function onPendingTransition(transition, from, to) {
      const lastTransition = this.history.lenth > 0 ? this.history[this.history.length - 1].transition : 'none';
      log.warning(`message: Error Pending transition, transition: ${transition}, from: ${from}, to: ${to}, lastTransition: ${lastTransition}`);
    },
  },
});

export default PeerConnectionFsm;
