#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <NicerConnection.h>
#include <lib/NicerInterface.h>

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

class MockNicer: public erizo::NicerInterface {
 public:
  MockNicer() {
    erizo::NicerConnection::initializeGlobals();
    ON_CALL(*this, IceContextCreate(_, _, _)).WillByDefault(Invoke(&real_impl_,
                            &erizo::NicerInterfaceImpl::IceContextCreate));
    ON_CALL(*this, IcePeerContextCreate(_, _, _, _)).WillByDefault(Invoke(&real_impl_,
                            &erizo::NicerInterfaceImpl::IcePeerContextCreate));

    ON_CALL(*this, IceContextDestroy(_)).WillByDefault(Invoke(&real_impl_,
                            &erizo::NicerInterfaceImpl::IceContextDestroy));
    ON_CALL(*this, IcePeerContextDestroy(_)).WillByDefault(Invoke(&real_impl_,
                            &erizo::NicerInterfaceImpl::IcePeerContextDestroy));
  }
  virtual ~MockNicer() {
  }

  MOCK_METHOD3(IceContextCreate, int(char *, UINT4, nr_ice_ctx **));
  MOCK_METHOD1(IceContextDestroy, int(nr_ice_ctx **));
  MOCK_METHOD3(IceContextSetTrickleCallback, int(nr_ice_ctx *, nr_ice_trickle_candidate_cb, void *));
  MOCK_METHOD2(IceContextSetSocketFactory, void(nr_ice_ctx *, nr_socket_factory *));
  MOCK_METHOD2(IceContextFinalize, void(nr_ice_ctx *, nr_ice_peer_ctx *));
  MOCK_METHOD3(IceContextSetStunServers, int(nr_ice_ctx *, nr_ice_stun_server *, int));
  MOCK_METHOD3(IceContextSetTurnServers, int(nr_ice_ctx *, nr_ice_turn_server *, int));
  MOCK_METHOD3(IceContextSetPortRange, void(nr_ice_ctx *, uint16_t, uint16_t));
  MOCK_METHOD4(IcePeerContextCreate, int(nr_ice_ctx *, nr_ice_handler *, char *, nr_ice_peer_ctx **));
  MOCK_METHOD1(IcePeerContextDestroy, int(nr_ice_peer_ctx **));
  MOCK_METHOD4(IcePeerContextParseTrickleCandidate, int(nr_ice_peer_ctx *, nr_ice_media_stream *, char *, const char*));
  MOCK_METHOD1(IcePeerContextPairCandidates, int(nr_ice_peer_ctx *));
  MOCK_METHOD2(IcePeerContextStartChecks2, int(nr_ice_peer_ctx *, int));
  MOCK_METHOD4(IcePeerContextParseStreamAttributes, int(nr_ice_peer_ctx *, nr_ice_media_stream *,
                                                        char **, size_t));

  MOCK_METHOD1(IceGetNewIceUFrag, int(char **));
  MOCK_METHOD1(IceGetNewIcePwd, int(char **));

  MOCK_METHOD3(IceGather, int(nr_ice_ctx *, NR_async_cb, void *));
  MOCK_METHOD6(IceAddMediaStream, int(nr_ice_ctx *, const char *, const char *, const char *, int,
        nr_ice_media_stream **));
  MOCK_METHOD5(IceMediaStreamSend, int(nr_ice_peer_ctx *, nr_ice_media_stream *, int,
                                       unsigned char *, size_t));
  MOCK_METHOD2(IceRemoveMediaStream, int(nr_ice_ctx *, nr_ice_media_stream **));
  MOCK_METHOD5(IceMediaStreamGetActive, int(nr_ice_peer_ctx *, nr_ice_media_stream *, int,
                                             nr_ice_candidate **, nr_ice_candidate **));

 private:
  erizo::NicerInterfaceImpl real_impl_;
};

class MockNicerConnectionListener: public erizo::IceConnectionListener {
 public:
  MockNicerConnectionListener() {
  }
  virtual ~MockNicerConnectionListener() {
  }
  MOCK_METHOD1(onPacketReceived, void(erizo::packetPtr packet));
  MOCK_METHOD2(onCandidate, void(const erizo::CandidateInfo&, erizo::IceConnection*));
  MOCK_METHOD2(updateIceState, void(erizo::IceState, erizo::IceConnection*));
};

class NicerConnectionStartTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    nicer = new MockNicer;
    nicer_pointer.reset(nicer);
    nicer_listener = std::make_shared<MockNicerConnectionListener>();
    ice_config = new erizo::IceConfig();
    io_worker = std::make_shared<erizo::IOWorker>();
    io_worker->start();
  }
  void waitUntilClosed(std::shared_ptr<erizo::NicerConnection> nicer) {
    while (!nicer->isClosed()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
  virtual void TearDown() {
    delete ice_config;
  }

  std::shared_ptr<erizo::NicerInterface> nicer_pointer;
  MockNicer* nicer;
  std::shared_ptr<MockNicerConnectionListener> nicer_listener;
  erizo::IceConfig* ice_config;
  std::shared_ptr<erizo::IOWorker> io_worker;
};

class NicerConnectionTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    const std::string kArbitraryLocalCredentialUsername = "ufrag";
    const std::string kArbitraryLocalCredentialPassword = "upass";
    const std::string kArbitraryConnectionId = "a_connection_id";
    const std::string kArbitraryTransportName = "video";
    const std::string kArbitraryDataPacket = "test";

    nicer = new MockNicer;
    nicer_pointer.reset(nicer);
    nicer_listener = std::make_shared<MockNicerConnectionListener>();
    ice_config = new erizo::IceConfig();
    ufrag = r_strdup(strdup(kArbitraryLocalCredentialUsername.c_str()));
    pass = r_strdup(strdup(kArbitraryLocalCredentialPassword.c_str()));
    test_packet = strdup(kArbitraryDataPacket.c_str());

    ice_config->media_type = erizo::VIDEO_TYPE;
    ice_config->transport_name = kArbitraryTransportName;
    ice_config->ice_components = 1;
    ice_config->connection_id = kArbitraryConnectionId;

    EXPECT_CALL(*nicer, IceGetNewIceUFrag(_)).Times(1).WillOnce(DoAll(SetArgPointee<0>(ufrag), Return(0)));
    EXPECT_CALL(*nicer, IceGetNewIcePwd(_)).Times(1).WillOnce(DoAll(SetArgPointee<0>(pass), Return(0)));
    EXPECT_CALL(*nicer, IceContextCreate(_, _, _)).Times(1);
    EXPECT_CALL(*nicer, IceContextSetTrickleCallback(_, _, _)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(*nicer, IcePeerContextCreate(_, _, _, _)).Times(1);
    EXPECT_CALL(*nicer, IceAddMediaStream(_, _, _, _, _, _)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(*nicer, IcePeerContextParseStreamAttributes(_, _, _, _)).Times(0);
    EXPECT_CALL(*nicer, IceGather(_, _, _)).Times(1).WillOnce(Return(0));
    EXPECT_CALL(*nicer, IcePeerContextDestroy(_)).Times(1);
    EXPECT_CALL(*nicer, IceContextDestroy(_)).Times(1);

    io_worker = std::make_shared<erizo::IOWorker>();
    io_worker->start();

    nicer_connection = std::make_shared<erizo::NicerConnection>(io_worker, nicer_pointer,
        *ice_config);
    nicer_connection->setIceListener(nicer_listener);
    nicer_connection->start();
  }

  virtual void TearDown() {
    nicer_connection->close();
    while (!nicer_connection->isClosed()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    delete ice_config;
    free(test_packet);
  }

  std::shared_ptr<erizo::NicerInterface> nicer_pointer;
  MockNicer* nicer;
  std::shared_ptr<MockNicerConnectionListener> nicer_listener;
  erizo::IceConfig* ice_config;
  std::shared_ptr<erizo::NicerConnection> nicer_connection;
  std::shared_ptr<erizo::IOWorker> io_worker;
  char* ufrag, * pass, * test_packet;
};

TEST_F(NicerConnectionStartTest, start_Configures_Libnice_With_Default_Config) {
  const std::string kArbitraryLocalCredentialUsername = "ufrag";
  const std::string kArbitraryLocalCredentialPassword = "upass";
  const std::string kArbitraryConnectionId = "a_connection_id";
  const std::string kArbitraryTransportName = "video";

  char *ufrag = r_strdup(strdup(kArbitraryLocalCredentialUsername.c_str()));
  char *pass = r_strdup(strdup(kArbitraryLocalCredentialPassword.c_str()));

  ice_config->media_type = erizo::VIDEO_TYPE;
  ice_config->transport_name = kArbitraryTransportName;
  ice_config->ice_components = 1;
  ice_config->connection_id = kArbitraryConnectionId;

  EXPECT_CALL(*nicer, IceGetNewIceUFrag(_)).Times(1).WillOnce(DoAll(SetArgPointee<0>(ufrag), Return(0)));
  EXPECT_CALL(*nicer, IceGetNewIcePwd(_)).Times(1).WillOnce(DoAll(SetArgPointee<0>(pass), Return(0)));
  EXPECT_CALL(*nicer, IceContextCreate(_, _, _)).Times(1);
  EXPECT_CALL(*nicer, IceContextSetTrickleCallback(_, _, _)).Times(1).WillOnce(Return(0));
  EXPECT_CALL(*nicer, IcePeerContextCreate(_, _, _, _)).Times(1);
  EXPECT_CALL(*nicer, IceAddMediaStream(_, _, _, _, _, _)).Times(1).WillOnce(Return(0));
  EXPECT_CALL(*nicer, IceGather(_, _, _)).Times(1).WillOnce(Return(0));

  EXPECT_CALL(*nicer, IcePeerContextParseStreamAttributes(_, _, _, _)).Times(0);
  EXPECT_CALL(*nicer, IceContextSetTurnServers(_, _, _)).Times(0);

  EXPECT_CALL(*nicer, IcePeerContextDestroy(_)).Times(1);
  EXPECT_CALL(*nicer, IceContextDestroy(_)).Times(1);

  auto nice = std::make_shared<erizo::NicerConnection>(io_worker, nicer_pointer, *ice_config);
  nice->setIceListener(nicer_listener);
  nice->start();
  nice->close();
  waitUntilClosed(nice);
}

TEST_F(NicerConnectionStartTest, start_Configures_Libnice_With_Remote_Credentials) {
  const std::string kArbitraryLocalCredentialUsername = "ufrag";
  const std::string kArbitraryLocalCredentialPassword = "upass";
  const std::string kArbitraryRemoteCredentialUsername = "remote_ufrag";
  const std::string kArbitraryRemoteCredentialPassword = "remote_upass";

  const std::string kArbitraryConnectionId = "a_connection_id";
  const std::string kArbitraryTransportName = "video";

  char *ufrag = r_strdup(strdup(kArbitraryLocalCredentialUsername.c_str()));
  char *pass = r_strdup(strdup(kArbitraryLocalCredentialPassword.c_str()));

  ice_config->media_type = erizo::VIDEO_TYPE;
  ice_config->transport_name = kArbitraryTransportName;
  ice_config->ice_components = 1;
  ice_config->connection_id = kArbitraryConnectionId;

  ice_config->username = kArbitraryRemoteCredentialUsername;
  ice_config->password = kArbitraryRemoteCredentialPassword;

  EXPECT_CALL(*nicer, IceGetNewIceUFrag(_)).Times(1).WillOnce(DoAll(SetArgPointee<0>(ufrag), Return(0)));
  EXPECT_CALL(*nicer, IceGetNewIcePwd(_)).Times(1).WillOnce(DoAll(SetArgPointee<0>(pass), Return(0)));
  EXPECT_CALL(*nicer, IceContextCreate(_, _, _)).Times(1);
  EXPECT_CALL(*nicer, IceContextSetTrickleCallback(_, _, _)).Times(1).WillOnce(Return(0));
  EXPECT_CALL(*nicer, IcePeerContextCreate(_, _, _, _)).Times(1);
  EXPECT_CALL(*nicer, IceAddMediaStream(_, _, _, _, _, _)).Times(1).WillOnce(Return(0));

  EXPECT_CALL(*nicer, IceGather(_, _, _)).Times(1).WillOnce(Return(0));
  EXPECT_CALL(*nicer, IcePeerContextPairCandidates(_)).Times(1).WillOnce(Return(0));
  EXPECT_CALL(*nicer, IcePeerContextStartChecks2(_, _)).Times(1).WillOnce(Return(0));

  EXPECT_CALL(*nicer, IcePeerContextParseStreamAttributes(_, _, _, _)).Times(1).WillOnce(Return(0));
  EXPECT_CALL(*nicer, IceContextSetTurnServers(_, _, _)).Times(0);

  EXPECT_CALL(*nicer, IcePeerContextDestroy(_)).Times(1);
  EXPECT_CALL(*nicer, IceContextDestroy(_)).Times(1);

  auto nice = std::make_shared<erizo::NicerConnection>(io_worker, nicer_pointer, *ice_config);
  nice->setIceListener(nicer_listener);
  nice->start();
  nice->close();
  waitUntilClosed(nice);
}

TEST_F(NicerConnectionStartTest, start_Configures_Nicer_With_Turn) {
  const std::string kArbitraryLocalCredentialUsername = "ufrag";
  const std::string kArbitraryLocalCredentialPassword = "upass";
  const std::string kArbitraryConnectionId = "a_connection_id";
  const std::string kArbitraryTransportName = "video";
  const std::string kArbitraryTurnServerUrl = "aturnserver.com";
  const uint16_t kArbitraryTurnPort = 443;
  const std::string kArbitraryTurnUsername = "a_turn_username";
  const std::string kArbitraryTurnPassword = "a_turn_password";

  char *ufrag = r_strdup(strdup(kArbitraryLocalCredentialUsername.c_str()));
  char *pass = r_strdup(strdup(kArbitraryLocalCredentialPassword.c_str()));

  ice_config->turn_server = kArbitraryTurnServerUrl;
  ice_config->turn_port = kArbitraryTurnPort;
  ice_config->turn_username = kArbitraryTurnUsername;
  ice_config->turn_pass = kArbitraryTurnPassword;
  ice_config->transport_name = kArbitraryTransportName;
  ice_config->media_type = erizo::VIDEO_TYPE;
  ice_config->connection_id = kArbitraryConnectionId;
  ice_config->ice_components = 1;


  EXPECT_CALL(*nicer, IceGetNewIceUFrag(_)).Times(1).WillOnce(DoAll(SetArgPointee<0>(ufrag), Return(0)));
  EXPECT_CALL(*nicer, IceGetNewIcePwd(_)).Times(1).WillOnce(DoAll(SetArgPointee<0>(pass), Return(0)));
  EXPECT_CALL(*nicer, IceContextCreate(_, _, _)).Times(1);
  EXPECT_CALL(*nicer, IceContextSetTrickleCallback(_, _, _)).Times(1).WillOnce(Return(0));
  EXPECT_CALL(*nicer, IcePeerContextCreate(_, _, _, _)).Times(1);
  EXPECT_CALL(*nicer, IceAddMediaStream(_, _, _, _, _, _)).Times(1).WillOnce(Return(0));
  EXPECT_CALL(*nicer, IceGather(_, _, _)).Times(1).WillOnce(Return(0));

  EXPECT_CALL(*nicer, IcePeerContextParseStreamAttributes(_, _, _, _)).Times(0);
  EXPECT_CALL(*nicer, IceContextSetTurnServers(_, _, _)).Times(1).WillOnce(Return(0));

  EXPECT_CALL(*nicer, IcePeerContextDestroy(_)).Times(1);
  EXPECT_CALL(*nicer, IceContextDestroy(_)).Times(1);

  auto nice = std::make_shared<erizo::NicerConnection>(io_worker, nicer_pointer, *ice_config);
  nice->setIceListener(nicer_listener);
  nice->start();
  nice->close();
  waitUntilClosed(nice);
}

TEST_F(NicerConnectionStartTest, start_Configures_Nicer_With_Stun) {
  const std::string kArbitraryLocalCredentialUsername = "ufrag";
  const std::string kArbitraryLocalCredentialPassword = "upass";
  const std::string kArbitraryConnectionId = "a_connection_id";
  const std::string kArbitraryTransportName = "video";
  const std::string kArbitraryStunServerUrl = "astunserver.com";
  const uint16_t kArbitraryStunPort = 443;

  char *ufrag = r_strdup(strdup(kArbitraryLocalCredentialUsername.c_str()));
  char *pass = r_strdup(strdup(kArbitraryLocalCredentialPassword.c_str()));

  ice_config->stun_server = kArbitraryStunServerUrl;
  ice_config->stun_port = kArbitraryStunPort;
  ice_config->transport_name = kArbitraryTransportName;
  ice_config->media_type = erizo::VIDEO_TYPE;
  ice_config->connection_id = kArbitraryConnectionId;
  ice_config->ice_components = 1;


  EXPECT_CALL(*nicer, IceGetNewIceUFrag(_)).Times(1).WillOnce(DoAll(SetArgPointee<0>(ufrag), Return(0)));
  EXPECT_CALL(*nicer, IceGetNewIcePwd(_)).Times(1).WillOnce(DoAll(SetArgPointee<0>(pass), Return(0)));
  EXPECT_CALL(*nicer, IceContextCreate(_, _, _)).Times(1);
  EXPECT_CALL(*nicer, IceContextSetTrickleCallback(_, _, _)).Times(1).WillOnce(Return(0));
  EXPECT_CALL(*nicer, IcePeerContextCreate(_, _, _, _)).Times(1);
  EXPECT_CALL(*nicer, IceAddMediaStream(_, _, _, _, _, _)).Times(1).WillOnce(Return(0));
  EXPECT_CALL(*nicer, IceGather(_, _, _)).Times(1).WillOnce(Return(0));

  EXPECT_CALL(*nicer, IcePeerContextParseStreamAttributes(_, _, _, _)).Times(0);
  EXPECT_CALL(*nicer, IceContextSetStunServers(_, _, _)).Times(1).WillOnce(Return(0));

  EXPECT_CALL(*nicer, IcePeerContextDestroy(_)).Times(1);
  EXPECT_CALL(*nicer, IceContextDestroy(_)).Times(1);

  auto nice = std::make_shared<erizo::NicerConnection>(io_worker, nicer_pointer, *ice_config);
  nice->setIceListener(nicer_listener);
  nice->start();
  nice->close();
  waitUntilClosed(nice);
}

TEST_F(NicerConnectionTest, setRemoteCandidates_Success_WhenCalled) {
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

  EXPECT_CALL(*nicer, IcePeerContextParseTrickleCandidate(_, _, _, _)).Times(1);
  nicer_connection->setRemoteCandidates(candidate_list, true);
}

TEST_F(NicerConnectionTest, setRemoteSdpCandidates_Success_WhenCalled) {
  erizo::CandidateInfo arbitrary_candidate;
  arbitrary_candidate.isBundle = true;
  arbitrary_candidate.priority = 0;
  arbitrary_candidate.componentId = 1;
  arbitrary_candidate.foundation = "10";
  arbitrary_candidate.hostAddress = "7be847e2.local";
  arbitrary_candidate.rAddress = "";
  arbitrary_candidate.hostPort = 5000;
  arbitrary_candidate.rPort = 0;
  arbitrary_candidate.netProtocol = "udp";
  arbitrary_candidate.hostType = erizo::HOST;
  arbitrary_candidate.username = "hola";
  arbitrary_candidate.password = "hola";
  arbitrary_candidate.mediaType = erizo::VIDEO_TYPE;
  arbitrary_candidate.sdp =
    "a=candidate:547260449 1 udp 21131 7be847e2.local 53219 typ host generation 0 ufrag JVl4 network-cost 999";

  std::vector<erizo::CandidateInfo> candidate_list;
  candidate_list.push_back(arbitrary_candidate);

  EXPECT_CALL(*nicer, IcePeerContextParseTrickleCandidate(_, _, _, _)).Times(1);
  nicer_connection->setRemoteCandidates(candidate_list, true);
}

TEST_F(NicerConnectionTest, queuePacket_QueuedPackets_Can_Be_getPacket_When_Ready) {
  erizo::packetPtr packet;
  EXPECT_CALL(*nicer_listener, updateIceState(erizo::IceState::READY , _)).Times(1);
  nicer_connection->updateIceState(erizo::IceState::READY);
  EXPECT_CALL(*nicer_listener, onPacketReceived(_)).WillOnce(SaveArg<0>(&packet));

  nicer_connection->onData(0, test_packet, strlen(test_packet));

  ASSERT_THAT(packet.get(), Not(Eq(nullptr)));
  EXPECT_EQ(static_cast<unsigned int>(packet->length), strlen(test_packet));
  EXPECT_EQ(0, strcmp(test_packet, packet->data));
}

TEST_F(NicerConnectionTest, sendData_Succeed_When_Ice_Ready) {
  const unsigned int kCompId = 1;
  const int kLength = strlen(test_packet);

  EXPECT_CALL(*nicer_listener, updateIceState(erizo::IceState::READY , _)).Times(1);
  nicer_connection->updateIceState(erizo::IceState::READY);
  EXPECT_CALL(*nicer, IceMediaStreamSend(_, _, kCompId, _, kLength)).Times(1).WillOnce(Return(0));
  EXPECT_EQ(kLength, nicer_connection->sendData(kCompId, test_packet, kLength));
}

TEST_F(NicerConnectionTest, sendData_Fail_When_Ice_Not_Ready) {
  const unsigned int kCompId = 1;
  const unsigned int kLength = strlen(test_packet);

  EXPECT_CALL(*nicer, IceMediaStreamSend(_, _, kCompId, _, kLength)).Times(0);
  EXPECT_EQ(-1, nicer_connection->sendData(kCompId, test_packet, kLength));
}

TEST_F(NicerConnectionTest, gatheringDone_Triggers_updateIceState) {
  EXPECT_CALL(*nicer_listener, updateIceState(erizo::IceState::CANDIDATES_RECEIVED, _)).Times(1);
  nicer_connection->gatheringDone(1);
}

static int create_nr_ice_candidate(UINT4 component_id, const char *ip, UINT2 port, nr_ice_candidate **candp) {
  nr_ice_candidate *cand = nullptr;
  nr_transport_addr *addr = nullptr;
  int r, _status;

  if (!(cand=reinterpret_cast<nr_ice_candidate*>(RCALLOC(sizeof(nr_ice_candidate)))))
    ABORT(1);

  cand->state = NR_ICE_CAND_STATE_INITIALIZED;
  cand->type = PEER_REFLEXIVE;
  cand->component_id = component_id;

  if (!(addr = reinterpret_cast<nr_transport_addr*>(RCALLOC(sizeof(nr_transport_addr)))))
    ABORT(2);

  addr->ip_version = NR_IPV4;

  if ((r = nr_str_port_to_transport_addr(ip, port, 17, addr)))
    ABORT(3);
  if ((r = nr_transport_addr_copy(&cand->base, addr)))
    ABORT(4);
  if ((r = nr_transport_addr_copy(&cand->addr, addr)))
    ABORT(5);
  /* Bogus foundation */
  if (!(cand->foundation = r_strdup(cand->addr.as_string)))
    ABORT(6);

  nr_ice_candidate_compute_codeword(cand);

  *candp = cand;

  _status = 0;
abort:
  if (_status) {
    nr_ice_candidate_destroy(&cand);
  }
  RFREE(addr);
  return _status;
}

TEST_F(NicerConnectionTest, getSelectedPair_Calls_Nicer_And_Returns_Pair) {
  const std::string kArbitraryRemoteIp = "192.168.1.2";
  const int kArbitraryRemotePort = 4242;
  const std::string kArbitraryLocalIp = "192.168.1.1";
  const int kArbitraryLocalPort = 2222;

  nr_ice_candidate* local_candidate;
  nr_ice_candidate* remote_candidate;
  ASSERT_EQ(create_nr_ice_candidate(1, strdup(kArbitraryLocalIp.c_str()),  kArbitraryLocalPort,  &local_candidate), 0);
  ASSERT_EQ(create_nr_ice_candidate(1, strdup(kArbitraryRemoteIp.c_str()), kArbitraryRemotePort, &remote_candidate), 0);

  EXPECT_CALL(*nicer, IceMediaStreamGetActive(_, _, _, _, _)).Times(1).WillOnce(
    DoAll(SetArgPointee<3>(local_candidate), SetArgPointee<4>(remote_candidate), Return(true)));

  erizo::CandidatePair candidate_pair = nicer_connection->getSelectedPair();
  EXPECT_EQ(candidate_pair.erizoCandidateIp, kArbitraryLocalIp);
  EXPECT_EQ(candidate_pair.erizoCandidatePort, kArbitraryLocalPort);
  EXPECT_EQ(candidate_pair.clientCandidateIp, kArbitraryRemoteIp);
  EXPECT_EQ(candidate_pair.clientCandidatePort, kArbitraryRemotePort);
}


TEST_F(NicerConnectionTest, setRemoteCredentials_Configures_NicerCandidate) {
  const std::string kArbitraryUsername = "username";
  const std::string kArbitraryPassword = "password";

  EXPECT_CALL(*nicer, IcePeerContextParseStreamAttributes(_, _, _, _)).Times(1);
  EXPECT_CALL(*nicer, IcePeerContextPairCandidates(_)).Times(1).WillOnce(Return(0));
  EXPECT_CALL(*nicer, IcePeerContextStartChecks2(_, _)).Times(1).WillOnce(Return(0));
  nicer_connection->setRemoteCredentials(kArbitraryUsername, kArbitraryPassword);
}
