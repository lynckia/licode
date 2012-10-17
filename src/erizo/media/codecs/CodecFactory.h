/*
 * CodecFactory.h
 */
#ifndef CODECFACTORY_H_
#define CODECFACTORY_H_

#include <stdint.h>

namespace erizo{

  enum VideoCodecID{
    VIDEO_CODEC_VP8,
    VIDEO_CODEC_H264
  };

  enum AudioCodecID{
    AUDIO_CODEC_PCM_MULAW_8
  };

 struct VideoCodecInfo {
    VideoCodecID codec;
    int payloadType;
    int width;
    int height;
    int bitRate;
    int frameRate;
  };

  struct AudioCodecInfo {
    AudioCodecID codec;
    int bitRate;
    int sampleRate;
  };

  class CodecFactory{
    public:
      static CodecFactory* getInstance();
    protected:
      CodecFactory();
    private:
      static CodecFactory* theInstance;
  };

}
#endif /* CODECFACTORY_H_ */
