#ifndef ONETOMANYPROCESSOR_H
#define ONETOMANYPROCESSOR_H

#include <node.h>
#include <OneToManyProcessor.h>
#include "MediaReceiver.h"
#include "WebRtcConnection.h"

class OneToManyProcessor : public MediaReceiver {
 public:
  static void Init(v8::Handle<v8::Object> target);

 private:
  OneToManyProcessor();
  ~OneToManyProcessor();

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> setPublisher(const v8::Arguments& args);
  static v8::Handle<v8::Value> hasPublisher(const v8::Arguments& args);
  static v8::Handle<v8::Value> addSubscriber(const v8::Arguments& args);
  static v8::Handle<v8::Value> removeSubscriber(const v8::Arguments& args);

};

#endif
