#include "HandlerImporter.h"
namespace erizo {

  HandlerImporter::HandlerImporter() {}

  void HandlerImporter::loadHandlers(std::vector<std::map<std::string, std::string>> custom_handlers) {
      for (unsigned int i = 0; i < custom_handlers.size(); i++) {
          std::map <std::string, std::string> parameters = custom_handlers[i];
          std::string handler_name = parameters.at("name");
          handler_order.push_back(handler_name);
          std::shared_ptr <CustomHandler> ptr;
          HandlersEnum handler_enum = handlers_dic[handler_name];

          switch (handler_enum) {
             case LoggerHandlerEnum :
                 ptr = std::make_shared<LoggerHandler>(parameters);
                 break;
             default:
                  break;
          }
          handlers_pointer_dic.insert({handler_name, ptr});
      }
  }
}  // namespace erizo
