/**
* VideoUtils.cpp
*/
#include "media/mixers/VideoUtils.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cstring>

//
// MIN macro
//
#define MIN(a, b) (a < b) ? a:b
#define MAX_WIDTH  4096
#define MAX_HEIGHT 4096

using std::memcpy;

DEFINE_LOGGER(VideoUtils, "erizo.media.mixers.VideoUtils");

inline void vRescaleP(unsigned char *inBuff,
                      unsigned int   inBuffLen,
                      unsigned char *outBuff,
                      unsigned int   outBuffLen,
                      unsigned int   inW,
                      unsigned int   inH,
                      unsigned int   outW,
                      unsigned int   outH,
                      unsigned int   BPP) {
  if (!outW || !outH) {
    return;
  }

  float actualZoomX = static_cast<float>(outW) / static_cast<float>(inW);
  float actualZoomY = static_cast<float>(outH) / static_cast<float>(inH);

  unsigned zoomedWidth = static_cast<int>(inW * actualZoomX);
  unsigned zoomedHeight = static_cast<int>(inH * actualZoomY);

  unsigned croppedZoomedWidth = MIN(zoomedWidth, outW);
  unsigned croppedZoomedHeight = MIN(zoomedHeight, outH);

  //
  // let start by zooming image
  //
  unsigned int xindex[MAX_WIDTH];
  unsigned int yindex[MAX_HEIGHT];

  unsigned curry = 0;
  float    deltaX = 1 / actualZoomX;
  float    deltaY = 1 / actualZoomY;

  for (unsigned iter = 0; iter < croppedZoomedHeight; iter++) {
    yindex[iter]= static_cast<int>(static_cast<float>(iter) * deltaY);
  }

  for (unsigned iter = 0; iter < croppedZoomedWidth; iter++) {
    xindex[iter]= BPP * static_cast<int>(static_cast<float>(iter) * deltaX);
  }

  for (unsigned iy = 0; iy < croppedZoomedHeight; iy++) {
    while (curry < yindex[iy]) {
      inBuff += inW * BPP;
      curry++;
    }
    for (unsigned ix = 0; ix < croppedZoomedWidth; ix++) {
      for (unsigned j = 0; j < BPP; j++) {
        *(outBuff + j) = *(inBuff + *(xindex + ix) + j);
      }
      outBuff += BPP;
    }
    for (unsigned ix = croppedZoomedWidth; ix < outW; ix++) {
      *outBuff = 0xff/2;
      outBuff++;
    }
  }
  return;
}

int VideoUtils::vRescale(unsigned char *inBuff,
                         unsigned int   inBuffLen,
                         unsigned char *outBuff,
                         unsigned int   outBuffLen,
                         unsigned int   inW,
                         unsigned int   inH,
                         unsigned int   outW,
                         unsigned int   outH,
                         uint32_t     format) {
  switch (static_cast<ImgFormat>(format)) {
    case I420P_FORMAT:
      if (outBuffLen < outW*outH * 3 / 2) {
        ELOG_DEBUG("vRescale:: needed %d, outBuffLen = %d", outW * outH * 3 / 2, outBuffLen);
        return -1;
      }

      // rescale luminance
      vRescaleP(inBuff,
                inW * inH,
                outBuff,
                outW * outH,
                inW,
                inH,
                outW,
                outH,
                1);  // Bytes Per Pixel

      // rescale chroma U
      vRescaleP(inBuff + inW * inH,
                inW * inH * 4,
                outBuff + outW * outH,
                outW*outH*4,
                inW / 2,
                inH / 2,
                outW / 2,
                outH / 2,
                1);  // Bytes Per Pixel

      // rescale chroma V
      vRescaleP(inBuff + inW * inH * 5 / 4,
                inW * inH * 4,
                outBuff + outW * outH * 5 / 4,
                outW * outH * 4,
                inW / 2,
                inH / 2,
                outW / 2,
                outH / 2,
                1);  // Bytes Per Pixel

      return outW * outH * 3 / 2;

    case RGB24_FORMAT:
    case BGR24_FORMAT:
      if (outBuffLen < outW * outH * 3) {
        ELOG_DEBUG("vRescale:: needed %d, outBuffLen = %d", outW * outH * 3, outBuffLen);
        return -1;
      }

      // rescale rgb plane
      vRescaleP(inBuff,
                inW * inH,
                outBuff,
                outW * outH,
                inW,
                inH,
                outW,
                outH,
                3);  // Bytes Per Pixel

      return outW * outH * 3;

    default:
      ELOG_WARN("vRescale: not implemented for %d", format);
      abort();
  }
  return -1;
}

