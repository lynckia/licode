#include <string.h>
#include <cstdio>
#include "RtpFragmenter.h"

#define MAX_SIZE 1180 //max size including vp8 payload descriptor
#define VP8 1
namespace erizo {

RtpFragmenter::RtpFragmenter(unsigned char* data, unsigned int length) :
		totalData_(data), totalLenth_(length) {
	calculatePackets();
	printf("Packets calculated successfully\n");
}

RtpFragmenter::~RtpFragmenter() {
}

int RtpFragmenter::getPacket(unsigned char* data, unsigned int* length,
		bool* lastPacket) {
	if (fragmentQueue_.size() > 0) {
		const Fragment& test = fragmentQueue_.front();
		printf("writing fragment, length %u\n", *length);

		*length = writeFragment(test, data, length);
		fragmentQueue_.pop();
		if (fragmentQueue_.empty())
			*lastPacket = true;

		printf("lastPacket %d\n", *lastPacket);

	}
	return 0;
}
void RtpFragmenter::calculatePackets() {
	unsigned int remaining = totalLenth_;
	unsigned int currentPos = 0;
	while (remaining > 0) {
		printf("Packetizing, remaining %u\n", remaining);
		Fragment newFragment;
		newFragment.first = false;
		newFragment.position = currentPos;
		if (currentPos == 0)
			newFragment.first = true;
		newFragment.size = remaining > MAX_SIZE - 1 ? MAX_SIZE - 1 : remaining;
		printf("New fragment size %u, position %u\n", newFragment.size,
				newFragment.position);
		currentPos += newFragment.size;
		remaining -= newFragment.size;
		fragmentQueue_.push(newFragment);
	}
}
unsigned int RtpFragmenter::writeFragment(const Fragment& fragment,
		unsigned char* buffer, unsigned int* length) {

	if (VP8) {
		buffer[0] = 0x0;
		if (fragment.first)
			buffer[0] |= 0x01; // S bit 1 // era 10
		memcpy(&buffer[1], &totalData_[fragment.position], fragment.size);
		return (fragment.size + 1);
	}else{
		memcpy(&buffer[0], &totalData_[fragment.position], fragment.size);
		return fragment.size;
	}
}

} /* namespace erizo */
