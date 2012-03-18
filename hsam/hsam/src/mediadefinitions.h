/*
 * mediadefinitions.h
 *
 *  Created on: Mar 16, 2012
 *      Author: pedro
 */

#ifndef MEDIADEFINITIONS_H_
#define MEDIADEFINITIONS_H_
class NiceConnection;

class MediaReceiver{
public:
	virtual int receiveAudioData(void* buf, int len)=0;
	virtual int receiveVideoData(void* buf, int len)=0;

	virtual ~MediaReceiver(){};
};

class NiceReceiver{
public:
	virtual int receiveAudioNiceData(void* buf, int len, NiceConnection* nice)=0;
	virtual int receiveVideoNiceData(void* buf, int len, NiceConnection* nice)=0;

	virtual ~NiceReceiver(){};
};


#endif /* MEDIADEFINITIONS_H_ */
