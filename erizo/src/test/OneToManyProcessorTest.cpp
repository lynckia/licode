#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <rtp/RtpHeaders.h>
#include <MediaDefinitions.h>
#include <OneToManyProcessor.h>
#include <string>

using testing::_;
using testing::Return;
using testing::Eq;

static const char kArbitraryPeerId[] = "111";

class MockPublisher: public erizo::MediaSource, public erizo::FeedbackSink {
 public:
  MockPublisher() {
    videoSourceSSRC_ = 1;
    audioSourceSSRC_ = 2;
    sourcefbSink_ = this;
  }
  ~MockPublisher() {}
  int sendPLI() { return 0; }

  MOCK_METHOD2(deliverFeedback_, int(char*, int));
};

class MockSubscriber: public erizo::MediaSink, public erizo::FeedbackSource {
 public:
  MockSubscriber() {
    sinkfbSource_ = this;
  }
  ~MockSubscriber() {}

  MOCK_METHOD2(deliverAudioData_, int(char*, int));
  MOCK_METHOD2(deliverVideoData_, int(char*, int));
};

class OneToManyProcessorTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    publisher = new MockPublisher();
    subscriber = new MockSubscriber();
    otm.setPublisher(publisher);
    otm.addSubscriber(subscriber, kArbitraryPeerId);
  }
  virtual void TearDown() {}

  MockSubscriber* getSubscriber(std::string peer_id) {
    if (otm.subscribers.find(peer_id) != otm.subscribers.end()) {
      return reinterpret_cast<MockSubscriber*>(
                        otm.subscribers.find(peer_id)->second.get());
    }
    return NULL;
  }
  MockPublisher *publisher;
  MockSubscriber *subscriber;
  erizo::OneToManyProcessor otm;
};

TEST_F(OneToManyProcessorTest, setPublisher_Success_WhenCalled) {
  EXPECT_THAT(otm.publisher.get(), Eq(publisher));
}

TEST_F(OneToManyProcessorTest, addSubscriber_Success_WhenAddingNewSubscribers) {
  EXPECT_THAT(getSubscriber(kArbitraryPeerId), Eq(subscriber));
}

TEST_F(OneToManyProcessorTest, removeSubscriber_RemovesSubscribers_WhenSubscribersExist) {
  otm.removeSubscriber(kArbitraryPeerId);

  EXPECT_EQ(NULL, getSubscriber(kArbitraryPeerId));
}

TEST_F(OneToManyProcessorTest, addSubscriber_Substitutes_WhenRepeated) {
  MockSubscriber *new_subscriber = new MockSubscriber();

  otm.addSubscriber(new_subscriber, kArbitraryPeerId);

  EXPECT_THAT(getSubscriber(kArbitraryPeerId), Eq(new_subscriber));
}

TEST_F(OneToManyProcessorTest, deliverFeedback_CallsPublisher_WhenCalled) {
  erizo::RtpHeader header;
  header.setSeqNumber(12);

  EXPECT_CALL(*publisher, deliverFeedback_(_, _)).Times(1).WillOnce(Return(0));
  otm.deliverFeedback(reinterpret_cast<char*>(&header),
                      sizeof(erizo::RtpHeader));
}

TEST_F(OneToManyProcessorTest, deliverVideoData_CallsSubscriber_whenCalled) {
  erizo::RtpHeader header;
  header.setSeqNumber(12);

  EXPECT_CALL(*subscriber, deliverVideoData_(_, _)).Times(1).WillOnce(Return(0));
  otm.deliverVideoData(reinterpret_cast<char*>(&header),
                       sizeof(erizo::RtpHeader));
}

TEST_F(OneToManyProcessorTest, deliverAudioData_CallsSubscriber_whenCalled) {
  erizo::RtpHeader header;
  header.setSeqNumber(12);

  EXPECT_CALL(*subscriber, deliverAudioData_(_, _)).Times(1).WillOnce(Return(0));
  otm.deliverAudioData(reinterpret_cast<char*>(&header),
                       sizeof(erizo::RtpHeader));
}
