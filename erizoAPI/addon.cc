#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif
#include <nan.h>
#include "WebRtcConnection.h"
#include "OneToManyProcessor.h"
#include "OneToManyTranscoder.h"
#include "ExternalInput.h"
#include "ExternalOutput.h"


void InitAll(v8::Local<v8::Object> exports) {
  WebRtcConnection::Init(exports);
  OneToManyProcessor::Init(exports);
  OneToManyTranscoder::Init(exports);
  ExternalInput::Init(exports);
  ExternalOutput::Init(exports);
}

NODE_MODULE(addon, InitAll)
