
#ifndef ERIZO_SRC_ERIZO_TRANSCEIVER_H_
#define ERIZO_SRC_ERIZO_TRANSCEIVER_H_

#include "MediaStream.h"

namespace erizo {


class Transceiver {
 public:
  explicit Transceiver(std::string id);
  virtual ~Transceiver();

  void resetSender();
  bool hasSender();
  std::shared_ptr<MediaStream> getSender();
  void setSender(std::shared_ptr<MediaStream> stream);
  void resetReceiver();
  bool hasReceiver();
  std::shared_ptr<MediaStream> getReceiver();
  void setReceiver(std::shared_ptr<MediaStream> stream);
  std::string getId();
  void setId(std::string id);
  bool isInactive();

 private:
  std::string id_;
  std::shared_ptr<MediaStream> sender_;
  std::shared_ptr<MediaStream> receiver_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_TRANSCEIVER_H_
