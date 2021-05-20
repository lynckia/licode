#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif
#include <nan.h>
#include <dtls/DtlsSocket.h>
#include "WebRtcConnection.h"
#include "MediaStream.h"
#include "OneToManyProcessor.h"
#include "OneToManyTranscoder.h"
#include "SyntheticInput.h"
#include "ExternalInput.h"
#include "ExternalOutput.h"
#include "ConnectionDescription.h"
#include "ThreadPool.h"
#include "IOThreadPool.h"

NAN_MODULE_INIT(InitAll) {
  dtls::DtlsSocketContext::Init();
  WebRtcConnection::Init(target);
  MediaStream::Init(target);
  OneToManyProcessor::Init(target);
  ExternalInput::Init(target);
  ExternalOutput::Init(target);
  SyntheticInput::Init(target);
  ThreadPool::Init(target);
  IOThreadPool::Init(target);
  ConnectionDescription::Init(target);
}

NODE_MODULE(addon, InitAll)
