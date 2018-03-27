#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sstream>
#include <fstream>
#include <algorithm>

// Headers for SdpInfo.h tests
#include <SdpInfo.h>
#include <MediaDefinitions.h>

using erizo::SdpInfo;
using erizo::RtpMap;
using erizo::Rid;
using erizo::RidDirection;

std::string readFile(std::ifstream& in) {
  std::stringstream sstr;
  sstr << in.rdbuf();
  return sstr.str();
}

class SdpInfoTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    erizo::RtpMap vp8;
    vp8.payload_type = 100;
    vp8.encoding_name = "VP8";
    vp8.clock_rate = 90000;
    vp8.channels = 1;
    vp8.media_type = erizo::VIDEO_TYPE;
    rtp_mappings.push_back(vp8);

    erizo::RtpMap red;
    red.payload_type = 110;
    red.encoding_name = "red";
    red.clock_rate = 90000;
    red.channels = 1;
    red.media_type = erizo::VIDEO_TYPE;
    rtp_mappings.push_back(red);

    erizo::RtpMap pcmu;
    pcmu.payload_type = 0;
    pcmu.encoding_name = "PCMU";
    pcmu.clock_rate = 8000;
    pcmu.channels = 1;
    pcmu.media_type = erizo::AUDIO_TYPE;

    rtp_mappings.push_back(pcmu);
    sdp.reset(new erizo::SdpInfo(rtp_mappings));
  }

  virtual void TearDown() {}

  std::vector<erizo::RtpMap> rtp_mappings;
  std::shared_ptr<erizo::SdpInfo> sdp;

  const std::string kChromeFingerprint =
    "22:E9:DE:F0:21:6C:6A:AC:9F:A1:0E:00:78:DC:67:9F:87:16:4C:EE:95:DE:DF:A3:66:AF:AD:5F:8A:46:CE:BB";
  const std::string kFirefoxFingerprint =
    "C4:16:75:5C:5B:5F:E1:89:D7:EF:84:F7:40:B7:23:87:5F:A1:20:E0:F1:0F:89:B9:AB:87:62:17:80:E8:39:19";
  const std::string kOpenWebRtcFingerprint =
    "7D:EC:D1:14:01:01:2D:53:C3:61:EE:19:66:EA:34:8C:05:05:86:8E:5C:CD:FC:C0:37:AD:3F:7D:4F:07:A5:6C";
};

class SdpInfoMediaTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    std::ifstream ifs("Chrome.sdp", std::fstream::in);
    chrome_sdp_string = readFile(ifs);
  }

  virtual void TearDown() {}
  std::string chrome_sdp_string;
  std::vector<erizo::RtpMap> rtp_mappings;
};


/*---------- SdpInfo TESTS ----------*/
TEST_F(SdpInfoTest, shouldParseIceCredentials_Chrome) {
  std::ifstream ifs("Chrome.sdp", std::fstream::in);
  std::string sdp_string = readFile(ifs);
  const std::string kUserNameFromSdpFile = "I+d3";
  const std::string kPassFromSdpFile =  "010TmRLizTQVHQ/PvsoyOh4a";

  sdp->initWithSdp(sdp_string, "video");
  // Check the mlines
  EXPECT_TRUE(sdp->hasVideo);
  EXPECT_TRUE(sdp->hasAudio);
  // Check the fingerprint
  EXPECT_EQ(sdp->fingerprint, kChromeFingerprint);
  // Check ICE Credentials
  std::string username = sdp->getUsername(erizo::VIDEO_TYPE);
  std::string password = sdp->getPassword(erizo::VIDEO_TYPE);
  EXPECT_EQ(username, kUserNameFromSdpFile);
  EXPECT_EQ(password, kPassFromSdpFile);
}

