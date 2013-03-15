/*
 * VideoMixer.cpp
 */

#include "VideoMixer.h"
#include "VideoUtils.h"
#include "../../RTPSink.h"
#include "../../WebRtcConnection.h"

namespace erizo {
  VideoMixer::VideoMixer() {

      sendVideoBuffer_ = (char*) malloc(2000);
      sendAudioBuffer_ = (char*) malloc(2000);

      subscriber = NULL;
      sentPackets_ = 0;
      ip = new InputProcessor();
      sink_ = new RTPSink("127.0.0.1", "50000");
      MediaInfo m;
      m.proccessorType = RTP_ONLY;
      //	m.videoCodec.bitRate = 2000000;
      //	printf("m.videoCodec.bitrate %d\n\n", m.videoCodec.bitRate);
      m.hasVideo = true;
      m.videoCodec.width = 640;
      m.videoCodec.height = 480;
      ip->init(m, this);

      MediaInfo om;
      om.proccessorType = RTP_ONLY;
      om.videoCodec.bitRate = 2000000;
      om.videoCodec.width = 640;
      om.videoCodec.height = 480;
      om.videoCodec.frameRate = 20;
      om.hasVideo = true;
      op = new OutputProcessor();
      op->init(om, this);

    }

  VideoMixer::~VideoMixer() {

    if (sendVideoBuffer_)
      delete sendVideoBuffer_;
    if (sendAudioBuffer_)
      delete sendAudioBuffer_;
    if (sink_) {
      delete sink_;
    }
  }

  int VideoMixer::deliverAudioData(char* buf, int len) {

  }

  int VideoMixer::deliverVideoData(char* buf, int len) {

  }

  void VideoMixer::receiveRawData(RawDataPacket& pkt) {
  }

  void VideoMixer::receiveRtpData(unsigned char* rtpdata, int len){
  }

  void VideoMixer::addPublisher(WebRtcConnection* webRtcConn, int peerSSRC){
  }

  void VideoMixer::setSubscriber(WebRtcConnection* webRtcConn){
  }

  void VideoMixer::removePublisher(int peerSSRC) {
  }

  void VideoMixer::closeSink() {
    this->closeAll();
  }

  void VideoMixer::closeAll() {
  }

}/* namespace erizo */

