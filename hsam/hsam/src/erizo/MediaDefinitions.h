/*
 * mediadefinitions.h
 *
 *  Created on: Mar 16, 2012
 *      Author: pedro
 */

#ifndef MEDIADEFINITIONS_H_
#define MEDIADEFINITIONS_H_

namespace erizo{

class NiceConnection;

struct MediaReceiver{

	virtual int receiveAudioData(char* buf, int len)=0;
	virtual int receiveVideoData(char* buf, int len)=0;
	virtual ~MediaReceiver(){};
};

struct NiceReceiver{

	virtual int receiveNiceData(char* buf, int len, NiceConnection* nice)=0;
	virtual ~NiceReceiver(){};
};

} /* namespace erizo */

#endif /* MEDIADEFINITIONS_H_ */
