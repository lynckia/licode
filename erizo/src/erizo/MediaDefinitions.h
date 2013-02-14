/*
 * mediadefinitions.h
 */

#ifndef MEDIADEFINITIONS_H_
#define MEDIADEFINITIONS_H_

namespace erizo{

class NiceConnection;

struct packet{
	char data[1200];
	int length;
};


class MediaSource;


class FeedbackReceiver{
public:
  virtual int receiveFeedback(char* buf, int len)=0;
};
/**
 * A MediaReceiver is any class that can receive audio or video data.
 */
class MediaSink{
private:
  //SSRCs received by the SINK
  unsigned int audioSinkSSRC_;
  unsigned int videoSinkSSRC_;
public:
	virtual int receiveAudioData(char* buf, int len)=0;
	virtual int receiveVideoData(char* buf, int len)=0;
  virtual void setFeedbackReceiver(MediaSource* source)=0;
  virtual unsigned int getVideoSinkSSRC (){ return videoSinkSSRC_;};
  virtual void setVideoSinkSSRC (unsigned int ssrc){ videoSinkSSRC_ = ssrc;};
  virtual unsigned int getAudioSinkSSRC (){ return audioSinkSSRC_;};
  virtual void setAudioSinkSSRC (unsigned int ssrc){ audioSinkSSRC_ = ssrc;};
  virtual void close()=0;
	virtual ~MediaSink(){};
};

/**
 * A MediaSource is any class that produces audio or video data.
 */
class MediaSource: public FeedbackReceiver{
private: 
  //SSRCs coming from the source
    unsigned int videoSourceSSRC_;
    unsigned int audioSourceSSRC_;
public:
  virtual void setAudioReceiver(MediaSink* audioReceiver)=0;
  virtual void setVideoReceiver(MediaSink* videoReceiver)=0;
  virtual int sendFirPacket()=0;
  virtual unsigned int getVideoSourceSSRC (){ return videoSourceSSRC_;};
  virtual void setVideoSourceSSRC (unsigned int ssrc){ videoSourceSSRC_ = ssrc;};
  virtual unsigned int getAudioSourceSSRC (){ return audioSourceSSRC_;};
  virtual void setAudioSourceSSRC (unsigned int ssrc){ audioSourceSSRC_ = ssrc;};
  virtual void close()=0;
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
