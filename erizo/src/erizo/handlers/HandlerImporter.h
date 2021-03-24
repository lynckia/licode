//
// Created by licode20 on 22/3/21.
//

#ifndef LICODE_HANDLERIMPORTER_H
#define LICODE_HANDLERIMPORTER_H

#include <string>
#include <map>
#include "../pipeline/Handler.h"
#include "./logger.h"
#include "LowerFPSHandler.h"

namespace erizo{
    class HandlerImporter {
        DECLARE_LOGGER();
    public:

        HandlerImporter();

        void loadHandlers(std::vector<std::map<std::string,std::string>> customHandlers);

        std::map<std::string, std::shared_ptr<erizo::CustomHandler>> handlersPointerDic = {};
    private:
	enum HandlersEnum {FPSReductionEnum};
	std::map<std::string, HandlersEnum> handlersDic ={{"FPSReduction" , FPSReductionEnum}};

    };

}



#endif //LICODE_HANDLERIMPORTER_H
