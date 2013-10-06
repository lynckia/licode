#ifndef RTPFRAGMENTER_H_
#define RTPFRAGMENTER_H_

#include <queue>
#include "logger.h"

namespace erizo {

class RtpVP8Fragmenter {
	DECLARE_LOGGER();
public:
	RtpVP8Fragmenter(unsigned char* data, unsigned int length, unsigned int maxLength);
	virtual ~RtpVP8Fragmenter();

	int getPacket(unsigned char* data, unsigned int* length, bool* lastPacket);

private:
	struct Fragment {
		unsigned int position;
		unsigned int size;
		bool first;
	};
	void calculatePackets();
	unsigned int writeFragment(const Fragment& fragment, unsigned char* buffer,
			unsigned int* length);
	unsigned char* totalData_;
	unsigned int totalLenth_;
	unsigned int maxlength_;
	std::queue<Fragment> fragmentQueue_;
};

} /* namespace erizo */
#endif /* RTPFRAGMENTER_H_ */
