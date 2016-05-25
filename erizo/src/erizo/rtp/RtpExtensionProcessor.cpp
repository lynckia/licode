/*
 * RtpExtensionProcessor.cpp
 */

#include "RtpExtensionProcessor.h"
#include "time.h"

namespace erizo{
  DEFINE_LOGGER(RtpExtensionProcessor, "RtpExtensionProcessor");

  RtpExtensionProcessor::RtpExtensionProcessor(){
    translationMap_["urn:ietf:params:rtp-hdrext:ssrc-audio-level"] = SSRC_AUDIO_LEVEL;
    translationMap_["http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time"] = ABS_SEND_TIME;
    translationMap_["urn:ietf:params:rtp-hdrext:toffset"] = TOFFSET;
    translationMap_["urn:3gpp:video-orientation"] = VIDEO_ORIENTATION;
    memset(extMapVideo_, 0, sizeof(int)*10);
    memset(extMapAudio_, 0, sizeof(int)*10);
  }

  RtpExtensionProcessor::~RtpExtensionProcessor(){
  }

  void RtpExtensionProcessor::setSdpInfo (const SdpInfo& theInfo){
    // We build the Extension Map
    for (unsigned int i = 0; i < theInfo.extMapVector.size(); i++){
      const ExtMap& theMap = theInfo.extMapVector[i];
      std::map<std::string, uint8_t>::iterator it;
      switch(theMap.mediaType){
        case VIDEO_TYPE:
          it = translationMap_.find(theMap.uri);
          if (it!=translationMap_.end()){
            ELOG_DEBUG("Adding RTP Extension for video %s, value %u", theMap.uri.c_str(), theMap.value);
            extMapVideo_[theMap.value] = (*it).second;
          } else {
            ELOG_WARN("Unsupported extension %s", theMap.uri.c_str());
          }
          break;
        case AUDIO_TYPE:
          it = translationMap_.find(theMap.uri);
          if (it!=translationMap_.end()){
            ELOG_DEBUG("Adding RTP Extension for Audio %s, value %u", theMap.uri.c_str(), theMap.value);
            extMapAudio_[theMap.value]=(*it).second;
          } else {
            ELOG_WARN("Unsupported extension %s", theMap.uri.c_str());
          }
          break;
        default:
          ELOG_WARN("Unknown type of extMap, ignoring");
          break;
      }
    }
  }

  uint32_t RtpExtensionProcessor::processRtpExtensions(dataPacket& p) {
    RtpHeader* head = reinterpret_cast<RtpHeader*>(p.data);
    uint32_t len = p.length;
    int* extMap; 
    if (head->getExtension()){
      switch(p.type){
        case VIDEO_PACKET:
          extMap = extMapVideo_;
          break;
        case AUDIO_PACKET:
          extMap = extMapAudio_;
          break;
        default:
          ELOG_WARN("Won't process RTP extensions for unknown type packets");
          return 0;
          break;
      }
      uint16_t totalExtLength = head->getExtLength();
      if (head->getExtId()==0xBEDE){
        char* extBuffer = (char*)&head->extensions;
        uint8_t extByte = 0;
        uint16_t currentPlace = 1;
        uint8_t extId = 0;
        uint8_t extLength = 0;
        while(currentPlace < (totalExtLength*4)) {
          extByte = (uint8_t)(*extBuffer);
          extId = extByte >> 4;
          extLength = extByte & 0x0F;
          if (extId!=0 && extMap[extId] !=0){
            switch(extMap[extId]){
              case ABS_SEND_TIME:
                processAbsSendTime(extBuffer);
                break;
            }
          }
          extBuffer = extBuffer + extLength + 2;
          currentPlace= currentPlace + extLength + 2;
        }
      }
    }
    return len;
  }

  uint32_t RtpExtensionProcessor::processAbsSendTime(char* buf){
    struct timeval theNow;
    AbsSendTimeExtension* head = reinterpret_cast<AbsSendTimeExtension*>(buf);
    gettimeofday(&theNow, NULL);
    uint8_t seconds = theNow.tv_sec & 0x3F;
    uint32_t absecs = theNow.tv_usec* ((1LL << 18)-1) *1e-6;
    absecs = (seconds << 18) + absecs;
    head->setAbsSendTime(absecs);
    return 0;
  }

  uint32_t RtpExtensionProcessor::stripExtension (char* buf, int len){
    return len;
  }

} // namespace erizo

