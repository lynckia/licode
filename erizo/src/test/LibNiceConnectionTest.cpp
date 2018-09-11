#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <LibNiceConnection.h>
#include <lib/LibNiceInterface.h>
#include <nice/nice.h>

#include <string>
#include <vector>

using testing::_;
using testing::Return;
using testing::SetArgPointee;
using testing::SetArgReferee;
using testing::Invoke;
using testing::DoAll;
using testing::Eq;
using testing::Not;
using testing::StrEq;
using testing::SaveArg;

class MockLibNice: public erizo::LibNiceInterface {
 public:
  MockLibNice() {
    ON_CALL(*this, NiceAgentNew(_)).WillByDefault(Invoke(&real_impl_, &erizo::LibNiceInterfaceImpl::NiceAgentNew));
  }
  virtual ~MockLibNice() {
  }

  MOCK_METHOD1(NiceAgentNew, NiceAgent*(GMainContext*));
  MOCK_METHOD1(NiceInterfacesGetIpForInterface, char*(const char *));
  MOCK_METHOD2(NiceAgentAddStream, int(NiceAgent*, unsigned int));
  MOCK_METHOD2(NiceAgentAddLocalAddress, bool(NiceAgent* agent, const char *ip));
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

class MockLibNiceConnectionListener: public erizo::IceConnectionListener {
 public:
  MockLibNiceConnectionListener() {
  }
  virtual ~MockLibNiceConnectionListener() {
  }
  MOCK_METHOD1(onPacketReceived, void(erizo::packetPtr packet));
  MOCK_METHOD2(onCandidate, void(const erizo::CandidateInfo&, erizo::IceConnection*));
  MOCK_METHOD2(updateIceState, void(erizo::IceState, erizo::IceConnection*));
};

class LibNiceConnectionStartTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    libnice = new MockLibNice;
    libnice_pointer.reset(libnice);
    nice_listener = std::make_shared<MockLibNiceConnectionListener>();
    ice_config = new erizo::IceConfig();
  }
  virtual void TearDown() {
    delete ice_config;
  }

  boost::shared_ptr<erizo::LibNiceInterface> libnice_pointer;
  MockLibNice* libnice;
  std::shared_ptr<MockLibNiceConnectionListener> nice_listener;
  erizo::IceConfig* ice_config;
};

class LibNiceConnectionTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    const std::string kArbitraryLocalCredentialUsername = "ufrag";
    const std::string kArbitraryLocalCredentialPassword = "upass";
    const std::string kArbitraryConnectionId = "a_connection_id";
    const std::string kArbitraryTransportName = "video";
    const std::string kArbitraryDataPacket = "test";

    libnice = new MockLibNice;
    libnice_pointer.reset(libnice);
    nice_listener = std::make_shared<MockLibNiceConnectionListener>();
    ice_config = new erizo::IceConfig();
    ufrag = strdup(kArbitraryLocalCredentialUsername.c_str());
    pass = strdup(kArbitraryLocalCredentialPassword.c_str());
    test_packet = strdup(kArbitraryDataPacket.c_str());

    ice_config->media_type = erizo::VIDEO_TYPE;
    ice_config->transport_name = kArbitraryTransportName;
    ice_config->ice_components = 1;
    ice_config->connection_id = kArbitraryConnectionId;

    EXPECT_CALL(*libnice, NiceAgentNew(_)).Times(1);
    EXPECT_CALL(*libnice, NiceAgentAddStream(_, _)).Times(1).WillOnce(Return(1));
    EXPECT_CALL(*libnice, NiceAgentGetLocalCredentials(_, _, _, _)).Times(1).
      WillOnce(DoAll(SetArgPointee<2>(ufrag), SetArgPointee<3>(pass), Return(true)));
    EXPECT_CALL(*libnice, NiceAgentAttachRecv(_, _, _, _, _, _)).Times(1).WillOnce(Return(true));
    EXPECT_CALL(*libnice, NiceAgentGatherCandidates(_, _)).Times(1).WillOnce(Return(true));
    EXPECT_CALL(*libnice, NiceAgentSetRemoteCredentials(_, _, _, _)).Times(0);
    EXPECT_CALL(*libnice, NiceAgentSetPortRange(_, _, _, _, _)).Times(0);
    EXPECT_CALL(*libnice, NiceAgentSetRelayInfo(_, _, _, _, _, _, _)).Times(0);

    nice_connection = new erizo::LibNiceConnection(libnice_pointer,
        *ice_config);
    nice_connection->setIceListener(nice_listener);
    nice_connection->start();
  }

  virtual void TearDown() {
    delete nice_connection;
    delete ice_config;
    free(test_packet);
  }

  boost::shared_ptr<erizo::LibNiceInterface> libnice_pointer;
  MockLibNice* libnice;
  std::shared_ptr<MockLibNiceConnectionListener> nice_listener;
  erizo::IceConfig* ice_config;
  erizo::LibNiceConnection* nice_connection;
  char* ufrag, * pass, * test_packet;
};

class LibNiceConnectionTwoComponentsTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    const std::string kArbitraryLocalCredentialUsername = "ufrag";
    const std::string kArbitraryLocalCredentialPassword = "upass";
    const std::string kArbitraryConnectionId = "a_connection_id";
    const std::string kArbitraryTransportName = "video";

    libnice = new MockLibNice;
    libnice_pointer.reset(libnice);
    nice_listener = std::make_shared<MockLibNiceConnectionListener>();
    ice_config = new erizo::IceConfig();
    ufrag = strdup(kArbitraryLocalCredentialUsername.c_str());
    pass = strdup(kArbitraryLocalCredentialPassword.c_str());
    ice_config->media_type = erizo::VIDEO_TYPE;
    ice_config->transport_name = kArbitraryTransportName;
    ice_config->ice_components = 2;
    ice_config->connection_id = kArbitraryConnectionId;

    EXPECT_CALL(*libnice, NiceAgentNew(_)).Times(1);
    EXPECT_CALL(*libnice, NiceAgentAddStream(_, _)).Times(1).WillOnce(Return(1));
    EXPECT_CALL(*libnice, NiceAgentGetLocalCredentials(_, _, _, _)).Times(1).
      WillOnce(DoAll(SetArgPointee<2>(ufrag), SetArgPointee<3>(pass), Return(true)));
    EXPECT_CALL(*libnice, NiceAgentAttachRecv(_, _, _, _, _, _)).Times(2).WillRepeatedly(Return(true));
    EXPECT_CALL(*libnice, NiceAgentGatherCandidates(_, _)).Times(1).WillOnce(Return(true));
    EXPECT_CALL(*libnice, NiceAgentSetRemoteCredentials(_, _, _, _)).Times(0);
    EXPECT_CALL(*libnice, NiceAgentSetPortRange(_, _, _, _, _)).Times(0);
    EXPECT_CALL(*libnice, NiceAgentSetRelayInfo(_, _, _, _, _, _, _)).Times(0);

    nice_connection = new erizo::LibNiceConnection(libnice_pointer,
        *ice_config);
    nice_connection->setIceListener(nice_listener);
    nice_connection->start();
  }

  virtual void TearDown() {
    delete nice_connection;
    delete ice_config;
  }

  boost::shared_ptr<erizo::LibNiceInterface> libnice_pointer;
  MockLibNice* libnice;
  std::shared_ptr<MockLibNiceConnectionListener> nice_listener;
  erizo::IceConfig* ice_config;
  erizo::LibNiceConnection* nice_connection;
  char* ufrag, * pass;
};

