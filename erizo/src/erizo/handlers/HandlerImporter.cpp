//
// Created by licode20 on 22/3/21.
//

#include "HandlerImporter.h"
namespace erizo {

    HandlerImporter::HandlerImporter() {}

    DEFINE_LOGGER(HandlerImporter,
    "erizo.HandlerImporter");

    void HandlerImporter::loadHandlers( std::vector<std::map<std::string,std::string>> customHandlers) {
        for (unsigned int i = 0; i < customHandlers.size(); i++) {
            ELOG_DEBUG("Handler Size %d", customHandlers.size());
            std::map <std::string, std::string> parameters = customHandlers[i];
            std::string handlerName = parameters.at("name");
            std::shared_ptr <CustomHandler> ptr;
            HandlersEnum handlerenum = handlersDic[handlerName];
            ELOG_DEBUG("Loand Handler %s", handlerName);

            switch (handlerenum) {
		case LowerFPSHandlerEnum:
			ptr = std::make_shared<LowerFPSHandler>(parameters);
			break;
                default:
                    break;
            }
            handlersPointerDic.insert({handlerName, ptr});
            ELOG_DEBUG("Handler inserted %s", handlerName);
        }
    }
}
