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
	virtual int receiveAudioData(char* buf, int len)=0;
	virtual int receiveVideoData(char* buf, int len)=0;
	virtual ~MediaReceiver(){};
};

class NiceReceiver{
public:
	virtual int receiveNiceData(char* buf, int len, NiceConnection* nice)=0;
	virtual ~NiceReceiver(){};
};


#endif /* MEDIADEFINITIONS_H_ */
