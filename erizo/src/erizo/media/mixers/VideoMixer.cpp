/*
 * VideoMixer.cpp
 */

#include "VideoMixer.h"
#include "VideoUtils.h"
#include "../../rtp/RTPSink.h"
#include "../../WebRtcConnection.h"

namespace erizo {

  DEFINE_LOGGER(VideoMixer, "media.mixers.VideoMixer");

  VideoMixer::VideoMixer() {

      subscriber = NULL;
      sentPackets_ = 0;
      ip = new InputProcessor();
      sink_ = new RTPSink("127.0.0.1", "50000");
      MediaInfo m;
      m.processorType = RTP_ONLY;
      //	m.videoCodec.bitRate = 2000000;
      //	ELOG_DEBUG("m.videoCodec.bitrate %d\n", m.videoCodec.bitRate);
      m.hasVideo = true;
      m.videoCodec.width = 640;
      m.videoCodec.height = 480;
      ip->init(m, this);

      MediaInfo om;
      om.processorType = RTP_ONLY;
      om.videoCodec.bitRate = 2000000;
      om.videoCodec.width = 640;
      om.videoCodec.height = 480;
      om.videoCodec.frameRate = 20;
      om.hasVideo = true;
      op = new OutputProcessor();
      op->init(om, this);

    }

  VideoMixer::~VideoMixer() {
    delete ip; ip = NULL;
    delete sink_; sink_ = NULL;
    delete op; op = NULL;
  }

  int VideoMixer::deliverAudioData(char* buf, int len) {
    return 0;
  }

  int VideoMixer::deliverVideoData(char* buf, int len) {
    return 0;
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

