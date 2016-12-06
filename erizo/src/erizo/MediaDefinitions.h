/*
 * mediadefinitions.h
 */

#ifndef ERIZO_SRC_ERIZO_MEDIADEFINITIONS_H_
#define ERIZO_SRC_ERIZO_MEDIADEFINITIONS_H_
#include <boost/thread/mutex.hpp>

namespace erizo {

class NiceConnection;

enum packetType {
    VIDEO_PACKET,
    AUDIO_PACKET,
    OTHER_PACKET
};

struct dataPacket {
  dataPacket() = default;
  dataPacket(int comp_, const char *data_, int length_, packetType type_) :
    comp{comp_}, length{length_}, type{type_} {
      memcpy(data, data_, length_);
    }
  dataPacket(int comp_, const unsigned char *data_, int length_) :
    comp{comp_}, length{length_}, type{VIDEO_PACKET} {
      memcpy(data, data_, length_);
    }
  int comp;
  char data[1500];
  int length;
  packetType type;
};

class Monitor {
 protected:
    boost::mutex myMonitor_;
};

class FeedbackSink {
 public:
    virtual ~FeedbackSink() {}
    int deliverFeedback(char* buf, int len) {
        return this->deliverFeedback_(buf, len);
    }
 private:
    virtual int deliverFeedback_(char* buf, int len) = 0;
};


class FeedbackSource {
 protected:
    FeedbackSink* fbSink_;
 public:
    void setFeedbackSink(FeedbackSink* sink) {
        fbSink_ = sink;
    }
};

/*
 * A MediaSink
 */
class MediaSink: public virtual Monitor {
 protected:
    // SSRCs received by the SINK
    unsigned int audioSinkSSRC_;
    unsigned int videoSinkSSRC_;
    // Is it able to provide Feedback
    FeedbackSource* sinkfbSource_;

 public:
    int deliverAudioData(char* buf, int len) {
        return this->deliverAudioData_(buf, len);
    }
    int deliverVideoData(char* buf, int len) {
        return this->deliverVideoData_(buf, len);
    }
    unsigned int getVideoSinkSSRC() {
        boost::mutex::scoped_lock lock(myMonitor_);
        return videoSinkSSRC_;
    }
    void setVideoSinkSSRC(unsigned int ssrc) {
        boost::mutex::scoped_lock lock(myMonitor_);
        videoSinkSSRC_ = ssrc;
    }
    unsigned int getAudioSinkSSRC() {
        boost::mutex::scoped_lock lock(myMonitor_);
        return audioSinkSSRC_;
    }
    void setAudioSinkSSRC(unsigned int ssrc) {
        boost::mutex::scoped_lock lock(myMonitor_);
        audioSinkSSRC_ = ssrc;
    }
    FeedbackSource* getFeedbackSource() {
        boost::mutex::scoped_lock lock(myMonitor_);
        return sinkfbSource_;
    }
    MediaSink() : audioSinkSSRC_(0), videoSinkSSRC_(0), sinkfbSource_(NULL) {}
    virtual ~MediaSink() {}

    virtual void close() = 0;

 private:
    virtual int deliverAudioData_(char* buf, int len) = 0;
    virtual int deliverVideoData_(char* buf, int len) = 0;
};

/**
 * A MediaSource is any class that produces audio or video data.
 */
class MediaSource: public virtual Monitor {
 protected:
    // SSRCs coming from the source
    unsigned int videoSourceSSRC_;
    unsigned int audioSourceSSRC_;
    MediaSink* videoSink_;
    MediaSink* audioSink_;
    // can it accept feedback
    FeedbackSink* sourcefbSink_;

 public:
    void setAudioSink(MediaSink* audioSink) {
        boost::mutex::scoped_lock lock(myMonitor_);
        this->audioSink_ = audioSink;
    }
    void setVideoSink(MediaSink* videoSink) {
        boost::mutex::scoped_lock lock(myMonitor_);
        this->videoSink_ = videoSink;
    }

    FeedbackSink* getFeedbackSink() {
        boost::mutex::scoped_lock lock(myMonitor_);
        return sourcefbSink_;
    }
    virtual int sendPLI() = 0;
    unsigned int getVideoSourceSSRC() {
        boost::mutex::scoped_lock lock(myMonitor_);
        return videoSourceSSRC_;
    }
    void setVideoSourceSSRC(unsigned int ssrc) {
        boost::mutex::scoped_lock lock(myMonitor_);
        videoSourceSSRC_ = ssrc;
    }
    unsigned int getAudioSourceSSRC() {
        boost::mutex::scoped_lock lock(myMonitor_);
        return audioSourceSSRC_;
    }
    void setAudioSourceSSRC(unsigned int ssrc) {
        boost::mutex::scoped_lock lock(myMonitor_);
        audioSourceSSRC_ = ssrc;
    }
    virtual ~MediaSource() {}

    virtual void close() = 0;
};

/**
 * A NiceReceiver is any class that can receive data from a nice connection.
 */
class NiceReceiver {
 public:
    virtual int receiveNiceData(char* buf, int len, NiceConnection* nice) = 0;
    virtual ~NiceReceiver() {}
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_MEDIADEFINITIONS_H_
