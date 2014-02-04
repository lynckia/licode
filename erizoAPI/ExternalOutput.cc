#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif
#include <node.h>
#include "ExternalOutput.h"


using namespace v8;

ExternalOutput::ExternalOutput() {};
ExternalOutput::~ExternalOutput() {};

void ExternalOutput::Init(Handle<Object> target) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(String::NewSymbol("ExternalOutput"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);
  // Prototype
  tpl->PrototypeTemplate()->Set(String::NewSymbol("close"), FunctionTemplate::New(close)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("init"), FunctionTemplate::New(init)->GetFunction());

  Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
  target->Set(String::NewSymbol("ExternalOutput"), constructor);
}

Handle<Value> ExternalOutput::New(const Arguments& args) {
  HandleScope scope;

  v8::String::Utf8Value param(args[0]->ToString());
  std::string url = std::string(*param);

  ExternalOutput* obj = new ExternalOutput();
  obj->me = new erizo::ExternalOutput(url);

  obj->Wrap(args.This());

  return args.This();
}

Handle<Value> ExternalOutput::close(const Arguments& args) {
  HandleScope scope;

  ExternalOutput* obj = ObjectWrap::Unwrap<ExternalOutput>(args.This());
  erizo::ExternalOutput *me = (erizo::ExternalOutput*)obj->me;

  delete me;

  return scope.Close(Null());
}

Handle<Value> ExternalOutput::init(const Arguments& args) {
  HandleScope scope;

  ExternalOutput* obj = ObjectWrap::Unwrap<ExternalOutput>(args.This());
  erizo::ExternalOutput *me = (erizo::ExternalOutput*) obj->me;

  int r = me->init();

  return scope.Close(Integer::New(r));
}

