#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <rtp/RtpExtensionProcessor.h>

#include <atomic>
#include <chrono>  // NOLINT
#include <string>
#include <vector>
#include <thread>  // NOLINT
#include <condition_variable>  // NOLINT

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
