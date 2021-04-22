#ifndef ERIZOAPI_HANDLERIMPORTER_H_
#define ERIZOAPI_HANDLERIMPORTER_H_

#include <nan.h>
#include "WebRtcConnection.h"
#include "lib/json.hpp"
#include "../erizo/src/erizo/handlers/HandlerImporter.h"

class HandlerImporter : public Nan::ObjectWrap {
 public:
    DECLARE_LOGGER();
    static NAN_MODULE_INIT(Init);
    std::shared_ptr<erizo::HandlerImporter> me;

 private:
    HandlerImporter();
    ~HandlerImporter();
    /*
     * Constructor.
     * Constructs a IOThreadPool
     */
    static NAN_METHOD(New);

    static Nan::Persistent<v8::Function> constructor;
};

#endif  // ERIZOAPI_HANDLERIMPORTER_H_
