/*
 * VideoMixer.cpp
 */
#include "media/mixers/VideoMixer.h"

#include "media/mixers/VideoUtils.h"
#include "./WebRtcConnection.h"

namespace erizo {
DEFINE_LOGGER(VideoMixer, "media.mixers.VideoMixer");

VideoMixer::VideoMixer() {
  subscriber = NULL;
  sentPackets_ = 0;
  ip = new InputProcessor();
  MediaInfo m;
  m.processorType = RTP_ONLY;
  // m.videoCodec.bitRate = 2000000;
  // ELOG_DEBUG("m.videoCodec.bitrate %d\n", m.videoCodec.bitRate);
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
  delete op; op = NULL;
}

int VideoMixer::deliverAudioData_(std::shared_ptr<DataPacket> audio_packet) {
  return 0;
}

int VideoMixer::deliverVideoData_(std::shared_ptr<DataPacket> video_packet) {
  return 0;
}

void VideoMixer::receiveRawData(const RawDataPacket& pkt) {
}

void VideoMixer::receiveRtpData(unsigned char* rtpdata, int len) {
}

void VideoMixer::addPublisher(WebRtcConnection* webRtcConn, int peerSSRC) {
}

void VideoMixer::setSubscriber(WebRtcConnection* webRtcConn) {
}

void VideoMixer::removePublisher(int peerSSRC) {
}

void VideoMixer::closeSink() {
  this->closeAll();
}

void VideoMixer::closeAll() {
}

}  // namespace erizo
