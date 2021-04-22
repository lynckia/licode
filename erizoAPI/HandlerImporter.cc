#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif

#include "HandlerImporter.h"
#include <logger.h>


using v8::Local;
using v8::Value;
using v8::Function;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Exception;
using v8::Array;
using json = nlohmann::json;

DEFINE_LOGGER(HandlerImporter, "ErizoAPI.HandlerImporter");

HandlerImporter::HandlerImporter() {
}

HandlerImporter::~HandlerImporter() {
}

Nan::Persistent<Function> HandlerImporter::constructor;

NAN_MODULE_INIT(HandlerImporter::Init) {
        // Prepare constructor template
        Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
        tpl->SetClassName(Nan::New("HandlerImporter").ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        // Prototype
        // Nan::SetPrototypeMethod(tpl, "close", close);
        // Nan::SetPrototypeMethod(tpl, "start", start);

        constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
        Nan::Set(target, Nan::New("HandlerImporter").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

NAN_METHOD(HandlerImporter::New) {
        if (info.Length() < 1) {
            Nan::ThrowError("Wrong number of arguments");
        }
        std::vector<std::map<std::string, std::string>> customHandlers = {};
        Local<Array> jsArr = Local<Array>::Cast(info[0]);
        for (unsigned int i = 0; i < jsArr->Length(); i++) {
            Nan::Utf8String jsElement(Nan::To<v8::String>(Nan::Get(jsArr, i).ToLocalChecked()).ToLocalChecked());
            std::string parameters = std::string(*jsElement);
            nlohmann::json handlersJson = nlohmann::json::parse(parameters);
            std::map<std::string, std::string> paramsDic = {};
            for (json::iterator it = handlersJson.begin(); it != handlersJson.end(); ++it) {
                paramsDic.insert((std::pair<std::string, std::string>(it.key(), it.value())));
            }
            customHandlers.push_back(paramsDic);
        }
        HandlerImporter* obj = new HandlerImporter();
        obj->me = std::make_shared<erizo::HandlerImporter>();
        obj->me->loadHandlers(customHandlers);
        obj->Wrap(info.This());
        info.GetReturnValue().Set(info.This());
}
