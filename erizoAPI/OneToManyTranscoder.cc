#define BUILDING_NODE_EXTENSION
#include <node.h>
#include "OneToManyTranscoder.h"


using namespace v8;

OneToManyTranscoder::OneToManyTranscoder() {};
OneToManyTranscoder::~OneToManyTranscoder() {};

void OneToManyTranscoder::Init(Handle<Object> target) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(String::NewSymbol("OneToManyTranscoder"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  // Prototype
  tpl->PrototypeTemplate()->Set(String::NewSymbol("close"), FunctionTemplate::New(close)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("setPublisher"), FunctionTemplate::New(setPublisher)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("hasPublisher"), FunctionTemplate::New(hasPublisher)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("addSubscriber"), FunctionTemplate::New(addSubscriber)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("removeSubscriber"), FunctionTemplate::New(removeSubscriber)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("sendFIR"), FunctionTemplate::New(sendFIR)->GetFunction());

  Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
  target->Set(String::NewSymbol("OneToManyTranscoder"), constructor);
}

Handle<Value> OneToManyTranscoder::New(const Arguments& args) {
  HandleScope scope;

  OneToManyTranscoder* obj = new OneToManyTranscoder();
  obj->me = new erizo::OneToManyTranscoder();
  obj->msink = obj->me;

  obj->Wrap(args.This());

  return args.This();
}

Handle<Value> OneToManyTranscoder::close(const Arguments& args) {
  HandleScope scope;

  OneToManyTranscoder* obj = ObjectWrap::Unwrap<OneToManyTranscoder>(args.This());
  erizo::OneToManyTranscoder *me = (erizo::OneToManyTranscoder*)obj->me;

  delete me;

  return scope.Close(Null());
}

Handle<Value> OneToManyTranscoder::setPublisher(const Arguments& args) {
  HandleScope scope;

  OneToManyTranscoder* obj = ObjectWrap::Unwrap<OneToManyTranscoder>(args.This());
  erizo::OneToManyTranscoder *me = (erizo::OneToManyTranscoder*)obj->me;

  WebRtcConnection* param = ObjectWrap::Unwrap<WebRtcConnection>(args[0]->ToObject());
  erizo::WebRtcConnection* wr = (erizo::WebRtcConnection*)param->me;

  erizo::MediaSource* ms = dynamic_cast<erizo::MediaSource*>(wr);
  me->setPublisher(ms);


  return scope.Close(Null());
}

Handle<Value> OneToManyTranscoder::hasPublisher(const Arguments& args) {
  HandleScope scope;

  OneToManyTranscoder* obj = ObjectWrap::Unwrap<OneToManyTranscoder>(args.This());
  erizo::OneToManyTranscoder *me = (erizo::OneToManyTranscoder*)obj->me;

  bool p = true;

  if(me->publisher == NULL) {
    p = false;
  }
  
  return scope.Close(Boolean::New(p));
}

Handle<Value> OneToManyTranscoder::addSubscriber(const Arguments& args) {
  HandleScope scope;

  OneToManyTranscoder* obj = ObjectWrap::Unwrap<OneToManyTranscoder>(args.This());
  erizo::OneToManyTranscoder *me = (erizo::OneToManyTranscoder*)obj->me;

  WebRtcConnection* param = ObjectWrap::Unwrap<WebRtcConnection>(args[0]->ToObject());
  erizo::WebRtcConnection* wr = param->me;

  erizo::MediaSink* ms = dynamic_cast<erizo::MediaSink*>(wr);
// get the param
  v8::String::Utf8Value param1(args[1]->ToString());

// convert it to string
  std::string peerId = std::string(*param1);
  me->addSubscriber(wr, peerId);

  return scope.Close(Null());
}

Handle<Value> OneToManyTranscoder::removeSubscriber(const Arguments& args) {
  HandleScope scope;

  OneToManyTranscoder* obj = ObjectWrap::Unwrap<OneToManyTranscoder>(args.This());
  erizo::OneToManyTranscoder *me = (erizo::OneToManyTranscoder*)obj->me;

// get the param
  v8::String::Utf8Value param1(args[0]->ToString());

// convert it to string
  std::string peerId = std::string(*param1);
  me->removeSubscriber(peerId);

  return scope.Close(Null());
}

Handle<Value> OneToManyTranscoder::sendFIR(const Arguments& args) {
  HandleScope scope;

  OneToManyTranscoder* obj = ObjectWrap::Unwrap<OneToManyTranscoder>(args.This());
  erizo::OneToManyTranscoder *me = (erizo::OneToManyTranscoder*)obj->me;

  me->publisher->sendFirPacket();
  
  return scope.Close(Null());
}