TEST_F(SdpInfoTest, shouldParseIceCredentials_Firefox) {
  std::ifstream ifs("Firefox.sdp", std::fstream::in);
  std::string sdp_string = readFile(ifs);
  const std::string kUserNameFromSdpFile = "b1239219";
  const std::string kPassFromSdpFile =  "b4ade8617fe94d5c800fdd085b86fd84";

  sdp->initWithSdp(sdp_string, "video");
  // Check the mlines
  EXPECT_EQ(sdp->hasVideo, true);
  EXPECT_EQ(sdp->hasAudio, true);
  // Check the fingerprint
  EXPECT_EQ(sdp->fingerprint, kFirefoxFingerprint);
  // Check ICE Credentials
  std::string username = sdp->getUsername(erizo::VIDEO_TYPE);
  std::string password = sdp->getPassword(erizo::VIDEO_TYPE);
  EXPECT_EQ(username, kUserNameFromSdpFile);
  EXPECT_EQ(password, kPassFromSdpFile);
}

TEST_F(SdpInfoTest, shouldParseIceCredentials_OpenWebRTC) {
  std::ifstream ifs("Openwebrtc.sdp", std::fstream::in);
  std::string sdp_string = readFile(ifs);
  const std::string kUserNameFromSdpFile = "mywn";
  const std::string kPassFromSdpFile = "K+K88NukgWJ4EroPyZHPVA";

  sdp->initWithSdp(sdp_string, "video");
  // Check the mlines
  EXPECT_TRUE(sdp->hasVideo);
  EXPECT_TRUE(sdp->hasAudio);
  // Check the fingerprint
  EXPECT_EQ(sdp->fingerprint, kOpenWebRtcFingerprint);
  // Check ICE Credentials
  std::string username = sdp->getUsername(erizo::AUDIO_TYPE);
  std::string password = sdp->getPassword(erizo::AUDIO_TYPE);
  EXPECT_EQ(username, kUserNameFromSdpFile);
  EXPECT_EQ(password, kPassFromSdpFile);
}

TEST_F(SdpInfoMediaTest, shouldDiscardNotSupportedCodecs) {
  const std::string kArbitraryCodecName = "opus";
  const unsigned int kArbitraryClockRate = 48000;

  erizo::SdpInfo sdp(rtp_mappings);
  sdp.initWithSdp(chrome_sdp_string, "video");
  EXPECT_FALSE(sdp.supportCodecByName(kArbitraryCodecName, kArbitraryClockRate));
}

TEST_F(SdpInfoMediaTest, shouldStoreSupportedCodecs) {
  erizo::RtpMap codec_exists_in_file;
  codec_exists_in_file.payload_type = 0;
  codec_exists_in_file.encoding_name = "PCMU";
  codec_exists_in_file.clock_rate = 8000;
  codec_exists_in_file.channels = 1;
  codec_exists_in_file.media_type = erizo::AUDIO_TYPE;
  rtp_mappings.push_back(codec_exists_in_file);

  erizo::SdpInfo sdp(rtp_mappings);
  sdp.initWithSdp(chrome_sdp_string, "video");
  EXPECT_TRUE(sdp.supportCodecByName("PCMU", 8000));
}

TEST_F(SdpInfoMediaTest, shouldStoreSupportedFeedback) {
  const std::string kFeedbackExistsInFile = "ccm fi";
  erizo::RtpMap codec_exists_in_file;
  codec_exists_in_file.payload_type = 0;
  codec_exists_in_file.encoding_name = "VP8";
  codec_exists_in_file.clock_rate = 90000;
  codec_exists_in_file.channels = 1;
  codec_exists_in_file.media_type = erizo::VIDEO_TYPE;
  codec_exists_in_file.feedback_types.push_back(kFeedbackExistsInFile);
  rtp_mappings.push_back(codec_exists_in_file);

  erizo::SdpInfo sdp(rtp_mappings);
  sdp.initWithSdp(chrome_sdp_string, "video");

  for (auto rtp_map : sdp.getPayloadInfos()) {
    if (rtp_map.encoding_name == codec_exists_in_file.encoding_name) {
       EXPECT_EQ(rtp_map.feedback_types.size(), static_cast<unsigned int>(1));
       EXPECT_EQ(rtp_map.feedback_types.front(), kFeedbackExistsInFile);
    }
  }
}

