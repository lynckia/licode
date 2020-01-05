#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <dtls/DtlsSocket.h>
#include <string>
#include <thread>  // NOLINT

using testing::_;
using testing::Return;
using testing::Eq;

void createDtlsClient() {
  std::unique_ptr<dtls::DtlsSocketContext> dtls_rtp;
  dtls_rtp.reset(new dtls::DtlsSocketContext());
  dtls_rtp->createClient();
}

class DtlsSocketTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    dtls::DtlsSocketContext::Init();
  }

  virtual void TearDown() {
    dtls::DtlsSocketContext::Destroy();
  }

  void runTest(int number_of_clients) {
    std::thread threads[number_of_clients];  // NOLINT
    for (int j = 0; j < number_of_clients; j++) {
      threads[j] = std::thread(createDtlsClient);
    }
    for (int j = 0; j < number_of_clients; j++) {
      threads[j].join();
    }
  }
};

TEST_F(DtlsSocketTest, create_1000_DtlsClient) {
  runTest(1000);
  EXPECT_THAT(true, Eq(true));
}
