#include <gmock/gmock.h>
#include <gtest/gtest.h>


#include <NiceConnection.h>
#include <lib/LibNiceInterface.h>
#include <nice/nice.h>

using testing::_;
using testing::Return;
using testing::SetArgPointee;
using testing::SetArgReferee;
using testing::Invoke;
using testing::DoAll;
using testing ::Eq;

class MockLibNice: public erizo::LibNiceInterface {
 public:
  MockLibNice() {
    ON_CALL(*this, NiceAgentNew(_)).WillByDefault(Invoke(&real_impl_, &erizo::LibNiceInterfaceImpl::NiceAgentNew));
  }
  virtual ~MockLibNice() {
  }

  MOCK_METHOD1(NiceAgentNew, NiceAgent*(GMainContext*));
  MOCK_METHOD2(NiceAgentAddStream, int(NiceAgent*, unsigned int));
  MOCK_METHOD4(NiceAgentGetLocalCredentials, bool(NiceAgent*, unsigned int, char**, char**));
  MOCK_METHOD5(NiceAgentSetPortRange, void(NiceAgent*, unsigned int, unsigned int,
        unsigned int, unsigned int));
  MOCK_METHOD5(NiceAgentGetSelectedPair, bool(NiceAgent*, unsigned int, unsigned int, NiceCandidate**,
        NiceCandidate**));
  MOCK_METHOD7(NiceAgentSetRelayInfo, bool(NiceAgent*, unsigned int, unsigned int, const char*, unsigned int,
        const char*, const char*));
  MOCK_METHOD6(NiceAgentAttachRecv, bool(NiceAgent*, unsigned int, unsigned int, GMainContext*,
        void*, void*));
  MOCK_METHOD2(NiceAgentGatherCandidates, bool(NiceAgent*, unsigned int));
  MOCK_METHOD4(NiceAgentSetRemoteCredentials, bool(NiceAgent*, unsigned int, const char*, const char*));
  MOCK_METHOD4(NiceAgentSetRemoteCandidates, int(NiceAgent*, unsigned int, unsigned int, const GSList*));
  MOCK_METHOD3(NiceAgentGetLocalCandidates, GSList*(NiceAgent*, unsigned int, unsigned int));
  MOCK_METHOD5(NiceAgentSend, int(NiceAgent*, unsigned int, unsigned int, unsigned int, const char*));

 private:
  erizo::LibNiceInterfaceImpl real_impl_;
};

class MockNiceConnectionListener: public erizo::NiceConnectionListener {
 public:
  MockNiceConnectionListener() {
  }
  virtual ~MockNiceConnectionListener() {
  }

  MOCK_METHOD4(onNiceData, void(unsigned int, char*, int, erizo::NiceConnection*));
  MOCK_METHOD2(onCandidate, void(const erizo::CandidateInfo&, erizo::NiceConnection*));
  MOCK_METHOD2(updateIceState, void(erizo::IceState, erizo::NiceConnection*));
};

class NiceConnectionStartTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    libnice = new MockLibNice;
    libnice_pointer.reset(libnice);
    nice_listener = new MockNiceConnectionListener();
    ice_config = new erizo::IceConfig();
  }
  virtual void TearDown() {
    delete nice_listener;
    delete ice_config;
  }

  boost::shared_ptr<erizo::LibNiceInterface> libnice_pointer;
  MockLibNice* libnice;
  MockNiceConnectionListener* nice_listener;
  erizo::IceConfig* ice_config;
};

class NiceConnectionTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    libnice = new MockLibNice;
    libnice_pointer.reset(libnice);
    nice_listener = new MockNiceConnectionListener();
    ice_config = new erizo::IceConfig();
    ufrag = strdup("ufrag");
    pass = strdup("pass");

    EXPECT_CALL(*libnice, NiceAgentNew(_)).Times(1);
    EXPECT_CALL(*libnice, NiceAgentAddStream(_, _)).Times(1).WillOnce(Return(1));
    EXPECT_CALL(*libnice, NiceAgentGetLocalCredentials(_, _, _, _)).Times(1).
      WillOnce(DoAll(SetArgPointee<2>(ufrag), SetArgPointee<3>(pass), Return(true)));
    EXPECT_CALL(*libnice, NiceAgentAttachRecv(_, _, _, _, _, _)).Times(1).WillOnce(Return(true));
    EXPECT_CALL(*libnice, NiceAgentGatherCandidates(_, _)).Times(1).WillOnce(Return(true));
    EXPECT_CALL(*libnice, NiceAgentSetRemoteCredentials(_, _, _, _)).Times(0);
    EXPECT_CALL(*libnice, NiceAgentSetPortRange(_, _, _, _, _)).Times(0);
    EXPECT_CALL(*libnice, NiceAgentSetRelayInfo(_, _, _, _, _, _, _)).Times(0);

    nice_connection = new erizo::NiceConnection(libnice_pointer,
        erizo::VIDEO_TYPE,
        "video",
        "test_connection",
        nice_listener,
        1,
        *ice_config);
    nice_connection->start();
  }

  virtual void TearDown() {
    delete nice_connection;
    delete nice_listener;
    delete ice_config;
  }

  boost::shared_ptr<erizo::LibNiceInterface> libnice_pointer;
  MockLibNice* libnice;
  MockNiceConnectionListener* nice_listener;
  erizo::IceConfig* ice_config;
  erizo::NiceConnection* nice_connection;
  char* ufrag, * pass;
};

class NiceConnectionTwoComponentsTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    libnice = new MockLibNice;
    libnice_pointer.reset(libnice);
    nice_listener = new MockNiceConnectionListener();
    ice_config = new erizo::IceConfig();
    ufrag = strdup("ufrag");
    pass = strdup("pass");

    EXPECT_CALL(*libnice, NiceAgentNew(_)).Times(1);
    EXPECT_CALL(*libnice, NiceAgentAddStream(_, _)).Times(1).WillOnce(Return(1));
    EXPECT_CALL(*libnice, NiceAgentGetLocalCredentials(_, _, _, _)).Times(1).
      WillOnce(DoAll(SetArgPointee<2>(ufrag), SetArgPointee<3>(pass), Return(true)));
    EXPECT_CALL(*libnice, NiceAgentAttachRecv(_, _, _, _, _, _)).Times(2).WillRepeatedly(Return(true));
    EXPECT_CALL(*libnice, NiceAgentGatherCandidates(_, _)).Times(1).WillOnce(Return(true));
    EXPECT_CALL(*libnice, NiceAgentSetRemoteCredentials(_, _, _, _)).Times(0);
    EXPECT_CALL(*libnice, NiceAgentSetPortRange(_, _, _, _, _)).Times(0);
    EXPECT_CALL(*libnice, NiceAgentSetRelayInfo(_, _, _, _, _, _, _)).Times(0);

    nice_connection = new erizo::NiceConnection(libnice_pointer,
        erizo::VIDEO_TYPE,
        "video",
        "test_connection",
        nice_listener,
        2,
        *ice_config);
    nice_connection->start();
  }

  virtual void TearDown() {
    delete nice_connection;
    delete nice_listener;
    delete ice_config;
  }

  boost::shared_ptr<erizo::LibNiceInterface> libnice_pointer;
  MockLibNice* libnice;
  MockNiceConnectionListener* nice_listener;
  erizo::IceConfig* ice_config;
  erizo::NiceConnection* nice_connection;
  char* ufrag, * pass;
};