TEST_F(SdpInfoMediaTest, shouldStoreSupportedFmtp) {
  const std::string kParameterNameExistsInFile = "minptime";
  const std::string kParameterValueExistsInFile = "10";
  erizo::RtpMap codec_exists_in_file;
  codec_exists_in_file.payload_type = 120;
  codec_exists_in_file.encoding_name = "opus";
  codec_exists_in_file.clock_rate = 48000;
  codec_exists_in_file.channels = 2;
  codec_exists_in_file.media_type = erizo::AUDIO_TYPE;
  codec_exists_in_file.format_parameters[kParameterNameExistsInFile] = kParameterValueExistsInFile;
  rtp_mappings.push_back(codec_exists_in_file);

  erizo::SdpInfo sdp(rtp_mappings);
  sdp.initWithSdp(chrome_sdp_string, "video");

  for (auto rtp_map : sdp.getPayloadInfos()) {
    if (rtp_map.encoding_name == codec_exists_in_file.encoding_name) {
      EXPECT_EQ(rtp_map.format_parameters.size(), static_cast<unsigned int>(1));
      EXPECT_EQ(rtp_map.format_parameters[kParameterNameExistsInFile], kParameterValueExistsInFile);
    }
  }
}

TEST_F(SdpInfoMediaTest, shouldBuildPTMatrix) {
  const unsigned int kOpusPtInFile = 111;
  erizo::RtpMap codec_exists_in_file;
  codec_exists_in_file.payload_type = 43;
  codec_exists_in_file.encoding_name = "opus";
  codec_exists_in_file.clock_rate = 48000;
  codec_exists_in_file.channels = 2;
  codec_exists_in_file.media_type = erizo::AUDIO_TYPE;
  rtp_mappings.push_back(codec_exists_in_file);

  erizo::SdpInfo sdp(rtp_mappings);
  sdp.initWithSdp(chrome_sdp_string, "video");

  EXPECT_EQ(sdp.getAudioExternalPT(codec_exists_in_file.payload_type), kOpusPtInFile);
  EXPECT_EQ(sdp.getAudioInternalPT(kOpusPtInFile), codec_exists_in_file.payload_type);
}

TEST_F(SdpInfoMediaTest, shouldOnlyMapRtxCorrespondingToSupportedCodecs) {
  const unsigned int kCorrespondingPT = 50;
  const std::string kFmtOptionForRtx = "apt";
  const std::string kRtxCodecName = "rtx";

  erizo::RtpMap codec_exists_in_file;
  codec_exists_in_file.payload_type = kCorrespondingPT;
  codec_exists_in_file.encoding_name = "VP8";
  codec_exists_in_file.clock_rate = 90000;
  codec_exists_in_file.channels = 1;
  codec_exists_in_file.media_type = erizo::VIDEO_TYPE;

  erizo::RtpMap rtx_codec_exists_in_file;
  rtx_codec_exists_in_file.payload_type = 105;
  rtx_codec_exists_in_file.encoding_name = kRtxCodecName;
  rtx_codec_exists_in_file.clock_rate = 90000;
  rtx_codec_exists_in_file.channels = 1;
  rtx_codec_exists_in_file.media_type = erizo::VIDEO_TYPE;
  rtx_codec_exists_in_file.format_parameters[kFmtOptionForRtx] = "50";

  rtp_mappings.push_back(codec_exists_in_file);
  rtp_mappings.push_back(rtx_codec_exists_in_file);

  erizo::SdpInfo sdp(rtp_mappings);
  sdp.initWithSdp(chrome_sdp_string, "video");
  int codec_hits_count = 0;
  for (auto negotiated_map : sdp.getPayloadInfos()) {
    if (negotiated_map.encoding_name == "rtx") {
      codec_hits_count++;
    }
  }
  EXPECT_EQ(codec_hits_count, 1);
}
