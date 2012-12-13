#ifndef EXTERNALINPUT_H_
#define EXTERNALINPUT_H_

#include <string> 
#include "codecs/VideoCodec.h"
#include "MediaProcessor.h"

namespace erizo{

  class ExternalInput : RTPDataReceiver {

    public:
      ExternalInput (const std::string& url);
      void init();
	    virtual void receiveRtpData(unsigned char* rtpdata, int len);
      virtual ~ExternalInput();
    private:
      OutputProcessor* op_;
      VideoDecoder* inCodec_;
      unsigned char* decodedBuffer_;
  };
}
#endif /* EXTERNALINPUT_H_ */