inline void vPutImageP(unsigned char *inBuff,
                       unsigned int   inBuffLen,
                       unsigned char *outBuff,
                       unsigned int   W,
                       unsigned int   H,
                       unsigned int   X,
                       unsigned int   Y,
                      unsigned int   totalW,
                       unsigned int   totalH,
                       unsigned int   BPP,
                       unsigned char *mask,
                       bool           invert) {
  unsigned lineSize1 = W * BPP;
  unsigned lineSize2 = totalW * BPP;
  unsigned initRectPos1 = 0;
  unsigned initRectPos2 = lineSize2 * Y + X * BPP;
  unsigned position1 = 0;
  unsigned position2 = 0;

  if (mask) {
    for (unsigned i = 0; i < H; i++) {
      position1 = initRectPos1 + lineSize1 * i;  // save image1 position
      position2 = initRectPos2 + lineSize2 * i;  // save image2 position
      for (unsigned j = 0; j < lineSize1; j++) {
        if ((static_cast<bool>(mask[position2 + j])) ^ invert) {
          outBuff[position2 + j] = inBuff[position1 + j];  // copy pixel
        }
      }
    }
  } else {
    for (unsigned i = 0; i < H; i++) {
      position1 = initRectPos1 + lineSize1 * i;  // save image1 position
      position2 = initRectPos2 + lineSize2 * i;  // save image2 position
      memcpy(&outBuff[position2], &inBuff[position1], lineSize1);  // copy line
    }
  }
}

int VideoUtils::vPutImage(unsigned char *inBuff,
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
                          unsigned char *mask,
                          bool           invert) {
  if ((outW > totalW) || (outH > totalH)) {
    ELOG_DEBUG("vPutImage : output resolution greater than total image resolution!");
    return -1;
  }

  if (posX + outW > totalW) outW = totalW - posX;
  if (posY + outH > totalH) outH = totalH - posY;

  double factor = 0;
  int BPP = 0;
  switch (format) {
    case I420P_FORMAT:
      factor = 1.5;
      BPP = 1;
      break;
    case RGB24_FORMAT:
    case BGR24_FORMAT:
      factor = 3;
      BPP = 3;
      break;
    default:
      ELOG_DEBUG("vPutImage : unknown format %d", format);
      abort();
  }

  if (outBuffLen < outW * outH * BPP * factor) {
    ELOG_DEBUG("vPutImage :: needed %f, outBuffLen = %d", totalW * totalH * BPP * factor, outBuffLen);
    return -1;
  }

  unsigned char * image = inBuff;

  int len = inBuffLen;
  if ((inW != outW) || (inH != outH)) {
    len   = static_cast<int>(outW * outH * factor);
    image = new unsigned char[len];
    int ret = vRescale(inBuff,
                       inBuffLen,
                       image,
                       len,
                       inW,
                       inH,
                       outW,
                       outH,
                       format);

    if (ret <= 0) {
      ELOG_DEBUG("vPutImage : vRescale failed");
      delete [] image;
      return -1;
    }
  }

  switch (format) {
    case I420P_FORMAT:
      // put luminance plane
      vPutImageP(image,
        outW * outH * BPP,
        outBuff,
        outW,
        outH,
        posX,
        posY,
        totalW,
        totalH,
        BPP,
        mask,
        invert);

      // put chroma U plane
      vPutImageP(image + outW * outH * BPP,
        outW*outH*BPP/4,
        outBuff + totalW * totalH * BPP,
        outW / 2,
        outH / 2,
        posX / 2,
        posY / 2,
        totalW / 2,
        totalH / 2,
        BPP,
        mask ? mask + totalW * totalH * BPP : NULL,
        invert);

      // put chroma V plane
      vPutImageP(image + outW * outH * BPP * 5 / 4,
        outW * outH * BPP / 4,
        outBuff + totalW * totalH * BPP * 5 / 4,
        outW / 2,
        outH / 2,
        posX / 2,
        posY / 2,
        totalW / 2,
        totalH / 2,
        BPP,
        mask ? mask + totalW * totalH * BPP * 5 / 4 : NULL,
        invert);
      break;

    case RGB24_FORMAT:
    case BGR24_FORMAT:
      // put bgr plane
      vPutImageP(image,
        outW * outH * BPP,
        outBuff,
        outW,
        outH,
        posX,
        posY,
        totalW,
        totalH,
        BPP,
        mask,
        invert);
      break;

    default:
      ELOG_DEBUG("vPutImage : unknown format");
      abort();
  }

  if (image != inBuff) {
    delete[] image;
  }

  return static_cast<int>(totalW * totalH * BPP * factor);
}

