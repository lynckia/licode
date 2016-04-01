/*
 * This file contains third party code: copyright below
 */

/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <cstdio>
#include <string>

#include "RtpVP8Parser.h"

namespace erizo {

  DEFINE_LOGGER(RtpVP8Parser, "rtp.RtpVP8Parser");

  RtpVP8Parser::RtpVP8Parser() {
  }

  RtpVP8Parser::~RtpVP8Parser() {

  }

  //
  // VP8 format:
  //
  // Payload descriptor
  //       0 1 2 3 4 5 6 7
  //      +-+-+-+-+-+-+-+-+
  //      |X|R|N|S|PartID | (REQUIRED)
  //      +-+-+-+-+-+-+-+-+
  // X:   |I|L|T|K|  RSV  | (OPTIONAL)
  //      +-+-+-+-+-+-+-+-+
  // I:   |   PictureID   | (OPTIONAL)
  //      +-+-+-+-+-+-+-+-+
  // L:   |   TL0PICIDX   | (OPTIONAL)
  //      +-+-+-+-+-+-+-+-+
  // T/K: |TID:Y| KEYIDX  | (OPTIONAL)
  //      +-+-+-+-+-+-+-+-+
  //
  // Payload header (considered part of the actual payload, sent to decoder)
  //       0 1 2 3 4 5 6 7
  //      +-+-+-+-+-+-+-+-+
  //      |Size0|H| VER |P|
  //      +-+-+-+-+-+-+-+-+
  //      |      ...      |
  //      +               +
  int ParseVP8PictureID(erizo::RTPPayloadVP8* vp8, const unsigned char** dataPtr,
      int* dataLength, int* parsedBytes) {
    if (*dataLength <= 0)
      return -1;
    vp8->pictureID = (**dataPtr & 0x7F);
    if (**dataPtr & 0x80) {
      (*dataPtr)++;
      (*parsedBytes)++;
      if (--(*dataLength) <= 0)
        return -1;
      // PictureID is 15 bits
      vp8->pictureID = (vp8->pictureID << 8) + **dataPtr;
    }
    (*dataPtr)++;
    (*parsedBytes)++;
    (*dataLength)--;
    return 0;
  }

  int ParseVP8Tl0PicIdx(erizo::RTPPayloadVP8* vp8, const unsigned char** dataPtr,
      int* dataLength, int* parsedBytes) {
    if (*dataLength <= 0)
      return -1;
    vp8->tl0PicIdx = **dataPtr;
    (*dataPtr)++;
    (*parsedBytes)++;
    (*dataLength)--;
    return 0;
  }

  int ParseVP8TIDAndKeyIdx(erizo::RTPPayloadVP8* vp8,
      const unsigned char** dataPtr, int* dataLength, int* parsedBytes) {
    if (*dataLength <= 0)
      return -1;
    if (vp8->hasTID) {
      vp8->tID = ((**dataPtr >> 6) & 0x03);
      vp8->layerSync = (**dataPtr & 0x20) ? true : false; // Y bit
    }
    if (vp8->hasKeyIdx) {
      vp8->keyIdx = (**dataPtr & 0x1F);
    }
    (*dataPtr)++;
    (*parsedBytes)++;
    (*dataLength)--;
    return 0;
  }

  int ParseVP8FrameSize(erizo::RTPPayloadVP8* vp8,
      const unsigned char* dataPtr,
      int dataLength) {
    if (vp8->frameType != kIFrame) {
      // Included in payload header for I-frames.
      return -1;
    }
    if (dataLength < 10) {
      // For an I-frame we should always have the uncompressed VP8 header
      // in the beginning of the partition.
      return -1;
    }
    vp8->frameWidth = ((dataPtr[7] << 8) + dataPtr[6]) & 0x3FFF;
    vp8->frameHeight = ((dataPtr[9] << 8) + dataPtr[8]) & 0x3FFF;
    return 0;
  }

