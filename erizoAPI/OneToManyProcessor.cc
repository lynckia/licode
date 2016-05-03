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

NAN_MODULE_INIT (OneToManyProcessor::Init) {
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
  Nan::Set(target, Nan::New("OneToManyProcessor").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(OneToManyProcessor::New) {
  OneToManyProcessor* obj = new OneToManyProcessor();
  obj->me = new erizo::OneToManyProcessor();
  obj->msink = obj->me;

  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(OneToManyProcessor::close) {
  OneToManyProcessor* obj = Nan::ObjectWrap::Unwrap<OneToManyProcessor>(info.Holder());
  erizo::OneToManyProcessor *me = (erizo::OneToManyProcessor*)obj->me;
  //TODO Make async 
  delete me;
}

NAN_METHOD(OneToManyProcessor::setPublisher) {
  OneToManyProcessor* obj = Nan::ObjectWrap::Unwrap<OneToManyProcessor>(info.Holder());
  erizo::OneToManyProcessor *me = (erizo::OneToManyProcessor*)obj->me;

  WebRtcConnection* param = Nan::ObjectWrap::Unwrap<WebRtcConnection>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::WebRtcConnection* wr = (erizo::WebRtcConnection*)param->me;

  erizo::MediaSource* ms = dynamic_cast<erizo::MediaSource*>(wr);
  me->setPublisher(ms);
}

NAN_METHOD(OneToManyProcessor::addExternalOutput) {
  OneToManyProcessor* obj = Nan::ObjectWrap::Unwrap<OneToManyProcessor>(info.Holder());
  erizo::OneToManyProcessor *me = (erizo::OneToManyProcessor*)obj->me;

  ExternalOutput* param = Nan::ObjectWrap::Unwrap<ExternalOutput>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::ExternalOutput* wr = param->me;

  erizo::MediaSink* ms = dynamic_cast<erizo::MediaSink*>(wr);

// get the param
  v8::String::Utf8Value param1(Nan::To<v8::String>(info[1]).ToLocalChecked());

// convert it to string
  std::string peerId = std::string(*param1);
  me->addSubscriber(ms, peerId);
}

NAN_METHOD(OneToManyProcessor::setExternalPublisher) {
  OneToManyProcessor* obj = Nan::ObjectWrap::Unwrap<OneToManyProcessor>(info.Holder());
  erizo::OneToManyProcessor *me = (erizo::OneToManyProcessor*)obj->me;

  ExternalInput* param = Nan::ObjectWrap::Unwrap<ExternalInput>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::ExternalInput* wr = (erizo::ExternalInput*)param->me;

  erizo::MediaSource* ms = dynamic_cast<erizo::MediaSource*>(wr);
  me->setPublisher(ms);

}

NAN_METHOD(OneToManyProcessor::getPublisherState) {
  OneToManyProcessor* obj = Nan::ObjectWrap::Unwrap<OneToManyProcessor>(info.Holder());
  erizo::OneToManyProcessor *me = (erizo::OneToManyProcessor*)obj->me;

  erizo::MediaSource * ms = me->publisher.get();

  erizo::WebRtcConnection* wr = (erizo::WebRtcConnection*)ms;

  int state = wr->getCurrentState();
  info.GetReturnValue().Set(Nan::New(state));
}

NAN_METHOD(OneToManyProcessor::hasPublisher) {
  OneToManyProcessor* obj = Nan::ObjectWrap::Unwrap<OneToManyProcessor>(info.Holder());
  erizo::OneToManyProcessor *me = (erizo::OneToManyProcessor*)obj->me;

  bool p = true;

  if(me->publisher == NULL) {
    p = false;
  }
  
  info.GetReturnValue().Set(Nan::New(p));
}

NAN_METHOD(OneToManyProcessor::addSubscriber) {
  OneToManyProcessor* obj = Nan::ObjectWrap::Unwrap<OneToManyProcessor>(info.Holder());
  erizo::OneToManyProcessor *me = (erizo::OneToManyProcessor*)obj->me;

  WebRtcConnection* param = Nan::ObjectWrap::Unwrap<WebRtcConnection>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
  erizo::WebRtcConnection* wr = (erizo::WebRtcConnection*)param->me;

  erizo::MediaSink* ms = dynamic_cast<erizo::MediaSink*>(wr);

// get the param
  v8::String::Utf8Value param1(Nan::To<v8::String>(info[1]).ToLocalChecked());

// convert it to string
  std::string peerId = std::string(*param1);
  me->addSubscriber(ms, peerId);
}

NAN_METHOD(OneToManyProcessor::removeSubscriber) {
  OneToManyProcessor* obj = Nan::ObjectWrap::Unwrap<OneToManyProcessor>(info.Holder());
  erizo::OneToManyProcessor *me = (erizo::OneToManyProcessor*)obj->me;

// get the param
  v8::String::Utf8Value param1(Nan::To<v8::String>(info[0]).ToLocalChecked());

// convert it to string
  std::string peerId = std::string(*param1);
  me->removeSubscriber(peerId);
}
