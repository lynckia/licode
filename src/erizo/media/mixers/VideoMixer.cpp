/*
 * VideoMixer.cpp
 */

#include "VideoMixer.h"
#include "../../WebRtcConnection.h"

namespace erizo {
  VideoMixer::VideoMixer() :
    MediaReceiver() {


    }

  VideoMixer::~VideoMixer() {
  }

  int VideoMixer::receiveAudioData(char* buf, int len) {

  }

  int VideoMixer::receiveVideoData(char* buf, int len) {
  }

  void VideoMixer::receiveRawData(RawDataPacket& pkt) {
  }

  void VideoMixer::receiveRtpData(unsigned char*rtpdata, int len) {
  }

  void VideoMixer::setPublisher(WebRtcConnection* webRtcConn) {
  }

  void VideoMixer::addSubscriber(WebRtcConnection* webRtcConn,
      int peerId) {
  }

  void VideoMixer::removeSubscriber(int peerId) {
  }

  void VideoMixer::closeAll() {
  }

}/* namespace erizo */

