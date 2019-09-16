#include "media/MediaProcessor.h"

extern "C" {
#include <libavutil/mathematics.h>
}

#include <string>
#include <cstring>

#include "rtp/RtpVP8Fragmenter.h"
#include "rtp/RtpHeaders.h"
#include "media/codecs/VideoCodec.h"
#include "lib/Clock.h"
#include "lib/ClockUtils.h"

using std::memcpy;

namespace erizo {

DEFINE_LOGGER(InputProcessor, "media.InputProcessor");
DEFINE_LOGGER(OutputProcessor, "media.OutputProcessor");

InputProcessor::InputProcessor() {
  audioDecoder = 0;
  videoDecoder = 0;
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
    if (!vDecoder.initDecoder(mediaInfo.videoCodec)) {
      // TODO(javier) check this condition
    }
    videoDecoder = 1;
    if (!this->initVideoUnpackager()) {
      // TODO(javier) check this condition
    }
  }
  if (mediaInfo.hasAudio) {
    ELOG_DEBUG("Init AUDIO processor");
    mediaInfo.audioCodec.codec = AUDIO_CODEC_PCM_U8;
    decodedAudioBuffer_ = (unsigned char*) malloc(UNPACKAGED_BUFFER_SIZE);
    unpackagedAudioBuffer_ = (unsigned char*) malloc(
        UNPACKAGED_BUFFER_SIZE);
    this->initAudioDecoder();
    this->initAudioUnpackager();
  }
  return 0;
}

