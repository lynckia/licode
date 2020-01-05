/* global */

import StateMachine from '../../lib/state-machine';
import Logger from '../utils/Logger';

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
      Logger.info(`FSM onBeforeClose from: ${lifecycle.from}, to: ${lifecycle.to}`);
      return this.baseStackCalls.protectedClose();
    },

    onBeforeAddIceCandidate: function onBeforeAddIceCandidate(lifecycle, candidate) {
      Logger.info(`FSM onBeforeAddIceCandidate, from ${lifecycle.from}, to: ${lifecycle.to}`);
      return this.baseStackCalls.protectedAddIceCandidate(candidate);
    },

    onBeforeAddStream: function onBeforeAddStream(lifecycle, stream) {
      Logger.info(`FSM onBeforeAddStream, from ${lifecycle.from}, to: ${lifecycle.to}`);
      return this.baseStackCalls.protectedAddStream(stream);
    },

    onBeforeRemoveStream: function onBeforeRemoveStream(lifecycle, stream) {
      Logger.info(`FSM onBeforeRemoveStream, from ${lifecycle.from}, to: ${lifecycle.to}`);
      return this.baseStackCalls.protectedRemoveStream(stream);
    },

    onBeforeCreateOffer: function onBeforeCreateOffer(lifecycle, isSubscribe) {
      Logger.info(`FSM onBeforeCreateOffer, from ${lifecycle.from}, to: ${lifecycle.to}`);
      return this.baseStackCalls.protectedCreateOffer(isSubscribe);
    },

    onBeforeProcessOffer:
    function onBeforeProcessOffer(lifecycle, message) {
      Logger.info(`FSM onBeforeProcessOffer, from ${lifecycle.from}, to: ${lifecycle.to}`);
      return this.baseStackCalls.protectedProcessOffer(message);
    },

    onBeforeProcessAnswer:
    function onBeforeProcessAnswer(lifecycle, message) {
      Logger.info(`FSM onBeforeProcessAnswer from: ${lifecycle.from}, to: ${lifecycle.to}`);
      return this.baseStackCalls.protectedProcessAnswer(message);
    },

    onBeforeNegotiateMaxBW:
    function onBeforeNegotiateMaxBW(lifecycle, configInput, callback) {
      Logger.info(`FSM onBeforeNegotiateMaxBW from: ${lifecycle.from}, to: ${lifecycle.to}`);
      return this.baseStackCalls.protectedNegotiateMaxBW(configInput, callback);
    },

    onStable: function onStable(lifecycle) {
      Logger.info(`FSM reached STABLE, from ${lifecycle.from}, to: ${lifecycle.to}`);
    },

    onClosed: function onClosed(lifecycle) {
      Logger.info(`FSM reached close, from ${lifecycle.from}, to: ${lifecycle.to}`);
    },

    onTransition: function saveToHistory(lifecycle) {
      Logger.debug(`FSM onTransition, transition: ${lifecycle.transition}, from ${lifecycle.from}, to: ${lifecycle.to}`);
      this.history.push(
        { from: lifecycle.from, to: lifecycle.to, transition: lifecycle.transition });
      if (this.history.length > HISTORY_SIZE_LIMIT) {
        this.history.shift();
      }
    },

    onError: function onError(lifecycle, message) {
      Logger.warning(`FSM Error Transition Failed, message: ${message}, from: ${lifecycle.from}, to: ${lifecycle.to}, printing history:`);
      this.history.forEach((item) => {
        Logger.warning(item);
      });
    },

    onInvalidTransition: function onInvalidTransition(transition, from, to) {
      if (from === 'closed') {
        Logger.info(`Trying to transition a closed FSM, transition: ${transition}, from: ${from}, to: ${to}`);
        return;
      }
      Logger.warning(`FSM Error Invalid transition: ${transition}, from: ${from}, to: ${to}`);
    },

    onPendingTransition: function onPendingTransition(transition, from, to) {
      Logger.warning(`FSM Error Pending transition: ${transition}, from: ${from}, to: ${to}`);
    },
  },
});

export default PeerConnectionFsm;
