#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <media/SyntheticInput.h>

using ::testing::_;
using ::testing::IsNull;
using ::testing::Args;
using ::testing::Return;
using erizo::SyntheticInput;
using erizo::MediaSink;
using erizo::SyntheticInputConfig;
using erizo::SimulatedClock;
using erizo::SimulatedWorker;
using erizo::Worker;

class MockMediaSink : public MediaSink {
 public:
  MOCK_METHOD0(close, void());
  MOCK_METHOD2(deliverAudioDataInternal, int(char*, int));
  MOCK_METHOD2(deliverVideoDataInternal, int(char*, int));

 private:
  int deliverAudioData_(char* buf, int len) override {
    deliverAudioDataInternal(buf, len);
  }
  int deliverVideoData_(char* buf, int len) override {
    deliverVideoDataInternal(buf, len);
  }
};

class SyntheticInputTest : public ::testing::Test {
 public:
  SyntheticInputTest() : config{30, 0, 5000}, clock{}, worker{std::make_shared<SimulatedWorker>(&clock)},
          final_worker{std::dynamic_pointer_cast<Worker>(worker)}, input{config, final_worker, &clock} {
  }

 protected:
  virtual void SetUp() {
    input.setVideoSink(&sink);
    input.setAudioSink(&sink);
    input.start();
  }

  virtual void TearDown() {
  }

  MockMediaSink sink;
  SyntheticInputConfig config;
  SimulatedClock clock;
  std::shared_ptr<SimulatedWorker> worker;
  std::shared_ptr<Worker> final_worker;
  SyntheticInput input;
};

TEST_F(SyntheticInputTest, basicBehaviourShouldWritePackets) {
  clock.advanceTime(std::chrono::milliseconds(20));
  EXPECT_CALL(sink, deliverAudioDataInternal(_, _)).Times(1);

  worker->executePastScheduledTasks();
}