inline void vSetMaskRectP(unsigned char *mask,
                          unsigned int   W,
                          unsigned int   H,
                          unsigned int   posX,
                          unsigned int   posY,
                          unsigned int   totalW,
                          unsigned int   totalH,
                          bool           val,
                          int            BPP) {
  unsigned lineSize1 = W*BPP;
  unsigned lineSize2 = totalW * BPP;
  // unsigned initRectPos1 = 0;
  unsigned initRectPos2 = lineSize2 * posY + posX * BPP;
  // unsigned position1 = 0;
  unsigned position2 = 0;
  for (unsigned i = 0; i < H; i++) {
    // position1 = initRectPos1 + lineSize1*i; // save image1 position
    position2 = initRectPos2 + lineSize2 * i;  // save image2 position
    for (unsigned j = 0; j < lineSize1; j += BPP) {
      for (int k = 0; k < BPP; k++) {
        mask[position2 + j + k]= val;
      }
    }
  }
}

void VideoUtils::vSetMaskRect(unsigned char *mask,
                              unsigned int   W,
                              unsigned int   H,
                              unsigned int   posX,
                              unsigned int   posY,
                              unsigned int   totalW,
                              unsigned int   totalH,
                              bool           val,
                              uint32_t       format) {
  int BPP;

  switch (format) {
    case I420P_FORMAT:
      BPP = 1;
      vSetMaskRectP(mask,
        W,
        H,
        posX,
        posY,
        totalW,
        totalH,
        val,
        BPP);
      vSetMaskRectP(mask + totalW * totalH,
        W / 2,
        H / 2,
        posX / 2,
        posY / 2,
        totalW / 2,
        totalH / 2,
        val,
        BPP);
      vSetMaskRectP(mask + totalW * totalH * 5 / 4,
        W / 2,
        H / 2,
        posX / 2,
        posY / 2,
        totalW / 2,
        totalH / 2,
        val,
        BPP);
      break;

    case RGB24_FORMAT:
    case BGR24_FORMAT:
      BPP = 3;
      vSetMaskRectP(mask,
        W,
        H,
        posX,
        posY,
        totalW,
        totalH,
        val,
        BPP);
      break;

    default:
      ELOG_DEBUG("vSetMaskRect : unknown format");
      abort();
  }
}

int VideoUtils::vSetMask(unsigned char *outBuff,
                         unsigned int   outBuffLen,
                         unsigned char *mask,
                         unsigned int   W,
                         unsigned int   H,
                         unsigned int   totalW,
                         unsigned int   totalH,
                         bool           val,
                         uint32_t       format) {
  double factor = 0;
  int BPP = 0;

  switch (format) {
    case I420P_FORMAT:
      factor = 1.5;
      BPP = 1;
      break;
    case RGB24_FORMAT:
    case BGR24_FORMAT:
      factor = 3;
      BPP = 3;
      break;
    default:
      ELOG_DEBUG("vPutImage : unknown format");
      abort();
  }

  if (outBuffLen < totalW * totalH * BPP * factor) {
    ELOG_DEBUG("vSetMask :: needed %f, outBuffLen = %d", totalW * totalH * BPP * factor, outBuffLen);
    return -1;
  }

  unsigned char * image  = NULL;
  unsigned char * image2 = NULL;

  int len = static_cast<int>(totalW * totalH * factor);
  image  = new unsigned char[len];
  image2 = new unsigned char[len/4];

  // luminance plane
  vRescaleP(mask,
    W * H,
    image,
    len,
    W,
    H,
    totalW,
    totalH,
    BPP);
  // U and V planes
  vRescaleP(mask,
    W * H,
    image2,
    len / 4,
    W,
    H,
    totalW / 2,
    totalH / 2,
    BPP);

  switch (format) {
    case I420P_FORMAT:
      // put luminance plane
      vPutImageP(image,
        totalW * totalH * BPP,
        outBuff,
        totalW,
        totalH,
        0,
        0,
        totalW,
        totalH,
        BPP,
        NULL,
        false);

      // put chroma U plane
      vPutImageP(image2,
        totalW * totalH * BPP / 4,
        outBuff + totalW * totalH * BPP,
        totalW / 2,
        totalH / 2,
        0,
        0,
        totalW / 2,
        totalH / 2,
        BPP,
        NULL,
        false);

      // put chroma V plane
      vPutImageP(image2,
        totalW * totalH * BPP / 4,
        outBuff + totalW * totalH * BPP * 5 / 4,
        totalW / 2,
        totalH / 2,
        0,
        0,
        totalW / 2,
        totalH / 2,
        BPP,
        NULL,
        false);

      break;

    case RGB24_FORMAT:
    case BGR24_FORMAT:
      // put bgr plane
      vPutImageP(image,
        totalW * totalH * BPP,
        outBuff,
        totalW,
        totalH,
        0,
        0,
        totalW,
        totalH,
        BPP,
        NULL,
        false);

      break;

    default:
      ELOG_DEBUG("vSetMask : unknown format");
      abort();
  }

  delete[] image;
  delete[] image2;

  return 0;
}
