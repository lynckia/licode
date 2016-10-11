/**
 * VideoUtils.h
 */
#ifndef ERIZO_SRC_ERIZO_MEDIA_MIXERS_VIDEOUTILS_H_
#define ERIZO_SRC_ERIZO_MEDIA_MIXERS_VIDEOUTILS_H_

#include <boost/cstdint.hpp>

#include "./logger.h"

class VideoUtils{
  DECLARE_LOGGER();

  enum ImgFormat{
    I420P_FORMAT,
    RGB24_FORMAT,
    BGR24_FORMAT
  };

  static int vRescale(unsigned char *inBuff,
        unsigned int  inBuffLen,
        unsigned char *outBuff,
        unsigned int   outBuffLen,
        unsigned int   inW,
        unsigned int   inH,
        unsigned int   outW,
        unsigned int   outH,
        uint32_t       format);

  static int vPutImage(unsigned char *inBuff,
        unsigned int   inBuffLen,
        unsigned char *outBuff,
        unsigned int   outBuffLen,
        unsigned int   inW,
        unsigned int   inH,
        unsigned int   outW,
        unsigned int   outH,
        unsigned int   posX,
        unsigned int   posY,
        unsigned int   totalW,
        unsigned int   totalH,
        uint32_t            format,
        unsigned char *mask = NULL,
        bool           invertMask = false);

  static void vSetMaskRect(unsigned char *mask,
        unsigned int   W,
        unsigned int   H,
        unsigned int   posX,
        unsigned int   posY,
        unsigned int   totalW,
        unsigned int   totalH,
        bool           val,
        uint32_t       format);

  static int vSetMask(unsigned char *outBuff,
        unsigned       outBuffLen,
        unsigned char *mask,
        unsigned       W,
        unsigned       H,
        unsigned       totalW,
        unsigned       totalH,
        bool           val,
        uint32_t       format);
};
#endif  // ERIZO_SRC_ERIZO_MEDIA_MIXERS_VIDEOUTILS_H_
