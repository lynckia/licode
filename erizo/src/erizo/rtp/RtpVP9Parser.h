#ifndef ERIZO_SRC_ERIZO_RTP_RTPVP9PARSER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPVP9PARSER_H_

#include <vector>

#include "./logger.h"

namespace erizo {

struct VP9ResolutionLayer {
  int width;
  int height;
};

typedef struct {
  bool hasPictureID;
  bool interPicturePrediction;
  bool hasLayerIndices;
  bool flexibleMode;
  bool beginningOfLayerFrame;
  bool endingOfLayerFrame;
  bool hasScalabilityStructure;
  bool largePictureID;
  int pictureID;
  int temporalID;
  bool isSwitchingUp;
  int spatialID;
  bool isInterLayeredDepUsed;
  int tl0PicIdx;
  int referenceIdx;
  bool additionalReferenceIdx;

  int spatialLayers = -1;
  bool hasResolution;

  bool hasGof;

  std::vector<VP9ResolutionLayer> resolutions;

  const unsigned char* data;
  unsigned int dataLength;
} RTPPayloadVP9;

class RtpVP9Parser {
  DECLARE_LOGGER();
 public:
  RtpVP9Parser();
  virtual ~RtpVP9Parser();
  erizo::RTPPayloadVP9* parseVP9(unsigned char* data, int datalength);
};
}  // namespace erizo
#endif  // ERIZO_SRC_ERIZO_RTP_RTPVP9PARSER_H_
