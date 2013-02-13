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
/**
 * A MediaReceiver is any class that can receive audio or video data.
 */
class MediaSink{
protected:
  unsigned int audioSinkSSRC_;
  unsigned int videoSinkSSRC_;
public:
	virtual int receiveAudioData(char* buf, int len)=0;
	virtual int receiveVideoData(char* buf, int len)=0;
  virtual void setFeedbackReceiver(MediaSource* source)=0;
  virtual unsigned int getVideoSinkSSRC (){ return videoSinkSSRC_;};
  virtual unsigned int setVideoSinkSSRC (unsigned int ssrc){ videoSinkSSRC_ = ssrc;};
  virtual unsigned int getAudioSinkSSRC (){ return audioSinkSSRC_;};
  virtual unsigned int setAudioSinkSSRC (unsigned int ssrc){ audioSinkSSRC_ = ssrc;};
  virtual void close()=0;
	virtual ~MediaSink(){};
};

/**
 * A MediaSource is any class that produces audio or video data.
 */
class MediaSource{
protected: 
    unsigned int videoSourceSSRC_;
    unsigned int audioSourceSSRC_;
public:
  virtual void setAudioReceiver(MediaSink* audioReceiver)=0;
  virtual void setVideoReceiver(MediaSink* videoReceiver)=0;
  virtual int receiveFeedback(char* buf, int len)=0;
  virtual int sendFirPacket()=0;
  virtual unsigned int getVideoSourceSSRC (){ return videoSourceSSRC_;};
  virtual unsigned int setVideoSourceSSRC (unsigned int ssrc){ videoSourceSSRC_ = ssrc;};
  virtual unsigned int getAudioSourceSSRC (){ return audioSourceSSRC_;};
  virtual unsigned int setAudioSourceSSRC (unsigned int ssrc){ audioSourceSSRC_ = ssrc;};
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
