#ifndef ERIZO_SRC_ERIZO_RTP_RTPVP9PARSER_H_
#define ERIZO_SRC_ERIZO_RTP_RTPVP9PARSER_H_

#include <vector>

#include "./logger.h"

namespace erizo {

struct VP9ResolutionLayer {
  int width;
  int height;
};

enum VP9FrameTypes {
  kVP9IFrame,  // key frame
  kVP9PFrame   // Delta frame
};

typedef struct {
  bool hasPictureID = false;
  bool interPicturePrediction = false;
  bool hasLayerIndices = false;
  bool flexibleMode = false;
  bool beginningOfLayerFrame = false;
  bool endingOfLayerFrame = false;
  bool hasScalabilityStructure = false;
  bool largePictureID = false;
  int pictureID = -1;
  int temporalID = -1;
  bool isSwitchingUp = false;
  int spatialID = -1;
  bool isInterLayeredDepUsed = false;
  int tl0PicIdx = -1;
  int referenceIdx = -1;
  bool additionalReferenceIdx = false;

  int spatialLayers = -1;
  bool hasResolution = false;

  bool hasGof = false;

  int numberOfFramesInGof = -1;

  std::vector<VP9ResolutionLayer> resolutions;

  const unsigned char* data;
  unsigned int dataLength;
  VP9FrameTypes frameType = kVP9PFrame;
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
