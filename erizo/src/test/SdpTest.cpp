#include <boost/test/unit_test.hpp>
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


BOOST_AUTO_TEST_SUITE(erizoSdp)
/*---------- RtpPacketQueue TESTS ----------*/
BOOST_AUTO_TEST_CASE(SdpInfoDefaults)
{
  BOOST_CHECK(true);
}
BOOST_AUTO_TEST_CASE(ChromeSdp)
{
  erizo::SdpInfo sdp;
  std::ifstream ifs("Chrome.sdp", std::fstream::in);
  std::string sdpString = readFile(ifs);
  sdp.initWithSdp(sdpString, "video");
  //Check the mlines
  BOOST_CHECK(sdp.hasVideo==true);
  BOOST_CHECK(sdp.hasAudio==true);
  //Check the fingerprint
  BOOST_CHECK(sdp.fingerprint.compare("58:8B:E5:05:5C:0F:B6:38:28:F9:DC:24:00:8F:E2:A5:52:B6:92:E7:58:38:53:6B:01:1A:12:7F:EF:55:78:6E")==0);
  //Check ICE Credentials
  std::string username, password;
  sdp.getCredentials(username, password, erizo::VIDEO_TYPE);
  BOOST_CHECK(username.compare("Bs0jL+c884dYG/oe")==0);   
  BOOST_CHECK(password.compare("ilq+r19kdvFsufkcyYAxoUM8")==0);
}

BOOST_AUTO_TEST_CASE(FirefoxSdp)
{
}

BOOST_AUTO_TEST_CASE(OpenWebRtcSdp)
{
}
BOOST_AUTO_TEST_SUITE_END()
