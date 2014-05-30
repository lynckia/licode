/*
 * mediadefinitions.h
 */

#ifndef MEDIADEFINITIONS_H_
#define MEDIADEFINITIONS_H_
#include <boost/thread/mutex.hpp>

namespace erizo{

class NiceConnection;

enum packetType{
    VIDEO_PACKET,
    AUDIO_PACKET,
    OTHER_PACKET
};

struct dataPacket{
    int comp;
    char data[1500];
    int length;
    packetType type;
};

class Monitor {
protected:
    boost::mutex myMonitor_;
};

class FeedbackSink: public virtual Monitor{
public:
    virtual ~FeedbackSink() {}
    int deliverFeedback(char* buf, int len){
        boost::mutex::scoped_lock myMonitor_;
        return this->deliverFeedback_(buf,len);
    }
private:
    virtual int deliverFeedback_(char* buf, int len)=0;
};


class FeedbackSource: public virtual Monitor{
protected:
    FeedbackSink* fbSink_;
public:
    void setFeedbackSink(FeedbackSink* sink) {
        boost::mutex::scoped_lock myMonitor_;
        fbSink_ = sink;
    }
};

/*
 * A MediaSink
 */
class MediaSink: public virtual Monitor{
protected:
    //SSRCs received by the SINK
    unsigned int audioSinkSSRC_;
    unsigned int videoSinkSSRC_;
    //Is it able to provide Feedback
    FeedbackSource* sinkfbSource_;
public:
    int deliverAudioData(char* buf, int len){
        boost::mutex::scoped_lock myMonitor_;
        return this->deliverAudioData_(buf, len);
    }
    int deliverVideoData(char* buf, int len){
        boost::mutex::scoped_lock myMonitor_;
        return this->deliverVideoData_(buf, len);
    }
    unsigned int getVideoSinkSSRC (){
        boost::mutex::scoped_lock myMonitor_;
        return videoSinkSSRC_;
    }
    void setVideoSinkSSRC (unsigned int ssrc){
        boost::mutex::scoped_lock myMonitor_;
        videoSinkSSRC_ = ssrc;
    }
    unsigned int getAudioSinkSSRC (){
        boost::mutex::scoped_lock myMonitor_;
        return audioSinkSSRC_;
    }
    void setAudioSinkSSRC (unsigned int ssrc){
        boost::mutex::scoped_lock myMonitor_;
        audioSinkSSRC_ = ssrc;
    }
    FeedbackSource* getFeedbackSource(){
        boost::mutex::scoped_lock myMonitor_;
        return sinkfbSource_;
    }
    MediaSink() : audioSinkSSRC_(0), videoSinkSSRC_(0), sinkfbSource_(nullptr) {}
    virtual ~MediaSink() {}
private:
    virtual int deliverAudioData_(char* buf, int len)=0;
    virtual int deliverVideoData_(char* buf, int len)=0;

};


/**
 * A MediaSource is any class that produces audio or video data.
 */
class MediaSource: public virtual Monitor{
protected: 
    //SSRCs coming from the source
    unsigned int videoSourceSSRC_;
    unsigned int audioSourceSSRC_;
    MediaSink* videoSink_;
    MediaSink* audioSink_;
    //can it accept feedback
    FeedbackSink* sourcefbSink_;
public:
    void setAudioSink(MediaSink* audioSink){
        boost::mutex::scoped_lock myMonitor_;
        this->audioSink_ = audioSink;
    }
    void setVideoSink(MediaSink* videoSink){
        boost::mutex::scoped_lock myMonitor_;
        this->videoSink_ = videoSink;
    }

    FeedbackSink* getFeedbackSink(){
        boost::mutex::scoped_lock myMonitor_;
        return sourcefbSink_;
    }
    virtual int sendFirPacket()=0;
    unsigned int getVideoSourceSSRC (){
        boost::mutex::scoped_lock myMonitor_;
        return videoSourceSSRC_;
    }
    void setVideoSourceSSRC (unsigned int ssrc){
        boost::mutex::scoped_lock myMonitor_;
        videoSourceSSRC_ = ssrc;
    }
    unsigned int getAudioSourceSSRC (){
        boost::mutex::scoped_lock myMonitor_;
        return audioSourceSSRC_;
    }
    void setAudioSourceSSRC (unsigned int ssrc){
        boost::mutex::scoped_lock myMonitor_;
        audioSourceSSRC_ = ssrc;
    }
    virtual ~MediaSource(){}
};

/**
 * A NiceReceiver is any class that can receive data from a nice connection.
 */
class NiceReceiver{
public:
    virtual int receiveNiceData(char* buf, int len, NiceConnection* nice)=0;
    virtual ~NiceReceiver(){}
};

} /* namespace erizo */

#endif /* MEDIADEFINITIONS_H_ */
