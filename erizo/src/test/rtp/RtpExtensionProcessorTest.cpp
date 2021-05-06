#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <rtp/RtpExtensionProcessor.h>

#include <atomic>
#include <chrono>  // NOLINT
#include <string>
#include <vector>
#include <thread>  // NOLINT
#include <condition_variable>  // NOLINT

#include "../utils/Mocks.h"
#include "../utils/Tools.h"

using testing::Eq;
using testing::Not;

class RtpExtensionProcessorTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    ext_mappings.push_back({1, "urn:ietf:params:rtp-hdrext:ssrc-audio-level"});
  }
  virtual void TearDown() {
  }

  std::vector<erizo::ExtMap> ext_mappings;
};

TEST_F(RtpExtensionProcessorTest, extensionShouldBeValidWhenItIsPassed) {
  erizo::RtpExtensionProcessor processor(ext_mappings);

  bool is_valid = processor.isValidExtension("urn:ietf:params:rtp-hdrext:ssrc-audio-level");

  EXPECT_THAT(is_valid, Eq(true));
}

TEST_F(RtpExtensionProcessorTest, extensionShouldBeInvalidWhenItIsNotPassed) {
  erizo::RtpExtensionProcessor processor(ext_mappings);

  bool is_valid = processor.isValidExtension("http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time");

  EXPECT_THAT(is_valid, Eq(false));
}

TEST_F(RtpExtensionProcessorTest, extensionShouldBeInvalidWhenWeDoNotKnowIt) {
  erizo::RtpExtensionProcessor processor(ext_mappings);

  bool is_valid = processor.isValidExtension("unknown");

  EXPECT_THAT(is_valid, Eq(false));
}

TEST_F(RtpExtensionProcessorTest, shouldResetMidExtensionDataFromPacket) {
  ext_mappings.push_back({erizo::MID, "urn:ietf:params:rtp-hdrext:sdes:mid"});
  ext_mappings.push_back({erizo::VIDEO_ORIENTATION, "urn:3gpp:video-orientation"});
  erizo::RtpExtensionProcessor processor(ext_mappings);
  processor.setExtension(erizo::VIDEO_TYPE, erizo::MID, erizo::MID);
  processor.setExtension(erizo::VIDEO_TYPE, erizo::VIDEO_ORIENTATION, erizo::VIDEO_ORIENTATION);

  char arbitrary_extension_data = 0x30;

  const auto pkt = erizo::PacketTools::createVP8PacketWithExtensions(erizo::VIDEO_ORIENTATION, arbitrary_extension_data,
    erizo::MID, arbitrary_extension_data);

  erizo::RtpHeader* h = reinterpret_cast<erizo::RtpHeader*>(pkt->data);
  char* data_pointer = pkt->data + h->getHeaderLength();
  char* extension = (char*)&h->extensions;  // NOLINT

  EXPECT_THAT(extension[0], Eq(static_cast<char>(erizo::VIDEO_ORIENTATION << 4)));  // Extension Id = MID(8), Length = 0
  EXPECT_THAT(extension[1], Eq(arbitrary_extension_data));  // Arbitrary extension data
  EXPECT_THAT(extension[2], Eq(static_cast<char>(erizo::MID << 4)));  // Extension Id = MID(8), Length = 0
  EXPECT_THAT(extension[3], Eq(arbitrary_extension_data));  // Arbitrary extension data
  EXPECT_THAT(data_pointer[0], Eq(0x10));

  processor.removeMidAndRidExtensions(pkt);

  EXPECT_THAT(extension[0], Eq(static_cast<char>(erizo::VIDEO_ORIENTATION << 4)));  // Extension Id = MID(8), Length = 0
  EXPECT_THAT(extension[1], Eq(arbitrary_extension_data));  // Arbitrary extension data
  EXPECT_THAT(extension[2], Eq(0));  // Extension Id = 0, Length = 0
  EXPECT_THAT(extension[3], Eq(0));  // Extension Data = 0
  EXPECT_THAT(data_pointer[0], Eq(0x10));
}

