import Logger from '../utils/Logger';
import BaseStack from './BaseStack';

import SemanticSdp from '../../../common/semanticSdp/SemanticSdp';
import SdpHelpers from '../utils/SdpHelpers';

const EdgeStack = (specInput) => {
  Logger.info('Starting Edge stack');
  const spec = specInput;

  // Setting bundle policy
  spec.bundlePolicy = 'max-bundle';

  const that = BaseStack(spec);

  const baseSetLocalDescForOffer = that.setLocalDescForOffer;

  that.setLocalDescForOffer = (isSubscribe, streamId, sessionDescription) => {
    const thisSessionDescription = sessionDescription;
    const localSdp = SemanticSdp.SDPInfo.processString(thisSessionDescription.sdp);
    SdpHelpers.filterAbsSendTimeAudio(localSdp);
    thisSessionDescription.sdp = localSdp.toString();
    baseSetLocalDescForOffer(isSubscribe, streamId, thisSessionDescription);
  };

  const baseSetLocalDescForAnswerp2p = that.setLocalDescForAnswerp2p;

  that.setLocalDescForAnswerp2p = (sessionDescription) => {
    const thisSessionDescription = sessionDescription;
    const localSdp = SemanticSdp.SDPInfo.processString(thisSessionDescription.sdp);
    SdpHelpers.filterAbsSendTimeAudio(localSdp);
    thisSessionDescription.sdp = localSdp.toString();
    baseSetLocalDescForAnswerp2p(thisSessionDescription);
  };

  const baseProcessOffer = that.processOffer;

  that.processOffer = (message) => {
    const thisMessage = message;
    const remoteSdp = SemanticSdp.SDPInfo.processString(thisMessage.sdp);
    SdpHelpers.filterAbsSendTimeAudio(remoteSdp);
    thisMessage.sdp = remoteSdp.toString();
    baseProcessOffer(thisMessage);
  };

  const baseProcessAnswer = that.processAnswer;

  that.processAnswer = (message) => {
    const thisMessage = message;
    const remoteSdp = SemanticSdp.SDPInfo.processString(thisMessage.sdp);
    SdpHelpers.filterAbsSendTimeAudio(remoteSdp);
    thisMessage.sdp = remoteSdp.toString();
    baseProcessAnswer(thisMessage);
  };

  return that;
};

export default EdgeStack;