  int ParseVP8Extension(erizo::RTPPayloadVP8* vp8, const unsigned char* dataPtr, int dataLength) {

    int parsedBytes = 0;
    if (dataLength <= 0)
      return -1;
    // Optional X field is present
    vp8->hasPictureID = (*dataPtr & 0x80) ? true : false; // I bit
    vp8->hasTl0PicIdx = (*dataPtr & 0x40) ? true : false; // L bit
    vp8->hasTID = (*dataPtr & 0x20) ? true : false; // T bit
    vp8->hasKeyIdx = (*dataPtr & 0x10) ? true : false; // K bit

    //ELOG_DEBUG("Parsing extension haspic %d, hastl0 %d, has TID %d, has Key %d ",vp8->hasPictureID,vp8->hasTl0PicIdx, vp8->hasTID, vp8->hasKeyIdx  );

    // Advance dataPtr and decrease remaining payload size
    dataPtr++;
    parsedBytes++;
    dataLength--;

    if (vp8->hasPictureID) {
      if (ParseVP8PictureID(vp8, &dataPtr, &dataLength, &parsedBytes) != 0) {
        return -1;
      }
    }

    if (vp8->hasTl0PicIdx) {
      if (ParseVP8Tl0PicIdx(vp8, &dataPtr, &dataLength, &parsedBytes) != 0) {
        return -1;
      }
    }

    if (vp8->hasTID || vp8->hasKeyIdx) {
      if (ParseVP8TIDAndKeyIdx(vp8, &dataPtr, &dataLength, &parsedBytes)
          != 0) {
        return -1;
      }
    }
    return parsedBytes;
  }

  RTPPayloadVP8* RtpVP8Parser::parseVP8(unsigned char* data,
      int dataLength) {
    //ELOG_DEBUG("Parsing VP8 %d bytes", dataLength);
    RTPPayloadVP8* vp8 = new RTPPayloadVP8; // = &parsedPacket.info.VP8;
    const unsigned char* dataPtr = data;

    // Parse mandatory first byte of payload descriptor
    bool extension = (*dataPtr & 0x80) ? true : false; // X bit
    vp8->nonReferenceFrame = (*dataPtr & 0x20) ? true : false; // N bit
    vp8->beginningOfPartition = (*dataPtr & 0x10) ? true : false; // S bit
    vp8->partitionID = (*dataPtr & 0x0F); // PartID field

    //ELOG_DEBUG("X: %d N %d S %d PartID %d", extension, vp8->nonReferenceFrame, vp8->beginningOfPartition, vp8->partitionID);

    if (vp8->partitionID > 8) {
      // Weak check for corrupt data: PartID MUST NOT be larger than 8.
      return vp8;
    }

    // Advance dataPtr and decrease remaining payload size
    dataPtr++;
    dataLength--;

    if (extension) {
      const int parsedBytes = ParseVP8Extension(vp8, dataPtr, dataLength);
      if (parsedBytes < 0)
        return vp8;
      dataPtr += parsedBytes;
      dataLength -= parsedBytes;
      //ELOG_DEBUG("Parsed bytes in extension %d", parsedBytes);
    }

    if (dataLength <= 0) {
      ELOG_DEBUG("Error parsing VP8 payload descriptor; payload too short");
      return vp8;
    }

    // Read P bit from payload header (only at beginning of first partition)
    if (dataLength > 0 && vp8->beginningOfPartition && vp8->partitionID == 0) {
      //parsedPacket.frameType = (*dataPtr & 0x01) ? kPFrame : kIFrame;
      vp8->frameType = (*dataPtr & 0x01) ? kPFrame : kIFrame;
    } else {

      vp8->frameType = kPFrame;
    }
    if (0 == ParseVP8FrameSize(vp8, dataPtr, dataLength)) {
      if (vp8->frameWidth != 640){
        ELOG_DEBUG("VP8 Frame width changed! = %d need postprocessing", vp8->frameWidth);
      }
    }
    vp8->data = dataPtr;
    vp8->dataLength = (unsigned int) dataLength;

    return vp8;
  }
}