int InputProcessor::deliverAudioData_(std::shared_ptr<DataPacket> audio_packet) {
  if (audioDecoder && audioUnpackager) {
    std::shared_ptr<DataPacket> copied_packet = std::make_shared<DataPacket>(*audio_packet);
    ELOG_DEBUG("Decoding audio");
    int unp = unpackageAudio((unsigned char*) copied_packet->data, copied_packet->length,
        unpackagedAudioBuffer_);
    int a = decodeAudio(unpackagedAudioBuffer_, unp, decodedAudioBuffer_);
    ELOG_DEBUG("DECODED AUDIO a %d", a);
    RawDataPacket p;
    p.data = decodedAudioBuffer_;
    p.type = AUDIO;
    p.length = a;
    rawReceiver_->receiveRawData(p);
    return a;
  }
  return 0;
}
int InputProcessor::deliverVideoData_(std::shared_ptr<DataPacket> video_packet) {
  if (videoUnpackager && videoDecoder) {
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

      c = vDecoder.decodeVideo(unpackagedBufferPtr_, upackagedSize_,
                               decodedBuffer_,
                               mediaInfo.videoCodec.width * mediaInfo.videoCodec.height * 3 / 2,
                               &gotDecodedFrame);

      upackagedSize_ = 0;
      gotUnpackagedFrame_ = 0;
      ELOG_DEBUG("Bytes dec = %d", c);
      if (gotDecodedFrame && c > 0) {
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

bool InputProcessor::initAudioDecoder() {
  aDecoder = avcodec_find_decoder(static_cast<AVCodecID>(mediaInfo.audioCodec.codec));
  if (!aDecoder) {
    ELOG_DEBUG("Decoder de audio no encontrado");
    return false;
  }

  aDecoderContext = avcodec_alloc_context3(aDecoder);
  if (!aDecoderContext) {
    ELOG_DEBUG("Error de memoria en decoder de audio");
    return false;
  }

  aDecoderContext->sample_fmt = AV_SAMPLE_FMT_S16;
  aDecoderContext->bit_rate = mediaInfo.audioCodec.bitRate;
  aDecoderContext->sample_rate = mediaInfo.audioCodec.sampleRate;
  aDecoderContext->channels = 1;

  if (avcodec_open2(aDecoderContext, aDecoder, NULL) < 0) {
    ELOG_DEBUG("Error al abrir el decoder de audio");
    return false;
  }
  audioDecoder = 1;
  return true;
}

bool InputProcessor::initAudioUnpackager() {
  audioUnpackager = 1;
  return true;
}

bool InputProcessor::initVideoUnpackager() {
  videoUnpackager = 1;
  return true;
}

int InputProcessor::decodeAudio(unsigned char* inBuff, int inBuffLen, unsigned char* outBuff) {
  if (audioDecoder == 0) {
    ELOG_DEBUG("No se han inicializado los parámetros del audioDecoder");
    return -1;
  }

  AVPacket avpkt;
  int outSize = 0;
  int decSize = 0;
  int len = -1;
  uint8_t *decBuff = reinterpret_cast<uint8_t*>(malloc(16000));

  av_init_packet(&avpkt);
  avpkt.data = (unsigned char*) inBuff;
  avpkt.size = inBuffLen;

  while (avpkt.size > 0) {
    outSize = 16000;

    // Puede fallar. Cogido de libavcodec/utils.c del paso de avcodec_decode_audio3 a avcodec_decode_audio4
    // avcodec_decode_audio3(aDecoderContext, (short*)decBuff, &outSize, &avpkt);

    AVFrame frame;
    int got_frame = 0;

    //      aDecoderContext->get_buffer = avcodec_default_get_buffer;
    //      aDecoderContext->release_buffer = avcodec_default_release_buffer;

    len = avcodec_decode_audio4(aDecoderContext, &frame, &got_frame,
        &avpkt);
    if (len >= 0 && got_frame) {
      int plane_size;
      // int planar = av_sample_fmt_is_planar(aDecoderContext->sample_fmt);
      int data_size = av_samples_get_buffer_size(&plane_size,
          aDecoderContext->channels, frame.nb_samples,
          aDecoderContext->sample_fmt, 1);
      if (outSize < data_size) {
        ELOG_DEBUG("output buffer size is too small for the current frame");
        free(decBuff);
        return AVERROR(EINVAL);
      }

      memcpy(decBuff, frame.extended_data[0], plane_size);

      /* Si hay más de un canal
         if (planar && aDecoderContext->channels > 1) {
         uint8_t *out = ((uint8_t *)decBuff) + plane_size;
         for (int ch = 1; ch < aDecoderContext->channels; ch++) {
         memcpy(out, frame.extended_data[ch], plane_size);
         out += plane_size;
         }
         }
         */
      outSize = data_size;
    } else {
      outSize = 0;
    }

    if (len < 0) {
      ELOG_DEBUG("Error al decodificar audio");
      free(decBuff);
      return -1;
    }

    avpkt.size -= len;
    avpkt.data += len;

    if (outSize <= 0) {
      continue;
    }

    memcpy(outBuff, decBuff, outSize);
    outBuff += outSize;
    decSize += outSize;
  }

  free(decBuff);

  if (outSize <= 0) {
    ELOG_DEBUG("Error de decodificación de audio debido a tamaño incorrecto");
    return -1;
  }

  return decSize;
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

boost::future<void> InputProcessor::close() {
  if (audioDecoder == 1) {
    avcodec_close(aDecoderContext);
    av_free(aDecoderContext);
    audioDecoder = 0;
  }

  if (videoDecoder == 1) {
    vDecoder.closeDecoder();
    videoDecoder = 0;
  }
  free(decodedBuffer_); decodedBuffer_ = NULL;
  free(unpackagedBuffer_); unpackagedBuffer_ = NULL;
  free(unpackagedAudioBuffer_); unpackagedAudioBuffer_ = NULL;
  free(decodedAudioBuffer_); decodedAudioBuffer_ = NULL;
  std::shared_ptr<boost::promise<void>> p = std::make_shared<boost::promise<void>>();
  p->set_value();
  return p->get_future();
}

OutputProcessor::OutputProcessor() {
  audioCoder = 0;
  videoCoder = 0;

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
    if (vCoder.initEncoder(mediaInfo.videoCodec)) {
      ELOG_DEBUG("Error initing encoder");
    }
    this->initVideoPackager();
  }
  if (mediaInfo.hasAudio) {
    ELOG_DEBUG("Init AUDIO processor");
    mediaInfo.audioCodec.codec = AUDIO_CODEC_PCM_U8;
    mediaInfo.audioCodec.sampleRate = 44100;
    mediaInfo.audioCodec.bitRate = 64000;
    encodedAudioBuffer_ = (unsigned char*) malloc(UNPACKAGED_BUFFER_SIZE);
    packagedAudioBuffer_ = (unsigned char*) malloc(UNPACKAGED_BUFFER_SIZE);
    this->initAudioCoder();
    this->initAudioPackager();
  }

  return 0;
}

void OutputProcessor::close() {
  if (audioCoder == 1) {
    avcodec_close(aCoderContext);
    av_free(aCoderContext);
    audioCoder = 0;
  }

  if (videoCoder == 1) {
    vCoder.closeEncoder();
    videoCoder = 0;
  }

  free(encodedBuffer_); encodedBuffer_ = NULL;
  free(packagedBuffer_); packagedBuffer_ = NULL;
  free(rtpBuffer_); rtpBuffer_ = NULL;
  free(encodedAudioBuffer_); encodedAudioBuffer_ = NULL;
  free(packagedAudioBuffer_); packagedAudioBuffer_ = NULL;
}

void OutputProcessor::requestKeyframe() {
  vCoder.requestKeyframe();
}

void OutputProcessor::setTargetBitrate(uint64_t bitrate) {
  vCoder.setTargetBitrate(bitrate);
}

void OutputProcessor::receiveRawData(const RawDataPacket& packet) {
  if (packet.type == VIDEO) {
    // ELOG_DEBUG("Encoding video: size %d", packet.length);
    int a = vCoder.encodeVideo(packet.data, packet.length, encodedBuffer_, UNPACKAGED_BUFFER_SIZE);
    if (a > 0)
      this->packageVideo(encodedBuffer_, a, packagedBuffer_);
  } else {
    // int a = this->encodeAudio(packet.data, packet.length, &pkt);
    // if (a > 0) {
    //   ELOG_DEBUG("GUAY a %d", a);
    // }
  }
  // av_free_packet(&pkt);
}

bool OutputProcessor::initAudioCoder() {
  aCoder = avcodec_find_encoder(static_cast<AVCodecID>(mediaInfo.audioCodec.codec));
  if (!aCoder) {
    ELOG_DEBUG("Encoder de audio no encontrado");
    return false;
  }

  aCoderContext = avcodec_alloc_context3(aCoder);
  if (!aCoderContext) {
    ELOG_DEBUG("Error de memoria en coder de audio");
    return false;
  }

  aCoderContext->sample_fmt = AV_SAMPLE_FMT_S16;
  aCoderContext->bit_rate = mediaInfo.audioCodec.bitRate;
  aCoderContext->sample_rate = mediaInfo.audioCodec.sampleRate;
  aCoderContext->channels = 1;

  if (avcodec_open2(aCoderContext, aCoder, NULL) < 0) {
    ELOG_DEBUG("Error al abrir el coder de audio");
    return false;
  }

  audioCoder = 1;
  return true;
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
    rtpHeader.setPayloadType(96);
    memcpy(rtpBuffer_, &rtpHeader, rtpHeader.getHeaderLength());
    memcpy(&rtpBuffer_[rtpHeader.getHeaderLength()], outBuff, outlen);

    int l = outlen + rtpHeader.getHeaderLength();
    rtpReceiver_->receiveRtpData(rtpBuffer_, l);
  } while (!lastFrame);

  return 0;
}

int OutputProcessor::encodeAudio(unsigned char* inBuff, int nSamples, AVPacket* pkt) {
  if (audioCoder == 0) {
    ELOG_DEBUG("No se han inicializado los parámetros del audioCoder");
    return -1;
  }

  AVFrame *frame = av_frame_alloc();
  if (!frame) {
    ELOG_ERROR("could not allocate audio frame");
    return -1;
  }
  int ret, got_output, buffer_size;
  // float t, tincr;

  frame->nb_samples = aCoderContext->frame_size;
  frame->format = aCoderContext->sample_fmt;
  // frame->channel_layout = aCoderContext->channel_layout;

  /* the codec gives us the frame size, in samples,
   * we calculate the size of the samples buffer in bytes */
  ELOG_DEBUG("channels %d, frame_size %d, sample_fmt %d",
      aCoderContext->channels, aCoderContext->frame_size,
      aCoderContext->sample_fmt);
  buffer_size = av_samples_get_buffer_size(NULL, aCoderContext->channels,
      aCoderContext->frame_size, aCoderContext->sample_fmt, 0);
  uint16_t* samples = reinterpret_cast<uint16_t*>(av_malloc(buffer_size));
  if (!samples) {
    ELOG_ERROR("could not allocate %d bytes for samples buffer",
        buffer_size);
    return -1;
  }
  /* setup the data pointers in the AVFrame */
  ret = avcodec_fill_audio_frame(frame, aCoderContext->channels,
      aCoderContext->sample_fmt, (const uint8_t*) samples, buffer_size,
      0);
  if (ret < 0) {
    ELOG_ERROR("could not setup audio frame");
    return ret;
  }

  ret = avcodec_encode_audio2(aCoderContext, pkt, frame, &got_output);
  if (ret < 0) {
    ELOG_ERROR("error encoding audio frame");
    return ret;
  }
  if (got_output) {
    // fwrite(pkt.data, 1, pkt.size, f);
    ELOG_DEBUG("Got OUTPUT");
  }
  return ret;
}
}  // namespace erizo
