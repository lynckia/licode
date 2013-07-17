#include <string>

#include "MediaProcessor.h"
#include "rtp/RtpVP8Fragmenter.h"
#include "rtp/RtpHeader.h"
#include "codecs/VideoCodec.h"

namespace erizo {

  InputProcessor::InputProcessor() {

    audioDecoder = 0;
    videoDecoder = 0;

    audioUnpackager = 1;
    videoUnpackager = 1;
    gotUnpackagedFrame_ = false;
    upackagedSize_ = 0;
    decodedBuffer_ = NULL;

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
      unpackagedBuffer_ = (unsigned char*) malloc(UNPACKAGED_BUFFER_SIZE);
      if(!vDecoder.initDecoder(mediaInfo.videoCodec));
        videoDecoder = 1; 
      if(!this->initVideoUnpackager());
    }
    if (mediaInfo.hasAudio) {
      printf("Init AUDIO processor\n");
      mediaInfo.audioCodec.codec = AUDIO_CODEC_PCM_U8;
      decodedAudioBuffer_ = (unsigned char*) malloc(UNPACKAGED_BUFFER_SIZE);
      unpackagedAudioBuffer_ = (unsigned char*) malloc(
          UNPACKAGED_BUFFER_SIZE);
      this->initAudioDecoder();
      this->initAudioUnpackager();
    }
    return 0;
  }

  int InputProcessor::deliverAudioData(char* buf, int len) {
    if (audioDecoder && audioUnpackager) {
      printf("Decoding audio\n");
      int unp = unpackageAudio((unsigned char*) buf, len,
          unpackagedAudioBuffer_);
      int a = decodeAudio(unpackagedAudioBuffer_, unp, decodedAudioBuffer_);
      printf("DECODED AUDIO a %d\n", a);
      RawDataPacket p;
      p.data = decodedAudioBuffer_;
      p.type = AUDIO;
      p.length = a;
      rawReceiver_->receiveRawData(p);
    }
  }
  int InputProcessor::deliverVideoData(char* buf, int len) {
    if (videoUnpackager && videoDecoder) {
      int ret = unpackageVideo(reinterpret_cast<unsigned char*>(buf), len,
          unpackagedBuffer_, &gotUnpackagedFrame_);
      if (ret < 0)
        return 0;
      upackagedSize_ += ret;
      unpackagedBuffer_ += ret;
      if (gotUnpackagedFrame_) {
        unpackagedBuffer_ -= upackagedSize_;
        printf("Tengo un frame desempaquetado!! Size = %d\n",
            upackagedSize_);
        int c;
        int gotDecodedFrame = 0;

        c = vDecoder.decodeVideo(unpackagedBuffer_, upackagedSize_,
            decodedBuffer_,
            mediaInfo.videoCodec.width * mediaInfo.videoCodec.height * 3
            / 2, &gotDecodedFrame);
        
        upackagedSize_ = 0;
        gotUnpackagedFrame_ = 0;
        printf("Bytes dec = %d\n", c);
        if (gotDecodedFrame && c > 0) {
          printf("Tengo un frame decodificado!!\n");
          gotDecodedFrame = 0;
          RawDataPacket p;
          p.data = decodedBuffer_;
          p.length = c;
          p.type = VIDEO;
          rawReceiver_->receiveRawData(p);
        }
      }
      return 1;
    }
    return 1;
  }

  bool InputProcessor::initAudioDecoder() {

    aDecoder = avcodec_find_decoder(static_cast<AVCodecID>(mediaInfo.audioCodec.codec));
    if (!aDecoder) {
      printf("Decoder de audio no encontrado");
      return false;
    }

    aDecoderContext = avcodec_alloc_context3(aDecoder);
    if (!aDecoderContext) {
      printf("Error de memoria en decoder de audio");
      return false;
    }

    aDecoderContext->sample_fmt = AV_SAMPLE_FMT_S16;
    aDecoderContext->bit_rate = mediaInfo.audioCodec.bitRate;
    aDecoderContext->sample_rate = mediaInfo.audioCodec.sampleRate;
    aDecoderContext->channels = 1;

    if (avcodec_open2(aDecoderContext, aDecoder, NULL) < 0) {
      printf("Error al abrir el decoder de audio\n");
      exit(0);
      return false;
    }
    printf("EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE %d\n", aDecoderContext->frame_size);
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

  int InputProcessor::decodeAudio(unsigned char* inBuff, int inBuffLen,
      unsigned char* outBuff) {

    if (audioDecoder == 0) {
      printf("No se han inicializado los parámetros del audioDecoder\n");
      return -1;
    }

    AVPacket avpkt;
    int outSize;
    int decSize = 0;
    int len = -1;
    uint8_t *decBuff = (uint8_t*) malloc(16000);

    av_init_packet(&avpkt);
    avpkt.data = (unsigned char*) inBuff;
    avpkt.size = inBuffLen;

    while (avpkt.size > 0) {

      outSize = 16000;

      //Puede fallar. Cogido de libavcodec/utils.c del paso de avcodec_decode_audio3 a avcodec_decode_audio4
      //avcodec_decode_audio3(aDecoderContext, (short*)decBuff, &outSize, &avpkt);

      AVFrame frame;
      int got_frame = 0;

//      aDecoderContext->get_buffer = avcodec_default_get_buffer;
//      aDecoderContext->release_buffer = avcodec_default_release_buffer;

      len = avcodec_decode_audio4(aDecoderContext, &frame, &got_frame,
          &avpkt);
      if (len >= 0 && got_frame) {
        int plane_size;
        //int planar = av_sample_fmt_is_planar(aDecoderContext->sample_fmt);
        int data_size = av_samples_get_buffer_size(&plane_size,
            aDecoderContext->channels, frame.nb_samples,
            aDecoderContext->sample_fmt, 1);
        if (outSize < data_size) {
          printf(
              "output buffer size is too small for the current frame\n");
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
        printf("Error al decodificar audio\n");
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
      printf("Error de decodificación de audio debido a tamaño incorrecto");
      return -1;
    }

    return decSize;

  }

  int InputProcessor::unpackageAudio(unsigned char* inBuff, int inBuffLen,
      unsigned char* outBuff) {

    int l = inBuffLen - RTPHeader::MIN_SIZE;
    memcpy(outBuff, &inBuff[RTPHeader::MIN_SIZE], l);

    return l;
  }

  int InputProcessor::unpackageVideo(unsigned char* inBuff, int inBuffLen,
      unsigned char* outBuff, int *gotFrame) {

    if (videoUnpackager == 0) {
      printf("Unpackager not correctly initialized");
      return -1;
    }

    int inBuffOffset = 0;
    *gotFrame = 0;
    RTPHeader* head = reinterpret_cast<RTPHeader*>(inBuff);


//    printf("PT %d, ssrc %u, extension %d\n", head->getPayloadType(), head->getSSRC(),
//        head->getExtension());
//    if ( head->getSSRC() != 55543 /*&& head->payloadtype!=101*/) {
//      return -1;
//    }
    if (head->getPayloadType() != 100) {
      return -1;
    }

//    printf("RTP header length: %d", head->getHeaderLength()); //Should include extensions
    int l = inBuffLen - head->getHeaderLength();
    inBuffOffset+=head->getHeaderLength();

    erizo::RTPPayloadVP8* parsed = pars.parseVP8(
        (unsigned char*) &inBuff[inBuffOffset], l);
    memcpy(outBuff, parsed->data, parsed->dataLength);
    if (head->getMarker()) {
      *gotFrame = 1;
    }
    return parsed->dataLength;

  }

  void InputProcessor::closeSink(){
    this->close();
  }

  void InputProcessor::close(){

    if (audioDecoder == 1) {
      avcodec_close(aDecoderContext);
      av_free(aDecoderContext);
      audioDecoder = 0;
    }

    if (videoDecoder == 1) {
      vDecoder.closeDecoder();      
      videoDecoder = 0;
    }
    if (decodedBuffer_ != NULL) {
      free(decodedBuffer_);
    }
  }

  OutputProcessor::OutputProcessor() {

    audioCoder = 0;
    videoCoder = 0;

    audioPackager = 0;
    videoPackager = 0;
    timestamp_ = 0;

    encodedBuffer_ = NULL;
    packagedBuffer_ = NULL;

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

    if (mediaInfo.hasVideo) {
      this->mediaInfo.videoCodec.codec = VIDEO_CODEC_VP8;
      if (vCoder.initEncoder(mediaInfo.videoCodec)) {
        printf("Error initing encoder\n");
      }
      this->initVideoPackager();
    }
    if (mediaInfo.hasAudio) {
      
      printf("Init AUDIO processor\n");
      mediaInfo.audioCodec.codec = AUDIO_CODEC_PCM_U8;
      mediaInfo.audioCodec.sampleRate= 44100;
      mediaInfo.audioCodec.bitRate = 64000;
      encodedAudioBuffer_ = (unsigned char*) malloc(UNPACKAGED_BUFFER_SIZE);
      packagedAudioBuffer_ = (unsigned char*) malloc(UNPACKAGED_BUFFER_SIZE);
      this->initAudioCoder();
      this->initAudioPackager();

    }

    return 0;
  }

  void OutputProcessor::close(){

    if (audioCoder == 1) {
      avcodec_close(aCoderContext);
      av_free(aCoderContext);
      audioCoder = 0;
    }

    if (videoCoder == 1) {
      vCoder.closeEncoder();
      videoCoder = 0;
    }
    if (encodedBuffer_) {
      free(encodedBuffer_);
    }
    if (packagedBuffer_) {
      free(packagedBuffer_);
    }
    if (rtpBuffer_) {
      free(rtpBuffer_);
    }
  }


  void OutputProcessor::receiveRawData(RawDataPacket& packet) {
    int hasFrame = 0;
    if (packet.type == VIDEO) {
//      printf("Encoding video: size %d\n", packet.length);
      int a = vCoder.encodeVideo(packet.data, packet.length, encodedBuffer_,UNPACKAGED_BUFFER_SIZE,hasFrame);
      if (a > 0)
        int b = this->packageVideo(encodedBuffer_, a, packagedBuffer_);
    } else {
//      int a = this->encodeAudio(packet.data, packet.length, &pkt);
//      if (a > 0) {
//        printf("GUAY a %d\n", a);
//      }

    }
//    av_free_packet(&pkt);
  }

  bool OutputProcessor::initAudioCoder() {

    aCoder = avcodec_find_encoder(static_cast<AVCodecID>(mediaInfo.audioCodec.codec));
    if (!aCoder) {
      printf("Encoder de audio no encontrado");
      exit(0);
      return false;
    }

    aCoderContext = avcodec_alloc_context3(aCoder);
    if (!aCoderContext) {
      printf("Error de memoria en coder de audio");
      exit(0);
      return false;
    }

    aCoderContext->sample_fmt = AV_SAMPLE_FMT_S16;
    aCoderContext->bit_rate = mediaInfo.audioCodec.bitRate;
    aCoderContext->sample_rate = mediaInfo.audioCodec.sampleRate;
    aCoderContext->channels = 1;

    if (avcodec_open2(aCoderContext, aCoder, NULL) < 0) {
      printf("Error al abrir el coder de audio");
      exit(0);
      return false;
    }

    audioCoder = 1;
    return true;
  }

  bool OutputProcessor::initAudioPackager() {
    audioPackager = 1;
    return true;
  }

  bool OutputProcessor::initVideoPackager() {
    seqnum_ = 0;
    videoPackager = 1;
    return true;
  }

  int OutputProcessor::packageAudio(unsigned char* inBuff, int inBuffLen,
      unsigned char* outBuff) {

    if (audioPackager == 0) {
      printf("No se ha inicializado el codec de output audio RTP");
      return -1;
    }


    timeval time;
    gettimeofday(&time, NULL);
    long millis = (time.tv_sec * 1000) + (time.tv_usec / 1000);

    RTPHeader head;
    head.setSeqNumber(seqnum_++);
    head.setTimestamp(millis*8);
    head.setSSRC(55543);
    head.setPayloadType(0);

    memcpy (rtpBuffer_, &head, head.getHeaderLength());
    memcpy(&rtpBuffer_[head.getHeaderLength()], inBuff, inBuffLen);
    //			sink_->sendData(rtpBuffer_, l);
    //	rtpReceiver_->receiveRtpData(rtpBuffer_, (inBuffLen + RTP_HEADER_LEN));
  }

  int OutputProcessor::packageVideo(unsigned char* inBuff, int buffSize, unsigned char* outBuff) {
    if (videoPackager == 0) {
      printf("No se ha inicailizado el codec de output vídeo RTP");
      return -1;
    }

//    printf("To packetize %u\n", buffSize);
    if (buffSize <= 0)
      return -1;
    RtpVP8Fragmenter frag(inBuff, buffSize, 1100);
    bool lastFrame = false;
    unsigned int outlen = 0;
    timeval time;
    gettimeofday(&time, NULL);
    long millis = (time.tv_sec * 1000) + (time.tv_usec / 1000);
    //		timestamp_ += 90000 / mediaInfo.videoCodec.frameRate;

    do {
      outlen = 0;
      frag.getPacket(outBuff, &outlen, &lastFrame);
      RTPHeader rtpHeader;
      rtpHeader.setMarker(lastFrame?1:0);
      rtpHeader.setSeqNumber(seqnum_++);
      rtpHeader.setTimestamp(millis*90);
      rtpHeader.setSSRC(55543);
      rtpHeader.setPayloadType(100);
      memcpy(rtpBuffer_, &rtpHeader, rtpHeader.getHeaderLength());
      memcpy(&rtpBuffer_[rtpHeader.getHeaderLength()],outBuff, outlen);

      int l = outlen + rtpHeader.getHeaderLength();
      //			sink_->sendData(rtpBuffer_, l);
      rtpReceiver_->receiveRtpData(rtpBuffer_, l);
    } while (!lastFrame);

    return 0;
  }

  int OutputProcessor::encodeAudio(unsigned char* inBuff, int nSamples,
      AVPacket* pkt) {

    if (audioCoder == 0) {
      printf("No se han inicializado los parámetros del audioCoder");
      return -1;
    }

    AVFrame *frame;
    /* frame containing input raw audio */
    frame = avcodec_alloc_frame();
    if (!frame) {
      fprintf(stderr, "could not allocate audio frame\n");
      exit(1);
    }
    uint16_t* samples;
    int ret, got_output, buffer_size;
    float t, tincr;

    frame->nb_samples = aCoderContext->frame_size;
    frame->format = aCoderContext->sample_fmt;
    //	frame->channel_layout = aCoderContext->channel_layout;

    /* the codec gives us the frame size, in samples,
     * we calculate the size of the samples buffer in bytes */
    printf("channels %d, frame_size %d, sample_fmt %d\n",
        aCoderContext->channels, aCoderContext->frame_size,
        aCoderContext->sample_fmt);
    buffer_size = av_samples_get_buffer_size(NULL, aCoderContext->channels,
        aCoderContext->frame_size, aCoderContext->sample_fmt, 0);
    samples = (uint16_t*) av_malloc(buffer_size);
    if (!samples) {
      fprintf(stderr, "could not allocate %d bytes for samples buffer\n",
          buffer_size);
      exit(1);
    }
    /* setup the data pointers in the AVFrame */
    ret = avcodec_fill_audio_frame(frame, aCoderContext->channels,
        aCoderContext->sample_fmt, (const uint8_t*) samples, buffer_size,
        0);
    if (ret < 0) {
      fprintf(stderr, "could not setup audio frame\n");
      exit(1);
    }

    ret = avcodec_encode_audio2(aCoderContext, pkt, frame, &got_output);
    if (ret < 0) {
      fprintf(stderr, "error encoding audio frame\n");
      exit(1);
    }
    if (got_output) {
      //fwrite(pkt.data, 1, pkt.size, f);
      printf("Got OUTPUT\n");
    }

    return ret;

  }

} /* namespace erizo */
