#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif

#include "OneToManyProcessor.h"


using namespace v8;

Nan::Persistent<Function> OneToManyProcessor::constructor;

OneToManyProcessor::OneToManyProcessor() {
};

OneToManyProcessor::~OneToManyProcessor() {
};

void OneToManyProcessor::Init(v8::Local<v8::Object> exports) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("OneToManyProcessor").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "close", close);
  Nan::SetPrototypeMethod(tpl, "setPublisher", close);
  Nan::SetPrototypeMethod(tpl, "addExternalOutput", close);
  Nan::SetPrototypeMethod(tpl, "setExternalPublisher", close);
  Nan::SetPrototypeMethod(tpl, "getPublisherState", close);
  Nan::SetPrototypeMethod(tpl, "hasPublisher", close);
  Nan::SetPrototypeMethod(tpl, "addSubscriber", close);
  Nan::SetPrototypeMethod(tpl, "removeSubscriber", close);

  constructor.Reset(tpl->GetFunction());
  exports->Set(Nan::New("OneToManyProcessor").ToLocalChecked(), tpl->GetFunction());
}

void OneToManyProcessor::New(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  OneToManyProcessor* obj = new OneToManyProcessor();
  obj->me = new erizo::OneToManyProcessor();
  obj->msink = obj->me;

  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

void OneToManyProcessor::close(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  OneToManyProcessor* obj = ObjectWrap::Unwrap<OneToManyProcessor>(info.Holder());
  erizo::OneToManyProcessor *me = (erizo::OneToManyProcessor*)obj->me;
  //TODO Make async 
  delete me;
}

void OneToManyProcessor::setPublisher(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  OneToManyProcessor* obj = ObjectWrap::Unwrap<OneToManyProcessor>(info.Holder());
  erizo::OneToManyProcessor *me = (erizo::OneToManyProcessor*)obj->me;

  WebRtcConnection* param = ObjectWrap::Unwrap<WebRtcConnection>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::WebRtcConnection* wr = (erizo::WebRtcConnection*)param->me;

  erizo::MediaSource* ms = dynamic_cast<erizo::MediaSource*>(wr);
  me->setPublisher(ms);
}

void OneToManyProcessor::addExternalOutput(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  OneToManyProcessor* obj = ObjectWrap::Unwrap<OneToManyProcessor>(info.Holder());
  erizo::OneToManyProcessor *me = (erizo::OneToManyProcessor*)obj->me;

  ExternalOutput* param = ObjectWrap::Unwrap<ExternalOutput>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::ExternalOutput* wr = param->me;

  erizo::MediaSink* ms = dynamic_cast<erizo::MediaSink*>(wr);

// get the param
  v8::String::Utf8Value param1(Nan::To<v8::String>(info[1]).ToLocalChecked());

// convert it to string
  std::string peerId = std::string(*param1);
  me->addSubscriber(ms, peerId);
}

void OneToManyProcessor::setExternalPublisher(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  OneToManyProcessor* obj = ObjectWrap::Unwrap<OneToManyProcessor>(info.Holder());
  erizo::OneToManyProcessor *me = (erizo::OneToManyProcessor*)obj->me;

  ExternalInput* param = ObjectWrap::Unwrap<ExternalInput>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::ExternalInput* wr = (erizo::ExternalInput*)param->me;

  erizo::MediaSource* ms = dynamic_cast<erizo::MediaSource*>(wr);
  me->setPublisher(ms);

}

void OneToManyProcessor::getPublisherState(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  OneToManyProcessor* obj = ObjectWrap::Unwrap<OneToManyProcessor>(info.Holder());
  erizo::OneToManyProcessor *me = (erizo::OneToManyProcessor*)obj->me;

  erizo::MediaSource * ms = me->publisher.get();

  erizo::WebRtcConnection* wr = (erizo::WebRtcConnection*)ms;

  int state = wr->getCurrentState();
  info.GetReturnValue().Set(Nan::New(state));
}

 void OneToManyProcessor::hasPublisher(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  OneToManyProcessor* obj = ObjectWrap::Unwrap<OneToManyProcessor>(info.Holder());
  erizo::OneToManyProcessor *me = (erizo::OneToManyProcessor*)obj->me;

  bool p = true;

  if(me->publisher == NULL) {
    p = false;
  }
  
  info.GetReturnValue().Set(Nan::New(p));
}

 void OneToManyProcessor::addSubscriber(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  OneToManyProcessor* obj = ObjectWrap::Unwrap<OneToManyProcessor>(info.Holder());
  erizo::OneToManyProcessor *me = (erizo::OneToManyProcessor*)obj->me;

  WebRtcConnection* param = ObjectWrap::Unwrap<WebRtcConnection>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::WebRtcConnection* wr = (erizo::WebRtcConnection*)param->me;

  erizo::MediaSink* ms = dynamic_cast<erizo::MediaSink*>(wr);

// get the param
  v8::String::Utf8Value param1(Nan::To<v8::String>(info[1]).ToLocalChecked());

// convert it to string
  std::string peerId = std::string(*param1);
  me->addSubscriber(ms, peerId);
}

void OneToManyProcessor::removeSubscriber(const Nan::FunctionCallbackInfo<v8::Value>& info) {
  OneToManyProcessor* obj = ObjectWrap::Unwrap<OneToManyProcessor>(info.Holder());
  erizo::OneToManyProcessor *me = (erizo::OneToManyProcessor*)obj->me;

// get the param
  v8::String::Utf8Value param1(Nan::To<v8::String>(info[0]).ToLocalChecked());

// convert it to string
  std::string peerId = std::string(*param1);
  me->removeSubscriber(peerId);
}
