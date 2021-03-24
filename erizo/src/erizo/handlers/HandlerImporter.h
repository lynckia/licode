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

        std::map<std::string, std::shared_ptr<erizo::CustomHandler>>* loadHandlers();

    private:
	enum HandlersEnum {FPSReductionEnum};
	std::map<std::string, HandlersEnum> handlersDic ={{"FPSReduction" , FPSReductionEnum}};
	std::map<std::string, std::shared_ptr<erizo::CustomHandler>> handlersPointerDic = {};

	std::vector<std::map<std::string,std::string>> customHandlers;
    };

}



#endif //LICODE_HANDLERIMPORTER_H
