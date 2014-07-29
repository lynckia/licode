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
    bool recording_, inited_;
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

    // Timestamping strategy: we use the RTP timestamps so we don't have to restamp and we're not
    // subject to error due to the RTP packet queue depth and playout.
    //
    // However, the units of our audio and video RTP timestamps are not comparable because:
    //
    // 1. RTP timestamps start at a random value
    // 2. The units are completely different.  Video is typically on a 90khz clock, whereas audio
    //    typically follows whatever the sample rate is (e.g. 8khz for PCMU, 48khz for Opus, etc.)
    //
    // So, our strategy is to keep track of the first audio and video timestamps we encounter so we
    // can adjust subsequent timestamps by that much so our timestamps roughly start at zero.
    //
    // Audio and video can also start at different times, and it's possible video wouldn't even arrive
    // at all (or arrive late) so we also need to keep track of a start time offset.  We also need to track
    // this *before* stuff enters the RTP packet queue, since that guy will mess with the timing of stuff that's
    // outputted in an attempt to re-order incoming packets.  So when we receive an audio or video packet,
    // we set firstDataReceived_.  We then use that to compute audio/videoStartTimeOffset_ appropriately,
    // and that value is added to every timestamp we write.
    long long firstVideoTimestamp_;
    long long firstAudioTimestamp_;
    long long firstDataReceived_;
    long long videoOffsetMsec_;
    long long audioOffsetMsec_;


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