TEST_F(LibNiceConnectionStartTest, start_Configures_Libnice_With_Default_Config) {
  const std::string kArbitraryLocalCredentialUsername = "ufrag";
  const std::string kArbitraryLocalCredentialPassword = "upass";
  const std::string kArbitraryConnectionId = "a_connection_id";
  const std::string kArbitraryTransportName = "video";

  char *ufrag = strdup(kArbitraryLocalCredentialUsername.c_str());
  char *pass = strdup(kArbitraryLocalCredentialPassword.c_str());

  ice_config->media_type = erizo::VIDEO_TYPE;
  ice_config->transport_name = kArbitraryTransportName;
  ice_config->ice_components = 1;
  ice_config->connection_id = kArbitraryConnectionId;

  EXPECT_CALL(*libnice, NiceAgentNew(_)).Times(1);
  EXPECT_CALL(*libnice, NiceAgentAddStream(_, _)).Times(1).WillOnce(Return(1));
  EXPECT_CALL(*libnice, NiceAgentGetLocalCredentials(_, _, _, _)).Times(1).
    WillOnce(DoAll(SetArgPointee<2>(ufrag), SetArgPointee<3>(pass), Return(true)));
  EXPECT_CALL(*libnice, NiceAgentAttachRecv(_, _, _, _, _, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*libnice, NiceAgentGatherCandidates(_, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*libnice, NiceAgentSetRemoteCredentials(_, _, _, _)).Times(0);
  EXPECT_CALL(*libnice, NiceAgentSetPortRange(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*libnice, NiceAgentSetRelayInfo(_, _, _, _, _, _, _)).Times(0);

  erizo::LibNiceConnection nice(libnice_pointer,
      *ice_config);
  nice.setIceListener(nice_listener);
  nice.start();
}

TEST_F(LibNiceConnectionStartTest, start_Configures_Libnice_With_Remote_Credentials) {
  const std::string kArbitraryLocalCredentialUsername = "ufrag";
  const std::string kArbitraryLocalCredentialPassword = "upass";
  const std::string kArbitraryRemoteCredentialUsername = "remote_ufrag";
  const std::string kArbitraryRemoteCredentialPassword = "remote_upass";

  const std::string kArbitraryConnectionId = "a_connection_id";
  const std::string kArbitraryTransportName = "video";

  char *ufrag = strdup(kArbitraryLocalCredentialUsername.c_str());
  char *pass = strdup(kArbitraryLocalCredentialPassword.c_str());

  ice_config->media_type = erizo::VIDEO_TYPE;
  ice_config->transport_name = kArbitraryTransportName;
  ice_config->ice_components = 1;
  ice_config->connection_id = kArbitraryConnectionId;

  ice_config->username = kArbitraryRemoteCredentialUsername;
  ice_config->password = kArbitraryRemoteCredentialPassword;

  EXPECT_CALL(*libnice, NiceAgentNew(_)).Times(1);
  EXPECT_CALL(*libnice, NiceAgentAddStream(_, _)).Times(1);
  EXPECT_CALL(*libnice, NiceAgentGetLocalCredentials(_, _, _, _)).Times(1).
    WillOnce(DoAll(SetArgPointee<2>(ufrag), SetArgPointee<3>(pass), Return(true)));
  EXPECT_CALL(*libnice, NiceAgentAttachRecv(_, _, _, _, _, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*libnice, NiceAgentGatherCandidates(_, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*libnice, NiceAgentSetRemoteCredentials(_, _, _, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*libnice, NiceAgentSetPortRange(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*libnice, NiceAgentSetRelayInfo(_, _, _, _, _, _, _)).Times(0);

  erizo::LibNiceConnection nice(libnice_pointer,
      *ice_config);
  nice.setIceListener(nice_listener);
  nice.start();
}

TEST_F(LibNiceConnectionStartTest, start_Configures_Libnice_With_Port_Range) {
  const std::string kArbitraryLocalCredentialUsername = "ufrag";
  const std::string kArbitraryLocalCredentialPassword = "upass";
  const std::string kArbitraryConnectionId = "a_connection_id";
  const std::string kArbitraryTransportName = "video";

  const unsigned int kArbitraryMinPort = 1240;
  const unsigned int kArbitraryMaxPort = 2504;

  char *ufrag = strdup(kArbitraryLocalCredentialUsername.c_str());
  char *pass = strdup(kArbitraryLocalCredentialPassword.c_str());

  ice_config->min_port = kArbitraryMinPort;
  ice_config->max_port = kArbitraryMaxPort;
  ice_config->transport_name = kArbitraryTransportName;
  ice_config->media_type = erizo::VIDEO_TYPE;
  ice_config->connection_id = kArbitraryConnectionId;
  ice_config->ice_components = 1;

  EXPECT_CALL(*libnice, NiceAgentNew(_)).Times(1);
  EXPECT_CALL(*libnice, NiceAgentAddStream(_, _)).Times(1);
  EXPECT_CALL(*libnice, NiceAgentGetLocalCredentials(_, _, _, _)).Times(1).
    WillOnce(DoAll(SetArgPointee<2>(ufrag), SetArgPointee<3>(pass), Return(true)));
  EXPECT_CALL(*libnice, NiceAgentAttachRecv(_, _, _, _, _, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*libnice, NiceAgentGatherCandidates(_, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*libnice, NiceAgentSetRemoteCredentials(_, _, _, _)).Times(0);
  EXPECT_CALL(*libnice, NiceAgentSetPortRange(_, _, _, kArbitraryMinPort, kArbitraryMaxPort)).Times(1);
  EXPECT_CALL(*libnice, NiceAgentSetRelayInfo(_, _, _, _, _, _, _)).Times(0);

  erizo::LibNiceConnection nice(libnice_pointer,
      *ice_config);
  nice.setIceListener(nice_listener);
  nice.start();
}

TEST_F(LibNiceConnectionStartTest, start_Configures_Libnice_With_Turn) {
  const std::string kArbitraryLocalCredentialUsername = "ufrag";
  const std::string kArbitraryLocalCredentialPassword = "upass";
  const std::string kArbitraryConnectionId = "a_connection_id";
  const std::string kArbitraryTransportName = "video";
  const std::string kArbitraryTurnServerUrl = "aturnserver.com";
  const uint16_t kArbitraryTurnPort = 443;
  const std::string kArbitraryTurnUsername = "a_turn_username";
  const std::string kArbitraryTurnPassword = "a_turn_password";

  char *ufrag = strdup(kArbitraryLocalCredentialUsername.c_str());
  char *pass = strdup(kArbitraryLocalCredentialPassword.c_str());

  ice_config->turn_server = kArbitraryTurnServerUrl;
  ice_config->turn_port = kArbitraryTurnPort;
  ice_config->turn_username = kArbitraryTurnUsername;
  ice_config->turn_pass = kArbitraryTurnPassword;
  ice_config->transport_name = kArbitraryTransportName;
  ice_config->media_type = erizo::VIDEO_TYPE;
  ice_config->connection_id = kArbitraryConnectionId;
  ice_config->ice_components = 1;


  EXPECT_CALL(*libnice, NiceAgentNew(_)).Times(1);
  EXPECT_CALL(*libnice, NiceAgentAddStream(_, _)).Times(1);
  EXPECT_CALL(*libnice, NiceAgentGetLocalCredentials(_, _, _, _)).Times(1).
    WillOnce(DoAll(SetArgPointee<2>(ufrag), SetArgPointee<3>(pass), Return(true)));
  EXPECT_CALL(*libnice, NiceAgentAttachRecv(_, _, _, _, _, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*libnice, NiceAgentGatherCandidates(_, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*libnice, NiceAgentSetRemoteCredentials(_, _, _, _)).Times(0);
  EXPECT_CALL(*libnice, NiceAgentSetPortRange(_, _, _, _, _)).Times(0);
  EXPECT_CALL(*libnice, NiceAgentSetRelayInfo(_, _, _, StrEq(ice_config->turn_server.c_str()),
        ice_config->turn_port, StrEq(ice_config->turn_username.c_str()),
        StrEq(ice_config->turn_pass.c_str()))).Times(1).
        WillOnce(Return(true));

  erizo::LibNiceConnection nice(libnice_pointer,
      *ice_config);
  nice.setIceListener(nice_listener);
  nice.start();
}

TEST_F(LibNiceConnectionTest, setRemoteCandidates_Success_WhenCalled) {
  erizo::CandidateInfo arbitrary_candidate;
  arbitrary_candidate.isBundle = true;
  arbitrary_candidate.priority = 0;
  arbitrary_candidate.componentId = 1;
  arbitrary_candidate.foundation = "10";
  arbitrary_candidate.hostAddress = "192.168.1.1";
  arbitrary_candidate.rAddress = "";
  arbitrary_candidate.hostPort = 5000;
  arbitrary_candidate.rPort = 0;
  arbitrary_candidate.netProtocol = "udp";
  arbitrary_candidate.hostType = erizo::HOST;
  arbitrary_candidate.username = "hola";
  arbitrary_candidate.password = "hola";
  arbitrary_candidate.mediaType = erizo::VIDEO_TYPE;

  std::vector<erizo::CandidateInfo> candidate_list;
  candidate_list.push_back(arbitrary_candidate);

  EXPECT_CALL(*libnice, NiceAgentSetRemoteCandidates(_, _, _, _)).Times(1);
  nice_connection->setRemoteCandidates(candidate_list, true);
}

TEST_F(LibNiceConnectionTest, queuePacket_QueuedPackets_Can_Be_getPacket_When_Ready) {
  erizo::packetPtr packet;
  EXPECT_CALL(*nice_listener, updateIceState(erizo::IceState::READY , _)).Times(1);
  nice_connection->updateIceState(erizo::IceState::READY);
  EXPECT_CALL(*nice_listener, onPacketReceived(_)).WillOnce(SaveArg<0>(&packet));

  nice_connection->onData(0, test_packet, strlen(test_packet));

  ASSERT_THAT(packet.get(), Not(Eq(nullptr)));
  EXPECT_EQ(static_cast<unsigned int>(packet->length), strlen(test_packet));
  EXPECT_EQ(0, strcmp(test_packet, packet->data));
}

TEST_F(LibNiceConnectionTest, sendData_Succeed_When_Ice_Ready) {
  const unsigned int kCompId = 1;
  const int kLength = strlen(test_packet);

  EXPECT_CALL(*nice_listener, updateIceState(erizo::IceState::READY , _)).Times(1);
  nice_connection->updateIceState(erizo::IceState::READY);
  EXPECT_CALL(*libnice, NiceAgentSend(_, _, kCompId, kLength, _)).Times(1).WillOnce(Return(kLength));
  EXPECT_EQ(kLength, nice_connection->sendData(kCompId, test_packet, kLength));
}

TEST_F(LibNiceConnectionTest, sendData_Fail_When_Ice_Not_Ready) {
  const unsigned int kCompId = 1;
  const unsigned int kLength = strlen(test_packet);

  EXPECT_CALL(*libnice, NiceAgentSend(_, _, kCompId, kLength, _)).Times(0);
  EXPECT_EQ(-1, nice_connection->sendData(kCompId, test_packet, kLength));
}

TEST_F(LibNiceConnectionTest, gatheringDone_Triggers_updateIceState) {
  EXPECT_CALL(*nice_listener, updateIceState(erizo::IceState::CANDIDATES_RECEIVED , _)).Times(1);
  nice_connection->gatheringDone(1);
}

TEST_F(LibNiceConnectionTest, getSelectedPair_Calls_Libnice_And_Returns_Pair) {
  const std::string kArbitraryRemoteIp = "192.168.1.2";
  const int kArbitraryRemotePort = 4242;
  const std::string kArbitraryLocalIp = "192.168.1.1";
  const int kArbitraryLocalPort = 2222;
  const int kArbitraryPriority = 1;

  const std::string kArbitraryLocalUsername = "localuser";
  const std::string kArbitraryLocalPassword = "localpass";
  const std::string kArbitraryRemoteUsername = "remoteuser";
  const std::string kArbitraryRemotePassword = "remotepass";


  NiceCandidate* local_candidate = nice_candidate_new(NICE_CANDIDATE_TYPE_HOST);
  local_candidate->username = strdup(kArbitraryLocalUsername.c_str());
  local_candidate->password = strdup(kArbitraryLocalPassword.c_str());
  local_candidate->stream_id = (guint) 1;
  local_candidate->component_id = 1;
  local_candidate->priority = kArbitraryPriority;
  local_candidate->transport = NICE_CANDIDATE_TRANSPORT_UDP;
  nice_address_set_from_string(&local_candidate->addr, kArbitraryLocalIp.c_str());
  nice_address_set_from_string(&local_candidate->base_addr, kArbitraryLocalIp.c_str());
  nice_address_set_port(&local_candidate->addr, kArbitraryLocalPort);
  nice_address_set_port(&local_candidate->base_addr, kArbitraryLocalPort);

  NiceCandidate* remote_candidate = nice_candidate_new(NICE_CANDIDATE_TYPE_HOST);
  remote_candidate->username = strdup(kArbitraryRemoteUsername.c_str());
  remote_candidate->password = strdup(kArbitraryRemotePassword.c_str());
  remote_candidate->stream_id = (guint) 1;
  remote_candidate->component_id = 1;
  remote_candidate->priority = kArbitraryPriority;
  remote_candidate->transport = NICE_CANDIDATE_TRANSPORT_UDP;
  nice_address_set_from_string(&remote_candidate->addr, kArbitraryRemoteIp.c_str());
  nice_address_set_port(&remote_candidate->addr, kArbitraryRemotePort);
  nice_address_set_from_string(&remote_candidate->base_addr, kArbitraryRemoteIp.c_str());
  nice_address_set_port(&remote_candidate->base_addr, kArbitraryRemotePort);

  EXPECT_CALL(*libnice, NiceAgentGetSelectedPair(_, _, _, _, _)).Times(1).WillOnce(
    DoAll(SetArgPointee<3>(local_candidate), SetArgPointee<4>(remote_candidate), Return(true)));

  erizo::CandidatePair candidate_pair = nice_connection->getSelectedPair();
  EXPECT_EQ(candidate_pair.erizoCandidateIp, kArbitraryLocalIp);
  EXPECT_EQ(candidate_pair.erizoCandidatePort, kArbitraryLocalPort);
  EXPECT_EQ(candidate_pair.clientCandidateIp, kArbitraryRemoteIp);
  EXPECT_EQ(candidate_pair.clientCandidatePort, kArbitraryRemotePort);
}

TEST_F(LibNiceConnectionTest, getCandidate_Passes_Candidate_to_Listener) {
  GSList* candidate_list = NULL;
  NiceCandidate* arbitrary_candidate = nice_candidate_new(NICE_CANDIDATE_TYPE_HOST);

  arbitrary_candidate->username = strdup("an_arbitrary_username");
  arbitrary_candidate->password = strdup("an_arbitrary_password");
  arbitrary_candidate->stream_id = (guint) 1;
  arbitrary_candidate->component_id = 1;
  arbitrary_candidate->priority = 1;
  arbitrary_candidate->transport = NICE_CANDIDATE_TRANSPORT_UDP;
  nice_address_set_from_string(&arbitrary_candidate->addr, "192.168.1.1");
  nice_address_set_from_string(&arbitrary_candidate->base_addr, "192.168.1.2");
  nice_address_set_port(&arbitrary_candidate->addr, 10);
  nice_address_set_port(&arbitrary_candidate->base_addr, 20);

  candidate_list = g_slist_prepend(candidate_list, arbitrary_candidate);
  EXPECT_CALL(*libnice, NiceAgentGetLocalCandidates(_, _, _)).Times(1).WillOnce(Return(candidate_list));
  EXPECT_CALL(*nice_listener, onCandidate(_, _)).Times(1);
  nice_connection->getCandidate(1, 1, "test");
}

TEST_F(LibNiceConnectionTest, setRemoteCredentials_Configures_NiceAgent) {
  const std::string kArbitraryUsername = "username";
  const std::string kArbitraryPassword = "password";

  EXPECT_CALL(*libnice, NiceAgentSetRemoteCredentials(_, _, kArbitraryUsername.c_str(),
        kArbitraryPassword.c_str())).Times(1);
  nice_connection->setRemoteCredentials(kArbitraryUsername, kArbitraryPassword);
}

TEST_F(LibNiceConnectionTest, updateComponentState_Listener_Is_Notified_Ready_When_Single_Component_Is_Ready) {
  EXPECT_CALL(*nice_listener, updateIceState(erizo::IceState::READY , _)).Times(1);
  nice_connection->updateComponentState(1, erizo::IceState::READY);
}

TEST_F(LibNiceConnectionTest, updateComponentState_Listener_Is_Not_Notified_If_Backwards_Transition) {
  EXPECT_CALL(*nice_listener, updateIceState(erizo::IceState::READY , _)).Times(1);
  EXPECT_CALL(*nice_listener, updateIceState(erizo::IceState::CANDIDATES_RECEIVED, _)).Times(1);
  nice_connection->updateComponentState(1, erizo::IceState::CANDIDATES_RECEIVED);
  nice_connection->updateComponentState(1, erizo::IceState::READY);
  nice_connection->updateComponentState(1, erizo::IceState::CANDIDATES_RECEIVED);
}

TEST_F(LibNiceConnectionTwoComponentsTest,
    updateComponentState_LibNiceConnection_Is_Not_Ready_When_Single_Component_Is_Ready) {
  EXPECT_CALL(*nice_listener, updateIceState(erizo::IceState::READY , _)).Times(0);
  nice_connection->updateComponentState(1, erizo::IceState::READY);
}

TEST_F(LibNiceConnectionTwoComponentsTest,
       updateComponentState_LibNiceConnection_Is_Ready_When_Both_Components_Are_Ready) {
  EXPECT_CALL(*nice_listener, updateIceState(erizo::IceState::READY , _)).Times(1);
  nice_connection->updateComponentState(1, erizo::IceState::READY);
  nice_connection->updateComponentState(2, erizo::IceState::READY);
}
