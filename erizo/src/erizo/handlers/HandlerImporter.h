//
// Created by licode20 on 22/3/21.
//

#ifndef ERIZO_SRC_ERIZO_HANDLERS_HANDLERIMPORTER_H_
#define ERIZO_SRC_ERIZO_HANDLERS_HANDLERIMPORTER_H_

#include <string>
#include <map>
#include "../pipeline/Handler.h"
#include "./logger.h"
#include "handlers/LoggerHandler.h"



namespace erizo {


class HandlerImporterInterface {
    DECLARE_LOGGER();
 public:
    void loadHandlers(std::vector<std::map<std::string, std::string>> custom_handlers);
    std::map<std::string, std::shared_ptr<erizo::CustomHandler>> handlers_pointer_dic = {};
    std::vector<std::string> handler_order = {};
};


class HandlerImporter: public HandlerImporterInterface {
    DECLARE_LOGGER();
 public:
    HandlerImporter();
    void loadHandlers(std::vector<std::map<std::string, std::string>> custom_handlers);
 private:
    enum HandlersEnum {LoggerHandlerEnum};

    std::map<std::string, HandlersEnum> handlers_dic ={{"LoggerHandler", LoggerHandlerEnum}};
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_HANDLERS_HANDLERIMPORTER_H_
