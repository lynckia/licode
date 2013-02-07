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

/**
 * A MediaReceiver is any class that can receive audio or video data.
 */
class MediaReceiver{
protected:
  unsigned int receiverSSRC_;
public:
	virtual int receiveAudioData(char* buf, int len)=0;
	virtual int receiveVideoData(char* buf, int len)=0;
  virtual void close()=0;
	virtual ~MediaReceiver(){};
};

/**
 * A MediaSource is any class that produces audio or video data.
 */
class MediaSource{
protected: 
    unsigned int sourceSSRC_;
public:
  virtual void setAudioReceiver(MediaReceiver* audioReceiver)=0;
  virtual void setVideoReceiver(MediaReceiver* videoReceiver)=0;
  virtual int receiveRTCP(char* buf, int len)=0;
  virtual int sendFirPacket();
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