TEST_F(RtpExtensionProcessorTest, shouldResetMidExtensionDataFromPacket2) {
  ext_mappings.push_back({erizo::MID, "urn:ietf:params:rtp-hdrext:sdes:mid"});
  ext_mappings.push_back({erizo::VIDEO_ORIENTATION, "urn:3gpp:video-orientation"});
  erizo::RtpExtensionProcessor processor(ext_mappings);
  processor.setExtension(erizo::VIDEO_TYPE, erizo::MID, erizo::MID);
  processor.setExtension(erizo::VIDEO_TYPE, erizo::VIDEO_ORIENTATION, erizo::VIDEO_ORIENTATION);

  char arbitrary_extension_data = 0x30;

  const auto pkt = erizo::PacketTools::createVP8PacketWithExtensions(erizo::MID, arbitrary_extension_data,
    erizo::VIDEO_ORIENTATION, arbitrary_extension_data);

  erizo::RtpHeader* h = reinterpret_cast<erizo::RtpHeader*>(pkt->data);
  char* data_pointer = pkt->data + h->getHeaderLength();
  char* extension = (char*)&h->extensions;  // NOLINT


  EXPECT_THAT(extension[0], Eq(static_cast<char>(erizo::MID << 4)));  // Extension Id = MID(8), Length = 0
  EXPECT_THAT(extension[1], Eq(arbitrary_extension_data));  // Arbitrary extension data
  EXPECT_THAT(extension[2], Eq(static_cast<char>(erizo::VIDEO_ORIENTATION << 4)));  // Extension Id = MID(8), Length = 0
  EXPECT_THAT(extension[3], Eq(arbitrary_extension_data));  // Arbitrary extension data
  EXPECT_THAT(data_pointer[0], Eq(0x10));

  processor.removeMidAndRidExtensions(pkt);

  EXPECT_THAT(extension[0], Eq(0));  // Extension Id = 0, Length = 0
  EXPECT_THAT(extension[1], Eq(0));  // Extension Data = 0
  EXPECT_THAT(extension[2], Eq(static_cast<char>(erizo::VIDEO_ORIENTATION << 4)));  // Extension Id = MID(8), Length = 0
  EXPECT_THAT(extension[3], Eq(arbitrary_extension_data));  // Arbitrary extension data

  EXPECT_THAT(data_pointer[0], Eq(0x10));
}

TEST_F(RtpExtensionProcessorTest, shouldResetRidExtensionDataFromPacket) {
  ext_mappings.push_back({erizo::RTP_ID, "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id"});
  ext_mappings.push_back({erizo::VIDEO_ORIENTATION, "urn:3gpp:video-orientation"});
  erizo::RtpExtensionProcessor processor(ext_mappings);
  processor.setExtension(erizo::VIDEO_TYPE, erizo::RTP_ID, erizo::RTP_ID);
  processor.setExtension(erizo::VIDEO_TYPE, erizo::VIDEO_ORIENTATION, erizo::VIDEO_ORIENTATION);

  char arbitrary_extension_data = 0x30;

  const auto pkt = erizo::PacketTools::createVP8PacketWithExtensions(erizo::VIDEO_ORIENTATION, arbitrary_extension_data,
    erizo::RTP_ID, arbitrary_extension_data);

  erizo::RtpHeader* h = reinterpret_cast<erizo::RtpHeader*>(pkt->data);
  char* data_pointer = pkt->data + h->getHeaderLength();
  char* extension = (char*)&h->extensions;  // NOLINT

  EXPECT_THAT(extension[0], Eq(static_cast<char>(erizo::VIDEO_ORIENTATION << 4)));  // Extension Id = RID(8), Length = 0
  EXPECT_THAT(extension[1], Eq(arbitrary_extension_data));  // Arbitrary extension data
  EXPECT_THAT(extension[2], Eq(static_cast<char>(erizo::RTP_ID << 4)));  // Extension Id = RID(8), Length = 0
  EXPECT_THAT(extension[3], Eq(arbitrary_extension_data));  // Arbitrary extension data
  EXPECT_THAT(data_pointer[0], Eq(0x10));

  processor.removeMidAndRidExtensions(pkt);

  EXPECT_THAT(extension[0], Eq(static_cast<char>(erizo::VIDEO_ORIENTATION << 4)));  // Extension Id = RID(8), Length = 0
  EXPECT_THAT(extension[1], Eq(arbitrary_extension_data));  // Arbitrary extension data
  EXPECT_THAT(extension[2], Eq(0));  // Extension Id = 0, Length = 0
  EXPECT_THAT(extension[3], Eq(0));  // Extension Data = 0

  EXPECT_THAT(data_pointer[0], Eq(0x10));
}

