#include <gtest/gtest.h>
#include <rtp/RtpExtentionIdTranslator.h>
#include <utility>

class RtpExtentionIdTranslatorTest : public ::testing::TestWithParam<int> {
 protected:
  void SetUp() override {
    translator.reset(new erizo::RtpExtentionIdTranslator);
  }

 protected:
  std::unique_ptr<erizo::RtpExtentionIdTranslator> translator;
};

TEST_P(RtpExtentionIdTranslatorTest, do_not_translate_if_both_empty) {
  auto val = GetParam();
  EXPECT_EQ(0, translator->translateId(val));
}

TEST_P(RtpExtentionIdTranslatorTest, do_not_translate_if_dest_empty) {
  const auto src_map = std::map<int, erizo::RTPExtensions>{{erizo::SSRC_AUDIO_LEVEL, erizo::SSRC_AUDIO_LEVEL},
                                                           {erizo::ABS_SEND_TIME, erizo::ABS_SEND_TIME},
                                                           {erizo::TOFFSET, erizo::TOFFSET},
                                                           {erizo::VIDEO_ORIENTATION, erizo::VIDEO_ORIENTATION},
                                                           {erizo::TRANSPORT_CC, erizo::TRANSPORT_CC},
                                                           {erizo::PLAYBACK_TIME, erizo::PLAYBACK_TIME},
                                                           {erizo::RTP_ID, erizo::RTP_ID}};
  translator->setSourceMap(erizo::RTPExtensionsMap(src_map));
  EXPECT_EQ(0, translator->translateId(GetParam()));
}

TEST_P(RtpExtentionIdTranslatorTest, translation_with_one_item) {
  const auto src_map = std::map<int, erizo::RTPExtensions>{{5, erizo::SSRC_AUDIO_LEVEL }};
  const auto dest_map = std::map<int, erizo::RTPExtensions>{{1, erizo::SSRC_AUDIO_LEVEL }};
  translator->setSourceMap(erizo::RTPExtensionsMap(src_map));
  translator->setDestMap(erizo::RTPExtensionsMap(dest_map));
  auto val = GetParam();
  EXPECT_EQ(val == 5? 1 : 0, translator->translateId(val));
}

TEST_P(RtpExtentionIdTranslatorTest, equal_map_test) {
  const auto map = std::map<int, erizo::RTPExtensions>{{5, erizo::SSRC_AUDIO_LEVEL},
                                                       {6, erizo::PLAYBACK_TIME},
                                                       {2, erizo::VIDEO_ORIENTATION},
                                                       {9, erizo::TRANSPORT_CC}};
  translator->setSourceMap(erizo::RTPExtensionsMap(map));
  translator->setDestMap(erizo::RTPExtensionsMap(map));
  auto val = GetParam();
  auto expect = map.find(val) != map.end()? val : 0;
  EXPECT_EQ(expect, translator->translateId(val));
}

TEST_P(RtpExtentionIdTranslatorTest, full_map_translation) {
  const auto src_map = std::map<int, erizo::RTPExtensions>{{erizo::SSRC_AUDIO_LEVEL, erizo::SSRC_AUDIO_LEVEL},
                                                           {erizo::ABS_SEND_TIME, erizo::ABS_SEND_TIME},
                                                           {erizo::TOFFSET, erizo::TOFFSET},
                                                           {erizo::VIDEO_ORIENTATION, erizo::VIDEO_ORIENTATION},
                                                           {erizo::TRANSPORT_CC, erizo::TRANSPORT_CC},
                                                           {erizo::PLAYBACK_TIME, erizo::PLAYBACK_TIME},
                                                           {erizo::RTP_ID, erizo::RTP_ID}};
  // scr -> dest + 1
  const auto dest_map = std::map<int, erizo::RTPExtensions>{{erizo::SSRC_AUDIO_LEVEL + 1, erizo::SSRC_AUDIO_LEVEL},
                                                            {erizo::ABS_SEND_TIME + 1, erizo::ABS_SEND_TIME},
                                                            {erizo::TOFFSET + 1, erizo::TOFFSET},
                                                            {erizo::VIDEO_ORIENTATION + 1, erizo::VIDEO_ORIENTATION},
                                                            {erizo::TRANSPORT_CC + 1, erizo::TRANSPORT_CC},
                                                            {erizo::PLAYBACK_TIME + 1, erizo::PLAYBACK_TIME},
                                                            {erizo::RTP_ID + 1, erizo::RTP_ID}};
  translator->setSourceMap(erizo::RTPExtensionsMap(src_map));
  translator->setDestMap(erizo::RTPExtensionsMap(dest_map));
  auto val = GetParam();
  auto expected = (val >= erizo::SSRC_AUDIO_LEVEL && val <= erizo::RTP_ID)? val + 1 : 0;
  EXPECT_EQ(expected, translator->translateId(val));
}

INSTANTIATE_TEST_CASE_P(ExtentionIds, RtpExtentionIdTranslatorTest,
                       // There can be up to 14 extensions, test them all
                       testing::Values(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14));