TEST_F(NiceConnectionStartTest, start_Configures_Libnice_With_Default_Config) {
  char *ufrag = strdup("ufrag");
  char *pass = strdup("pass");

  EXPECT_CALL(*libnice, NiceAgentNew(_)).Times(1);
  EXPECT_CALL(*libnice, NiceAgentAddStream(_, _)).Times(1).WillOnce(Return(1));
  EXPECT_CALL(*libnice, NiceAgentGetLocalCredentials(_, _, _, _)).Times(1).
    WillOnce(DoAll(SetArgPointee<2>(ufrag), SetArgPointee<3>(pass), Return(true)));
  EXPECT_CALL(*libnice, NiceAgentAttachRecv(_, _, _, _, _, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*libnice, NiceAgentGatherCandidates(_, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*libnice, NiceAgentSetRemoteCredentials(_, _, _, _)).Times(0);
  EXPECT_CALL(*libnice, NiceAgentSetPortRange(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*libnice, NiceAgentSetRelayInfo(_, _, _, _, _, _, _)).Times(0);

  erizo::NiceConnection nice(libnice_pointer,
      erizo::VIDEO_TYPE,
      "video",
      "test_connection",
      nice_listener,
      1,
      *ice_config);
  nice.start();
}

TEST_F(NiceConnectionStartTest, start_Configures_Libnice_With_Remote_Credentials) {
  char *ufrag = strdup("ufrag");
  char *pass = strdup("pass");

  EXPECT_CALL(*libnice, NiceAgentNew(_)).Times(1);
  EXPECT_CALL(*libnice, NiceAgentAddStream(_, _)).Times(1);
  EXPECT_CALL(*libnice, NiceAgentGetLocalCredentials(_, _, _, _)).Times(1).
    WillOnce(DoAll(SetArgPointee<2>(ufrag), SetArgPointee<3>(pass), Return(true)));
  EXPECT_CALL(*libnice, NiceAgentAttachRecv(_, _, _, _, _, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*libnice, NiceAgentGatherCandidates(_, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*libnice, NiceAgentSetRemoteCredentials(_, _, _, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*libnice, NiceAgentSetPortRange(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*libnice, NiceAgentSetRelayInfo(_, _, _, _, _, _, _)).Times(0);

  erizo::NiceConnection nice(libnice_pointer,
      erizo::VIDEO_TYPE,
      "video",
      "test_connection",
      nice_listener,
      1,
      *ice_config,
      "ufrag",
      "pass");
  nice.start();
}

TEST_F(NiceConnectionStartTest, start_Configures_Libnice_With_Port_Range) {
  char *ufrag = strdup("ufrag");
  char *pass = strdup("pass");

  ice_config->minPort = 10;
  ice_config->maxPort = 60;

  EXPECT_CALL(*libnice, NiceAgentNew(_)).Times(1);
  EXPECT_CALL(*libnice, NiceAgentAddStream(_, _)).Times(1);
  EXPECT_CALL(*libnice, NiceAgentGetLocalCredentials(_, _, _, _)).Times(1).
    WillOnce(DoAll(SetArgPointee<2>(ufrag), SetArgPointee<3>(pass), Return(true)));
  EXPECT_CALL(*libnice, NiceAgentAttachRecv(_, _, _, _, _, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*libnice, NiceAgentGatherCandidates(_, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*libnice, NiceAgentSetRemoteCredentials(_, _, _, _)).Times(0);
  EXPECT_CALL(*libnice, NiceAgentSetPortRange(_, _, _, 10, 60)).Times(1);
  EXPECT_CALL(*libnice, NiceAgentSetRelayInfo(_, _, _, _, _, _, _)).Times(0);

  erizo::NiceConnection nice(libnice_pointer,
      erizo::VIDEO_TYPE,
      "video",
      "test_connection",
      nice_listener,
      1,
      *ice_config);
  nice.start();
}

TEST_F(NiceConnectionStartTest, start_Configures_Libnice_With_Turn) {
  char *ufrag = strdup("ufrag");
  char *pass = strdup("pass");

  ice_config->turnServer = "atunrserver.com";
  ice_config->turnPort = 3478;
  ice_config->turnUsername = "test";
  ice_config->turnPass = "test";

  EXPECT_CALL(*libnice, NiceAgentNew(_)).Times(1);
  EXPECT_CALL(*libnice, NiceAgentAddStream(_, _)).Times(1);
  EXPECT_CALL(*libnice, NiceAgentGetLocalCredentials(_, _, _, _)).Times(1).
    WillOnce(DoAll(SetArgPointee<2>(ufrag), SetArgPointee<3>(pass), Return(true)));
  EXPECT_CALL(*libnice, NiceAgentAttachRecv(_, _, _, _, _, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*libnice, NiceAgentGatherCandidates(_, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*libnice, NiceAgentSetRemoteCredentials(_, _, _, _)).Times(0);
  EXPECT_CALL(*libnice, NiceAgentSetPortRange(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*libnice, NiceAgentSetRelayInfo(_, _, _, ice_config->turnServer.c_str(),
        ice_config->turnPort, ice_config->turnUsername.c_str(),
        ice_config->turnPass.c_str())).Times(1).
        WillOnce(Return(true));

  erizo::NiceConnection nice(libnice_pointer,
      erizo::VIDEO_TYPE,
      "video",
      "test_connection",
      nice_listener,
      1,
      *ice_config);
  nice.start();
}

TEST_F(NiceConnectionTest, setRemoteCandidates_Success_WhenCalled) {
  erizo::CandidateInfo test_candidate;
  test_candidate.isBundle = true;
  test_candidate.priority = 0;
  test_candidate.componentId = 1;
  test_candidate.foundation = "10";
  test_candidate.hostAddress = "192.168.1.1";
  test_candidate.rAddress = "";
  test_candidate.hostPort = 5000;
  test_candidate.rPort = 0;
  test_candidate.netProtocol = "udp";
  test_candidate.hostType = erizo::HOST;
  test_candidate.username = "hola";
  test_candidate.password = "hola";
  test_candidate.mediaType = erizo::VIDEO_TYPE;

  std::vector<erizo::CandidateInfo> candidate_list;
  candidate_list.push_back(test_candidate);

  EXPECT_CALL(*libnice, NiceAgentSetRemoteCandidates(_, _, _, _)).Times(1);
  nice_connection->setRemoteCandidates(candidate_list, true);
}

TEST_F(NiceConnectionTest, queuePacket_QueuedPackets_Can_Be_getPacket_When_Ready) {
  char* test_packet = strdup("tester");
  EXPECT_CALL(*nice_listener, updateIceState(erizo::NICE_READY , _)).Times(1);
  nice_connection->updateIceState(erizo::NICE_READY);
  nice_connection->queueData(0, test_packet, sizeof(test_packet));
  erizo::packetPtr packet = nice_connection->getPacket();
  EXPECT_EQ(packet->length, sizeof(test_packet));
  EXPECT_EQ(0, strcmp(test_packet, packet->data));
  free(test_packet);
}

TEST_F(NiceConnectionTest, sendData_Succeed_When_Ice_Ready) {
  char* test_packet = strdup("tester");
  const unsigned int kCompId = 1;
  const unsigned int kLength = sizeof(test_packet);
  EXPECT_CALL(*nice_listener, updateIceState(erizo::NICE_READY , _)).Times(1);
  nice_connection->updateIceState(erizo::NICE_READY);
  EXPECT_CALL(*libnice, NiceAgentSend(_, _, kCompId, kLength, _)).Times(1).WillOnce(Return(kLength));
  EXPECT_EQ(kLength, nice_connection->sendData(kCompId, test_packet, kLength));
  free(test_packet);
}

TEST_F(NiceConnectionTest, sendData_Fail_When_Ice_Not_Ready) {
  char* test_packet = strdup("tester");
  const unsigned int kCompId = 1;
  const unsigned int kLength = sizeof(test_packet);
  EXPECT_CALL(*libnice, NiceAgentSend(_, _, kCompId, kLength, _)).Times(0);
  EXPECT_EQ(-1, nice_connection->sendData(kCompId, test_packet, kLength));
  free(test_packet);
}

TEST_F(NiceConnectionTest, gatheringDone_Triggers_updateIceState) {
  EXPECT_CALL(*nice_listener, updateIceState(erizo::NICE_CANDIDATES_RECEIVED , _)).Times(1);
  nice_connection->gatheringDone(1);
}

TEST_F(NiceConnectionTest, getSelectedPair_Calls_Libnice_And_Returns_Pair) {
  const std::string kRemoteIp = "192.168.1.2";
  const int kRemotePort = 4242;

  const std::string kLocalIp = "192.168.1.1";
  const int kLocalPort = 2222;

  NiceCandidate* local_candidate = nice_candidate_new(NICE_CANDIDATE_TYPE_HOST);
  local_candidate->username = strdup("localuser");
  local_candidate->password = strdup("localpass");
  local_candidate->stream_id = (guint) 1;
  local_candidate->component_id = 1;
  local_candidate->priority = 1;
  local_candidate->transport = NICE_CANDIDATE_TRANSPORT_UDP;
  nice_address_set_from_string(&local_candidate->addr, kLocalIp.c_str());
  nice_address_set_from_string(&local_candidate->base_addr, kLocalIp.c_str());
  nice_address_set_port(&local_candidate->addr, kLocalPort);
  nice_address_set_port(&local_candidate->base_addr, kLocalPort);

  NiceCandidate* remote_candidate = nice_candidate_new(NICE_CANDIDATE_TYPE_HOST);
  remote_candidate->username = strdup("remoteUser");
  remote_candidate->password = strdup("remotePass");
  remote_candidate->stream_id = (guint) 1;
  remote_candidate->component_id = 1;
  remote_candidate->priority = 1;
  remote_candidate->transport = NICE_CANDIDATE_TRANSPORT_UDP;
  nice_address_set_from_string(&remote_candidate->addr, kRemoteIp.c_str());
  nice_address_set_port(&remote_candidate->addr, kRemotePort);
  nice_address_set_from_string(&remote_candidate->base_addr, kRemoteIp.c_str());
  nice_address_set_port(&remote_candidate->base_addr, kRemotePort);

  EXPECT_CALL(*libnice, NiceAgentGetSelectedPair(_, _, _, _, _)).Times(1).WillOnce(
    DoAll(SetArgPointee<3>(local_candidate), SetArgPointee<4>(remote_candidate), Return(true)));

  erizo::CandidatePair candidate_pair = nice_connection->getSelectedPair();
  EXPECT_EQ(candidate_pair.erizoCandidateIp, kLocalIp);
  EXPECT_EQ(candidate_pair.erizoCandidatePort, kLocalPort);
  EXPECT_EQ(candidate_pair.clientCandidateIp, kRemoteIp);
  EXPECT_EQ(candidate_pair.clientCandidatePort, kRemotePort);
}

TEST_F(NiceConnectionTest, getCandidate_Passes_Candidate_to_Listener) {
  GSList* candidate_list = NULL;
  NiceCandidate* candidate = nice_candidate_new(NICE_CANDIDATE_TYPE_HOST);

  candidate->username = strdup("username");
  candidate->password = strdup("password");
  candidate->stream_id = (guint) 1;
  candidate->component_id = 1;
  candidate->priority = 1;
  candidate->transport = NICE_CANDIDATE_TRANSPORT_UDP;
  nice_address_set_from_string(&candidate->addr, "192.168.1.1");
  nice_address_set_from_string(&candidate->base_addr, "192.168.1.2");
  nice_address_set_port(&candidate->addr, 10);
  nice_address_set_port(&candidate->base_addr, 20);

  candidate_list = g_slist_prepend(candidate_list, candidate);
  EXPECT_CALL(*libnice, NiceAgentGetLocalCandidates(_, _, _)).Times(1).WillOnce(Return(candidate_list));
  EXPECT_CALL(*nice_listener, onCandidate(_, _)).Times(1);
  nice_connection->getCandidate(1, 1, "test");
}

TEST_F(NiceConnectionTest, setRemoteCredentials_Configures_NiceAgent) {
  std::string username = "username";
  std::string password = "password";

  EXPECT_CALL(*libnice, NiceAgentSetRemoteCredentials(_, _, username.c_str(), password.c_str())).Times(1);
  nice_connection->setRemoteCredentials(username, password);
}

TEST_F(NiceConnectionTest, updateComponentState_NiceConnection_Is_Ready_When_Single_Component_Is_Ready) {
  EXPECT_CALL(*nice_listener, updateIceState(erizo::NICE_READY , _)).Times(1);
  nice_connection->updateComponentState(1, erizo::NICE_READY);
}

TEST_F(NiceConnectionTest, updateComponentState_Listener_Is_Notified_Ready_When_Single_Component_Is_Ready) {
  EXPECT_CALL(*nice_listener, updateIceState(erizo::NICE_READY , _)).Times(1);
  nice_connection->updateComponentState(1, erizo::NICE_READY);
}

TEST_F(NiceConnectionTest, updateComponentState_Listener_Is_Not_Notified_If_Backwards_Transition) {
  EXPECT_CALL(*nice_listener, updateIceState(erizo::NICE_READY , _)).Times(1);
  EXPECT_CALL(*nice_listener, updateIceState(erizo::NICE_CANDIDATES_RECEIVED, _)).Times(1);
  nice_connection->updateComponentState(1, erizo::NICE_CANDIDATES_RECEIVED);
  nice_connection->updateComponentState(1, erizo::NICE_READY);
  nice_connection->updateComponentState(1, erizo::NICE_CANDIDATES_RECEIVED);
}

TEST_F(NiceConnectionTwoComponentsTest,
    updateComponentState_NiceConnection_Is_Not_Ready_When_Single_Component_Is_Ready) {
  EXPECT_CALL(*nice_listener, updateIceState(erizo::NICE_READY , _)).Times(0);
  nice_connection->updateComponentState(1, erizo::NICE_READY);
}

TEST_F(NiceConnectionTwoComponentsTest, updateComponentState_NiceConnection_Is_Ready_When_Both_Components_Are_Ready) {
  EXPECT_CALL(*nice_listener, updateIceState(erizo::NICE_READY , _)).Times(1);
  nice_connection->updateComponentState(1, erizo::NICE_READY);
  nice_connection->updateComponentState(2, erizo::NICE_READY);
}
