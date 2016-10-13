#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sstream>
#include <fstream>

// Headers for SdpInfo.h tests
#include <SdpInfo.h>
#include <MediaDefinitions.h>

std::string readFile(std::ifstream& in) {
  std::stringstream sstr;
  sstr << in.rdbuf();
  return sstr.str();
}

static const char kChromeFingerprint[] =
      "58:8B:E5:05:5C:0F:B6:38:28:F9:DC:24:00:8F:E2:A5:52:B6:92:E7:58:38:53:6B:01:1A:12:7F:EF:55:78:6E";
static const char kFirefoxFingerprint[] =
      "C4:16:75:5C:5B:5F:E1:89:D7:EF:84:F7:40:B7:23:87:5F:A1:20:E0:F1:0F:89:B9:AB:87:62:17:80:E8:39:19";
static const char kOpenWebRtcFingerprint[] =
      "7D:EC:D1:14:01:01:2D:53:C3:61:EE:19:66:EA:34:8C:05:05:86:8E:5C:CD:FC:C0:37:AD:3F:7D:4F:07:A5:6C";

/*---------- SdpInfo TESTS ----------*/
TEST(erizoSdp, ChromeSdp) {
  erizo::SdpInfo sdp;
  std::ifstream ifs("Chrome.sdp", std::fstream::in);
  std::string sdpString = readFile(ifs);
  sdp.initWithSdp(sdpString, "video");
  // Check the mlines
  ASSERT_TRUE(sdp.hasVideo);
  ASSERT_TRUE(sdp.hasAudio);
  // Check the fingerprint
  ASSERT_EQ(sdp.fingerprint.compare(kChromeFingerprint), 0);
  // Check ICE Credentials
  std::string username = sdp.getUsername(erizo::VIDEO_TYPE);
  std::string password = sdp.getPassword(erizo::VIDEO_TYPE);
  ASSERT_EQ(username.compare("Bs0jL+c884dYG/oe"), 0);
  ASSERT_EQ(password.compare("ilq+r19kdvFsufkcyYAxoUM8"), 0);
}

TEST(erizoSdp, FirefoxSdp) {
  erizo::SdpInfo sdp;
  std::ifstream ifs("Firefox.sdp", std::fstream::in);
  std::string sdpString = readFile(ifs);
  sdp.initWithSdp(sdpString, "video");
  // Check the mlines
  ASSERT_EQ(sdp.hasVideo, true);
  ASSERT_EQ(sdp.hasAudio, true);
  // Check the fingerprint
  ASSERT_EQ(sdp.fingerprint.compare(kFirefoxFingerprint), 0);
  // Check ICE Credentials
  std::string username = sdp.getUsername(erizo::VIDEO_TYPE);
  std::string password = sdp.getPassword(erizo::VIDEO_TYPE);
  ASSERT_EQ(username.compare("b1239219"), 0);
  ASSERT_EQ(password.compare("b4ade8617fe94d5c800fdd085b86fd84"), 0);
}

TEST(erizoSdp, OpenWebRtc) {
  erizo::SdpInfo sdp;
  std::ifstream ifs("Openwebrtc.sdp", std::fstream::in);
  std::string sdpString = readFile(ifs);
  sdp.initWithSdp(sdpString, "video");
  // Check the mlines
  ASSERT_TRUE(sdp.hasVideo);
  ASSERT_TRUE(sdp.hasAudio);
  // Check the fingerprint
  ASSERT_EQ(sdp.fingerprint.compare(kOpenWebRtcFingerprint), 0);
  // Check ICE Credentials
  std::string username = sdp.getUsername(erizo::AUDIO_TYPE);
  std::string password = sdp.getPassword(erizo::AUDIO_TYPE);
  ASSERT_EQ(username.compare("mywn"), 0);
  ASSERT_EQ(password.compare("K+K88NukgWJ4EroPyZHPVA"), 0);
}
