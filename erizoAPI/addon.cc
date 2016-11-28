#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif
#include <nan.h>
#include "WebRtcConnection.h"
#include "OneToManyProcessor.h"
#include "OneToManyTranscoder.h"
#include "ExternalInput.h"
#include "ExternalOutput.h"
#include "ThreadPool.h"

NAN_MODULE_INIT(InitAll) {
  WebRtcConnection::Init(target);
  OneToManyProcessor::Init(target);
  ExternalInput::Init(target);
  ExternalOutput::Init(target);
  ThreadPool::Init(target);
}

NODE_MODULE(addon, InitAll)
