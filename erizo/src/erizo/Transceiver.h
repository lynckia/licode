
#ifndef ERIZO_SRC_ERIZO_TRANSCEIVER_H_
#define ERIZO_SRC_ERIZO_TRANSCEIVER_H_

#include "MediaStream.h"

namespace erizo {


class Transceiver {
 public:
  explicit Transceiver(std::string id, std::string kind);
  virtual ~Transceiver();

  void resetSender();
  bool hasSender();
  bool hadSenderBefore();
  std::shared_ptr<MediaStream> getSender();
  void setSender(std::shared_ptr<MediaStream> stream);
  void resetReceiver();
  bool hasReceiver();
  std::shared_ptr<MediaStream> getReceiver();
  void setReceiver(std::shared_ptr<MediaStream> stream);
  std::string getId();
  void setId(std::string id);
  bool isInactive();
  void setAsAddedToSdp();
  bool hasBeenAddedToSdp();
  std::string getSsrc();
  std::string getKind();
  std::string getSenderLabel();
  std::string getReceiverLabel();

 private:
  std::string id_;
  std::string sender_label_;
  std::string receiver_label_;
  std::string ssrc_;
  std::string kind_;
  bool added_to_sdp_;
  std::shared_ptr<MediaStream> sender_;
  std::shared_ptr<MediaStream> receiver_;
};
}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_TRANSCEIVER_H_
