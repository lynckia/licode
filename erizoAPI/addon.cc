#define BUILDING_NODE_EXTENSION
#include <node.h>
#include "WebRtcConnection.h"
#include "OneToManyProcessor.h"
#include "OneToManyTranscoder.h"

using namespace v8;

void InitAll(Handle<Object> target) {
  WebRtcConnection::Init(target);
  OneToManyProcessor::Init(target);
  OneToManyTranscoder::Init(target);
}

NODE_MODULE(addon, InitAll)
