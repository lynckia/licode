/*
* OneToManyTranscoder.cpp
*/

#include <map>
#include <string>
#include <cstring>

#include "media/OneToManyTranscoder.h"
#include "./MediaStream.h"
#include "rtp/RtpHeaders.h"

using std::memcpy;

namespace erizo {

DEFINE_LOGGER(OneToManyTranscoder, "media.OneToManyTranscoder");

OneToManyTranscoder::OneToManyTranscoder() {
  publisher = NULL;
  sentPackets_ = 0;
  ip_ = new InputProcessor();
  MediaInfo m;
  m.processorType = RTP_ONLY;
  // m.videoCodec.bitRate = 2000000;
  // ELOG_DEBUG("m.videoCodec.bitrate %d\n", m.videoCodec.bitRate);
  m.hasVideo = true;
  m.videoCodec.width = 640;
  m.videoCodec.height = 480;
  m.hasAudio = false;

  ELOG_DEBUG("init ip");
  ip_->init(m, this);

  MediaInfo om;
  om.processorType = RTP_ONLY;
  om.videoCodec.bitRate = 2000000;
  om.videoCodec.width = 640;
  om.videoCodec.height = 480;
  om.videoCodec.frameRate = 20;
  om.hasVideo = true;
  // om.url = "file://tmp/test.mp4";

  om.hasAudio = false;

  op_ = new OutputProcessor();
  op_->init(om, this);
}

OneToManyTranscoder::~OneToManyTranscoder() {
  this->closeAll();
  delete ip_; ip_ = NULL;
  delete op_; op_ = NULL;
}

int OneToManyTranscoder::deliverAudioData_(std::shared_ptr<DataPacket> audio_packet) {
  if (subscribers.empty() || audio_packet->length <= 0)
  return 0;

  std::map<std::string, MediaSink*>::iterator it;
  for (it = subscribers.begin(); it != subscribers.end(); it++) {
    (*it).second->deliverAudioData(audio_packet);
  }

  return 0;
}

int OneToManyTranscoder::deliverVideoData_(std::shared_ptr<DataPacket> video_packet) {
  RtpHeader* theHead = reinterpret_cast<RtpHeader*>(video_packet->data);
  // ELOG_DEBUG("extension %d pt %u", theHead->getExtension(),
  // theHead->getPayloadType());

  if (theHead->getPayloadType() == 100) {
    ip_->deliverVideoData(video_packet);
  } else {
    memcpy(sendVideoBuffer_, video_packet->data, video_packet->length);
    this->receiveRtpData((unsigned char*) sendVideoBuffer_, video_packet->length);
  }

  sentPackets_++;
  return 0;
}

int OneToManyTranscoder::deliverEvent_(MediaEventPtr event) {
  return 0;
}

void OneToManyTranscoder::receiveRawData(const RawDataPacket& pkt) {
  // ELOG_DEBUG("Received %d", pkt.length);
  op_->receiveRawData(pkt);
}

void OneToManyTranscoder::receiveRtpData(unsigned char*rtpdata, int len) {
  if (subscribers.empty() || len <= 0)
  return;
  std::map<std::string, MediaSink*>::iterator it;
  std::shared_ptr<DataPacket> data_packet = std::make_shared<DataPacket>(0,
      reinterpret_cast<char*>(rtpdata), len, VIDEO_PACKET);
  for (it = subscribers.begin(); it != subscribers.end(); it++) {
    (*it).second->deliverVideoData(data_packet);
  }
  sentPackets_++;
}

void OneToManyTranscoder::setPublisher(MediaSource* webRtcConn) {
  this->publisher = webRtcConn;
}

void OneToManyTranscoder::addSubscriber(MediaSink* webRtcConn, const std::string& peerId) {
  this->subscribers[peerId] = webRtcConn;
}

void OneToManyTranscoder::removeSubscriber(const std::string& peerId) {
  if (this->subscribers.find(peerId) != subscribers.end()) {
    delete this->subscribers[peerId];
    this->subscribers.erase(peerId);
  }
}

void OneToManyTranscoder::closeAll() {
  ELOG_WARN("OneToManyTranscoder closeAll");
  std::map<std::string, MediaSink*>::iterator it = subscribers.begin();
  while (it != subscribers.end()) {
    delete (*it).second;
    subscribers.erase(it++);
  }
  delete this->publisher;
}

}  // namespace erizo
