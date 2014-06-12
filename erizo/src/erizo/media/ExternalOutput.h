#ifndef EXTERNALOUTPUT_H_
#define EXTERNALOUTPUT_H_

#include "../MediaDefinitions.h"
#include "rtp/RtpPacketQueue.h"
#include "MediaProcessor.h"
#include "boost/thread.hpp"
#include "logger.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace erizo{
#define UNPACKAGE_BUFFER_SIZE 200000
class WebRtcConnection;

class ExternalOutput : public MediaSink, public RawDataReceiver, public FeedbackSource {
    DECLARE_LOGGER();
public:
    ExternalOutput(const std::string& outputUrl);
    virtual ~ExternalOutput();
    bool init();
    void receiveRawData(RawDataPacket& packet);

private:
    RtpPacketQueue audioQueue_, videoQueue_;
    volatile bool recording_;
    boost::mutex mtx_;  // a mutex we use to signal our writer thread that data is waiting.
    boost::thread thread_;
    boost::condition_variable cond_;
    AVStream *video_stream_, *audio_stream_;

    unsigned long long lastFullIntraFrameRequest_;

    AVFormatContext *context_;
    InputProcessor *inputProcessor_;

    int unpackagedSize_;
    unsigned char* unpackagedBufferpart_;
    unsigned char deliverMediaBuffer_[3000];
    unsigned char unpackagedBuffer_[UNPACKAGE_BUFFER_SIZE];
    unsigned char unpackagedAudioBuffer_[UNPACKAGE_BUFFER_SIZE/10];
    long long initTimeVideo_;
    long long initTimeAudio_;


    bool initContext();
    int sendFirPacket();
    void queueData(char* buffer, int length, packetType type);
    void sendLoop();
    int deliverAudioData_(char* buf, int len);
    int deliverVideoData_(char* buf, int len);
    void writeAudioData(char* buf, int len);
    void writeVideoData(char* buf, int len);
};
}
#endif /* EXTERNALOUTPUT_H_ */
