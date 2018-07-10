#include "media/MediaProcessor.h"

extern "C" {
#include <libavutil/mathematics.h>
}

#include <string>
#include <cstring>

#include "rtp/RtpVP8Fragmenter.h"
#include "rtp/RtpHeaders.h"
#include "media/codecs/VideoCodec.h"
#include "media/codecs/AudioCodec.h"
#include "lib/Clock.h"
#include "lib/ClockUtils.h"

using std::memcpy;

namespace erizo {

DEFINE_LOGGER(InputProcessor, "media.InputProcessor");
DEFINE_LOGGER(OutputProcessor, "media.OutputProcessor");

InputProcessor::InputProcessor() {
  lastVideoTs_ = 0;

  audioUnpackager = 1;
  videoUnpackager = 1;
  gotUnpackagedFrame_ = false;
  upackagedSize_ = 0;
  decodedBuffer_ = NULL;
  unpackagedBuffer_ = NULL;
  unpackagedBufferPtr_ = NULL;
  decodedAudioBuffer_ = NULL;
  unpackagedAudioBuffer_ = NULL;

  av_register_all();
}

InputProcessor::~InputProcessor() {
  this->close();
}

int InputProcessor::init(const MediaInfo& info, RawDataReceiver* receiver) {
  this->mediaInfo = info;
  this->rawReceiver_ = receiver;
  if (mediaInfo.hasVideo) {
    mediaInfo.videoCodec.codec = VIDEO_CODEC_VP8;
    decodedBuffer_ = (unsigned char*) malloc(
        info.videoCodec.width * info.videoCodec.height * 3 / 2);
    unpackagedBufferPtr_ = unpackagedBuffer_ = (unsigned char*) malloc(UNPACKAGED_BUFFER_SIZE);
    if (!video_decoder_.initDecoder(mediaInfo.videoCodec)) {
      // TODO(javier) check this condition
    }
    if (!this->initVideoUnpackager()) {
      // TODO(javier) check this condition
    }
  }
  if (mediaInfo.hasAudio) {
    ELOG_DEBUG("Init AUDIO processor");
    mediaInfo.audioCodec.codec = AUDIO_CODEC_PCM_U8;
    if (!mediaInfo.audioCodec.sampleFmt) {
      mediaInfo.audioCodec.sampleFmt = AV_SAMPLE_FMT_S16;
    }
    if (!mediaInfo.audioCodec.channels) {
      mediaInfo.audioCodec.channels = 1;
    }
    decodedAudioBuffer_ = (unsigned char*) malloc(UNPACKAGED_BUFFER_SIZE);
    unpackagedAudioBuffer_ = (unsigned char*) malloc(UNPACKAGED_BUFFER_SIZE);
    audio_decoder_.initDecoder(mediaInfo.audioCodec);
    this->initAudioUnpackager();
  }
  return 0;
}

int InputProcessor::deliverAudioData_(std::shared_ptr<DataPacket> audio_packet) {
  if (audioUnpackager && audio_decoder_.isInitialized()) {
    std::shared_ptr<DataPacket> copied_packet = std::make_shared<DataPacket>(*audio_packet);
    int unp = unpackageAudio((unsigned char*) copied_packet->data, copied_packet->length,
        unpackagedAudioBuffer_);
    int len = audio_decoder_.decodeAudio(unpackagedAudioBuffer_, unp, decodedAudioBuffer_, UNPACKAGED_BUFFER_SIZE);
    if (len > 0) {
      RawDataPacket p;
      p.data = decodedAudioBuffer_;
      p.type = AUDIO;
      p.length = len;
      rawReceiver_->receiveRawData(p);
    }
    return len;
  }
  return 0;
}
int InputProcessor::deliverVideoData_(std::shared_ptr<DataPacket> video_packet) {
  if (videoUnpackager && video_decoder_.isInitialized()) {
    std::shared_ptr<DataPacket> copied_packet = std::make_shared<DataPacket>(*video_packet);
    int ret = unpackageVideo(reinterpret_cast<unsigned char*>(copied_packet->data), copied_packet->length,
        unpackagedBufferPtr_, &gotUnpackagedFrame_);
    if (ret < 0)
      return 0;
    upackagedSize_ += ret;
    unpackagedBufferPtr_ += ret;
    if (gotUnpackagedFrame_) {
      unpackagedBufferPtr_ -= upackagedSize_;
      ELOG_DEBUG("Tengo un frame desempaquetado!! Size = %d", upackagedSize_);
      int c;
      int gotDecodedFrame = 0;

      c = video_decoder_.decodeVideoBuffer(unpackagedBufferPtr_, upackagedSize_,
                               decodedBuffer_,
                               mediaInfo.videoCodec.width * mediaInfo.videoCodec.height * 3 / 2,
                               &gotDecodedFrame);

      upackagedSize_ = 0;
      gotUnpackagedFrame_ = 0;
      ELOG_DEBUG("Bytes dec = %d", c);
      if (gotDecodedFrame && c > 0) {
        ELOG_DEBUG("Tengo un frame decodificado!!");
        gotDecodedFrame = 0;
        RawDataPacket p;
        p.data = decodedBuffer_;
        p.length = c;
        p.type = VIDEO;
        rawReceiver_->receiveRawData(p);
      }

      return c;
    }
  }
  return 0;
}

int InputProcessor::deliverEvent_(MediaEventPtr event) {
  return 0;
}

bool InputProcessor::initAudioUnpackager() {
  audioUnpackager = 1;
  return true;
}

bool InputProcessor::initVideoUnpackager() {
  videoUnpackager = 1;
  return true;
}

int InputProcessor::unpackageAudio(unsigned char* inBuff, int inBuffLen, unsigned char* outBuff) {
  int l = inBuffLen - RtpHeader::MIN_SIZE;
  if (l < 0) {
    ELOG_ERROR("Error unpackaging audio");
    return 0;
  }
  memcpy(outBuff, &inBuff[RtpHeader::MIN_SIZE], l);

  return l;
}

int InputProcessor::unpackageVideo(unsigned char* inBuff, int inBuffLen, unsigned char* outBuff, int* gotFrame) {
  if (videoUnpackager == 0) {
    ELOG_DEBUG("Unpackager not correctly initialized");
    return -1;
  }

  int inBuffOffset = 0;
  *gotFrame = 0;
  RtpHeader* head = reinterpret_cast<RtpHeader*>(inBuff);
  if (head->getPayloadType() != 100) {
    return -1;
  }

  int l = inBuffLen - head->getHeaderLength();
  inBuffOffset += head->getHeaderLength();

  erizo::RTPPayloadVP8* parsed = pars.parseVP8((unsigned char*) &inBuff[inBuffOffset], l);
  memcpy(outBuff, parsed->data, parsed->dataLength);
  if (head->getMarker()) {
    *gotFrame = 1;
  }
  int ret = parsed->dataLength;
  delete parsed;
  return ret;
}

void InputProcessor::closeSink() {
  this->close();
}

void InputProcessor::close() {
  audio_decoder_.closeCodec();
  video_decoder_.closeCodec();
  free(decodedBuffer_); decodedBuffer_ = NULL;
  free(unpackagedBuffer_); unpackagedBuffer_ = NULL;
  free(unpackagedAudioBuffer_); unpackagedAudioBuffer_ = NULL;
  free(decodedAudioBuffer_); decodedAudioBuffer_ = NULL;
}

OutputProcessor::OutputProcessor() {
  audioPackager = 0;
  videoPackager = 0;
  timestamp_ = 0;

  encodedBuffer_ = NULL;
  packagedBuffer_ = NULL;
  rtpBuffer_ = NULL;
  encodedAudioBuffer_ = NULL;
  packagedAudioBuffer_ = NULL;

  avcodec_register_all();
  av_register_all();
}

OutputProcessor::~OutputProcessor() {
  this->close();
}

int OutputProcessor::init(const MediaInfo& info, RTPDataReceiver* rtpReceiver) {
  this->mediaInfo = info;
  this->rtpReceiver_ = rtpReceiver;

  encodedBuffer_ = (unsigned char*) malloc(UNPACKAGED_BUFFER_SIZE);
  packagedBuffer_ = (unsigned char*) malloc(PACKAGED_BUFFER_SIZE);
  rtpBuffer_ = (unsigned char*) malloc(PACKAGED_BUFFER_SIZE);
  if (info.processorType == PACKAGE_ONLY) {
    this->initVideoPackager();
    this->initAudioPackager();
    return 0;
  }
  if (mediaInfo.hasVideo) {
    this->mediaInfo.videoCodec.codec = VIDEO_CODEC_VP8;
    if (video_encoder_.initEncoder(mediaInfo.videoCodec)) {
      ELOG_DEBUG("Error initing encoder");
    }
    this->initVideoPackager();
  }
  if (mediaInfo.hasAudio) {
    ELOG_DEBUG("Init AUDIO processor");
    mediaInfo.audioCodec.codec = AUDIO_CODEC_PCM_U8;
    mediaInfo.audioCodec.sampleRate = 44100;
    mediaInfo.audioCodec.bitRate = 64000;
    mediaInfo.audioCodec.channels = 1;
    mediaInfo.audioCodec.sampleFmt = AV_SAMPLE_FMT_S16;
    audio_encoder_.initEncoder(mediaInfo.audioCodec);
    encodedAudioBuffer_ = (unsigned char*) malloc(UNPACKAGED_BUFFER_SIZE);
    packagedAudioBuffer_ = (unsigned char*) malloc(UNPACKAGED_BUFFER_SIZE);
    this->initAudioPackager();
  }

  return 0;
}

void OutputProcessor::close() {
  audio_encoder_.closeCodec();
  video_encoder_.closeCodec();
  free(encodedBuffer_); encodedBuffer_ = NULL;
  free(packagedBuffer_); packagedBuffer_ = NULL;
  free(rtpBuffer_); rtpBuffer_ = NULL;
  free(encodedAudioBuffer_); encodedAudioBuffer_ = NULL;
  free(packagedAudioBuffer_); packagedAudioBuffer_ = NULL;
}


void OutputProcessor::receiveRawData(const RawDataPacket& packet) {
  if (packet.type == VIDEO) {
    EncodeVideoBufferCB done_callback = [this](bool fill_buffer, int size) {
      if (fill_buffer) {
        this->packageVideo(encodedBuffer_, size, packagedBuffer_);
      }
    };
    video_encoder_.encodeVideoBuffer(packet.data, packet.length, encodedBuffer_, done_callback);
  } else {
    // int a = audio_encoder_.encodeAudio(packet.data, packet.length, &pkt);
    // if (a > 0) {
    //   ELOG_DEBUG("GUAY a %d", a);
    // }
  }
  // av_free_packet(&pkt);
}

bool OutputProcessor::initAudioPackager() {
  audioPackager = 1;
  audioSeqnum_ = 0;
  return true;
}

bool OutputProcessor::initVideoPackager() {
  seqnum_ = 0;
  videoPackager = 1;
  return true;
}

int OutputProcessor::packageAudio(unsigned char* inBuff, int inBuffLen, unsigned char* outBuff,
                                  long int pts) {  // NOLINT
  if (audioPackager == 0) {
    ELOG_DEBUG("No se ha inicializado el codec de output audio RTP");
    return -1;
  }

  // uint64_t millis = ClockUtils::timePointToMs(clock::now());

  RtpHeader head;
  head.setSeqNumber(audioSeqnum_++);
  // head.setTimestamp(millis*8);
  head.setMarker(1);
  if (pts == 0) {
    // head.setTimestamp(audioSeqnum_*160);
    head.setTimestamp(av_rescale(audioSeqnum_, (mediaInfo.audioCodec.sampleRate/1000), 1));
  } else {
    // head.setTimestamp(pts*8);
    head.setTimestamp(av_rescale(pts, mediaInfo.audioCodec.sampleRate, 1000));
  }
  head.setSSRC(44444);
  head.setPayloadType(mediaInfo.rtpAudioInfo.PT);

  // memcpy (rtpAudioBuffer_, &head, head.getHeaderLength());
  // memcpy(&rtpAudioBuffer_[head.getHeaderLength()], inBuff, inBuffLen);
  memcpy(outBuff, &head, head.getHeaderLength());
  memcpy(&outBuff[head.getHeaderLength()], inBuff, inBuffLen);
  // sink_->sendData(rtpBuffer_, l);
  // rtpReceiver_->receiveRtpData(rtpBuffer_, (inBuffLen + RTP_HEADER_LEN));
  return (inBuffLen + head.getHeaderLength());
}

int OutputProcessor::packageVideo(unsigned char* inBuff, int buffSize, unsigned char* outBuff,
                                  long int pts) {  // NOLINT
  if (videoPackager == 0) {
    ELOG_DEBUG("No se ha inicailizado el codec de output vídeo RTP");
    return -1;
  }

  if (buffSize <= 0) {
    return -1;
  }
  RtpVP8Fragmenter frag(inBuff, buffSize);
  bool lastFrame = false;
  unsigned int outlen = 0;
  uint64_t millis = ClockUtils::timePointToMs(clock::now());
  // timestamp_ += 90000 / mediaInfo.videoCodec.frameRate;
  // int64_t pts = av_rescale(lastPts_, 1000000, (long int)video_time_base_);

  do {
    outlen = 0;
    frag.getPacket(outBuff, &outlen, &lastFrame);
    RtpHeader rtpHeader;
    rtpHeader.setMarker(lastFrame?1:0);
    rtpHeader.setSeqNumber(seqnum_++);
    if (pts == 0) {
        rtpHeader.setTimestamp(av_rescale(millis, 90000, 1000));
    } else {
        rtpHeader.setTimestamp(av_rescale(pts, 90000, 1000));
    }
    rtpHeader.setSSRC(55543);
    rtpHeader.setPayloadType(100);
    memcpy(rtpBuffer_, &rtpHeader, rtpHeader.getHeaderLength());
    memcpy(&rtpBuffer_[rtpHeader.getHeaderLength()], outBuff, outlen);

    int l = outlen + rtpHeader.getHeaderLength();
    rtpReceiver_->receiveRtpData(rtpBuffer_, l);
  } while (!lastFrame);

  return 0;
}

}  // namespace erizo
