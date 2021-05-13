//
// Created by licode20 on 22/3/21.
//

#include "HandlerImporter.h"
namespace erizo {

    HandlerImporter::HandlerImporter() {}

    void HandlerImporter::loadHandlers(std::vector<std::map<std::string, std::string>> customHandlers) {
        for (unsigned int i = 0; i < customHandlers.size(); i++) {
            std::map <std::string, std::string> parameters = customHandlers[i];
            std::string handlerName = parameters.at("name");
            handler_order.push_back(handlerName);
            std::shared_ptr <CustomHandler> ptr;
            HandlersEnum handlerenum = handlersDic[handlerName];

            switch (handlerenum) {
               case LoggerHandlerEnum :
                   ptr = std::make_shared<LoggerHandler>(parameters);
                   break;
               default:
                    break;
            }
            handlers_pointer_dic.insert({handlerName, ptr});
        }
    }
}  // namespace erizo
