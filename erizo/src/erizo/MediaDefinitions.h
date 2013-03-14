/*
 * mediadefinitions.h
 */

#ifndef MEDIADEFINITIONS_H_
#define MEDIADEFINITIONS_H_

namespace erizo{

class NiceConnection;

enum packetType{
  VIDEO_PACKET,
  AUDIO_PACKET,
  OTHER_PACKET    
};

struct dataPacket{
	char data[1200];
	int length;
  packetType type;
};

class FeedbackSink{
public:
  virtual int deliverFeedback(char* buf, int len)=0;
};

class FeedbackSource{
protected:
  FeedbackSink* fbSink_;
public:
  virtual void setFeedbackSink(FeedbackSink* sink){
    fbSink_ = sink;
  };

};

/*
 * A MediaSink 
 */
class MediaSink{
protected:
  //SSRCs received by the SINK
  unsigned int audioSinkSSRC_;
  unsigned int videoSinkSSRC_;
  //Is it able to provide Feedback
  FeedbackSource* sinkfbSource_;
public:
	virtual int deliverAudioData(char* buf, int len)=0;
	virtual int deliverVideoData(char* buf, int len)=0;
  unsigned int getVideoSinkSSRC (){return videoSinkSSRC_;};
  void setVideoSinkSSRC (unsigned int ssrc){videoSinkSSRC_ = ssrc;};
  unsigned int getAudioSinkSSRC (){return audioSinkSSRC_;};
  void setAudioSinkSSRC (unsigned int ssrc){audioSinkSSRC_ = ssrc;};
  FeedbackSource* getFeedbackSource(){
    return sinkfbSource_;
  };
  virtual void closeSink()=0;
	virtual ~MediaSink(){};
};

/**
 * A MediaSource is any class that produces audio or video data.
 */
class MediaSource{
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
    this->audioSink_ = audioSink;
  };
  void setVideoSink(MediaSink* videoSink){
    this->videoSink_ = videoSink;
  };

  FeedbackSink* getFeedbackSink(){
    return sourcefbSink_;
  };
  virtual int sendFirPacket()=0;
  unsigned int getVideoSourceSSRC (){return videoSourceSSRC_;};
  void setVideoSourceSSRC (unsigned int ssrc){videoSourceSSRC_ = ssrc;};
  unsigned int getAudioSourceSSRC (){return audioSourceSSRC_;};
  void setAudioSourceSSRC (unsigned int ssrc){audioSourceSSRC_ = ssrc;};
  virtual void closeSource()=0;
	virtual ~MediaSource(){};
};

/**
 * A NiceReceiver is any class that can receive data from a nice connection.
 */
class NiceReceiver{
public:
	virtual int receiveNiceData(char* buf, int len, NiceConnection* nice)=0;
	virtual ~NiceReceiver(){};
};

} /* namespace erizo */

#endif /* MEDIADEFINITIONS_H_ */