TEST_F(RtpExtensionProcessorTest, shouldResetRidExtensionDataFromPacket2) {
  ext_mappings.push_back({erizo::RTP_ID, "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id"});
  ext_mappings.push_back({erizo::VIDEO_ORIENTATION, "urn:3gpp:video-orientation"});
  erizo::RtpExtensionProcessor processor(ext_mappings);
  processor.setExtension(erizo::VIDEO_TYPE, erizo::RTP_ID, erizo::RTP_ID);
  processor.setExtension(erizo::VIDEO_TYPE, erizo::VIDEO_ORIENTATION, erizo::VIDEO_ORIENTATION);

  char arbitrary_extension_data = 0x30;

  const auto pkt = erizo::PacketTools::createVP8PacketWithExtensions(erizo::RTP_ID, arbitrary_extension_data,
    erizo::VIDEO_ORIENTATION, arbitrary_extension_data);

  erizo::RtpHeader* h = reinterpret_cast<erizo::RtpHeader*>(pkt->data);
  char* data_pointer = pkt->data + h->getHeaderLength();
  char* extension = (char*)&h->extensions;  // NOLINT


  EXPECT_THAT(extension[0], Eq(static_cast<char>(erizo::RTP_ID << 4)));  // Extension Id = RID(8), Length = 0
  EXPECT_THAT(extension[1], Eq(arbitrary_extension_data));  // Arbitrary extension data
  EXPECT_THAT(extension[2], Eq(static_cast<char>(erizo::VIDEO_ORIENTATION << 4)));  // Extension Id = RID(8), Length = 0
  EXPECT_THAT(extension[3], Eq(arbitrary_extension_data));  // Arbitrary extension data
  EXPECT_THAT(data_pointer[0], Eq(0x10));

  processor.removeMidAndRidExtensions(pkt);

  EXPECT_THAT(extension[0], Eq(0));  // Extension Id = 0, Length = 0
  EXPECT_THAT(extension[1], Eq(0));  // Extension Data = 0
  EXPECT_THAT(extension[2], Eq(static_cast<char>(erizo::VIDEO_ORIENTATION << 4)));  // Extension Id = RID(8), Length = 0
  EXPECT_THAT(extension[3], Eq(arbitrary_extension_data));  // Arbitrary extension data

  EXPECT_THAT(data_pointer[0], Eq(0x10));
}

TEST_F(RtpExtensionProcessorTest, shouldResetMidAndRidExtensionDataFromPacket) {
  ext_mappings.push_back({erizo::RTP_ID, "urn:ietf:params:rtp-hdrext:sdes:rtp-stream-id"});
  ext_mappings.push_back({erizo::MID, "urn:ietf:params:rtp-hdrext:sdes:mid"});
  erizo::RtpExtensionProcessor processor(ext_mappings);
  processor.setExtension(erizo::VIDEO_TYPE, erizo::RTP_ID, erizo::RTP_ID);
  processor.setExtension(erizo::VIDEO_TYPE, erizo::MID, erizo::MID);

  char arbitrary_extension_data = 0x30;

  const auto pkt = erizo::PacketTools::createVP8PacketWithExtensions(erizo::RTP_ID, arbitrary_extension_data,
    erizo::MID, arbitrary_extension_data);

  erizo::RtpHeader* h = reinterpret_cast<erizo::RtpHeader*>(pkt->data);
  char* data_pointer = pkt->data + h->getHeaderLength();
  char* extension = (char*)&h->extensions;  // NOLINT


  EXPECT_THAT(extension[0], Eq(static_cast<char>(erizo::RTP_ID << 4)));  // Extension Id = RID(8), Length = 0
  EXPECT_THAT(extension[1], Eq(arbitrary_extension_data));  // Arbitrary extension data
  EXPECT_THAT(extension[2], Eq(static_cast<char>(erizo::MID << 4)));  // Extension Id = RID(8), Length = 0
  EXPECT_THAT(extension[3], Eq(arbitrary_extension_data));  // Arbitrary extension data
  EXPECT_THAT(data_pointer[0], Eq(0x10));

  processor.removeMidAndRidExtensions(pkt);

  EXPECT_THAT(extension[0], Eq(0));  // Extension Id = 0, Length = 0
  EXPECT_THAT(extension[1], Eq(0));  // Extension Data = 0
  EXPECT_THAT(extension[0], Eq(0));  // Extension Id = 0, Length = 0
  EXPECT_THAT(extension[1], Eq(0));  // Extension Data = 0

  EXPECT_THAT(data_pointer[0], Eq(0x10));
}
