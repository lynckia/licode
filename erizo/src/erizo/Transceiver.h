
#ifndef ERIZO_SRC_ERIZO_TRANSCEIVER_H_
#define ERIZO_SRC_ERIZO_TRANSCEIVER_H_

#include "MediaStream.h"

namespace erizo {


class Transceiver {
 public:
  explicit Transceiver(std::string id);
  virtual ~Transceiver();

  void setMediaStream(std::shared_ptr<MediaStream> stream);
  std::shared_ptr<MediaStream> getMediaStream();
  void resetMediaStream();
  bool hasMediaStream();
  std::string getId();
  void setId(std::string id);
  bool isSending();
  bool isReceiving();
  bool isInactive();

 private:
  std::string id_;
  std::shared_ptr<MediaStream> stream_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_TRANSCEIVER_H_
