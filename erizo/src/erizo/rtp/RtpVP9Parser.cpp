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
#include "rtp/RtpVP9Parser.h"

#include <arpa/inet.h>

#include <cstddef>
#include <cstdio>
#include <string>
#include <iostream>

namespace erizo {

DEFINE_LOGGER(RtpVP9Parser, "rtp.RtpVP9Parser");

RtpVP9Parser::RtpVP9Parser() {
}

RtpVP9Parser::~RtpVP9Parser() {
}

//
// VP9 format:
//
// Payload descriptor (Flexible mode F = 1)
//        0 1 2 3 4 5 6 7
//       +-+-+-+-+-+-+-+-+
//       |I|P|L|F|B|E|V|-| (REQUIRED)
//       +-+-+-+-+-+-+-+-+
//  I:   |M| PICTURE ID  | (REQUIRED)
//       +-+-+-+-+-+-+-+-+
//  M:   | EXTENDED PID  | (RECOMMENDED)
//       +-+-+-+-+-+-+-+-+
//  L:   |  T  |U|  S  |D| (CONDITIONALLY RECOMMENDED)
//       +-+-+-+-+-+-+-+-+                             -
//  P,F: | P_DIFF      |N| (CONDITIONALLY REQUIRED)    - up to 3 times
//       +-+-+-+-+-+-+-+-+                             -
//  V:   | SS            |
//       | ..            |
//       +-+-+-+-+-+-+-+-+
//
// Payload descriptor (Non flexible mode F = 0)
//
//       0 1 2 3 4 5 6 7
//      +-+-+-+-+-+-+-+-+
//      |I|P|L|F|B|E|V|-| (REQUIRED)
//      +-+-+-+-+-+-+-+-+
// I:   |M| PICTURE ID  | (RECOMMENDED)
//      +-+-+-+-+-+-+-+-+
// M:   | EXTENDED PID  | (RECOMMENDED)
//      +-+-+-+-+-+-+-+-+
// L:   |  T  |U|  S  |D| (CONDITIONALLY RECOMMENDED)
//      +-+-+-+-+-+-+-+-+
//      |   TL0PICIDX   | (CONDITIONALLY REQUIRED)
//      +-+-+-+-+-+-+-+-+
// V:   | SS            |
//      | ..            |
//      +-+-+-+-+-+-+-+-+

RTPPayloadVP9* RtpVP9Parser::parseVP9(unsigned char* data, int dataLength) {
  // ELOG_DEBUG("Parsing VP9 %d bytes", dataLength);
  RTPPayloadVP9* vp9 = new RTPPayloadVP9;  // = &parsedPacket.info.VP9;
  const unsigned char* dataPtr = data;
  int len = dataLength;

  // Parse mandatory first byte of payload descriptor
  vp9->hasPictureID = (*dataPtr & 0x80) ? true : false;  // I bit
  vp9->interPicturePrediction = (*dataPtr & 0x40) ? true : false;  // P bit
  vp9->frameType = vp9->interPicturePrediction ? kVP9PFrame : kVP9IFrame;
  vp9->hasLayerIndices = (*dataPtr & 0x20) ? true : false;  // L bit
  vp9->flexibleMode = (*dataPtr & 0x10) ? true : false;  // F bit
  vp9->beginningOfLayerFrame = (*dataPtr & 0x08) ? true : false;  // B bit
  vp9->endingOfLayerFrame = (*dataPtr & 0x04) ? true : false;  // E bit
  vp9->hasScalabilityStructure = (*dataPtr & 0x02) ? true : false;  // V bit
  dataPtr++;
  len--;

  if (vp9->hasPictureID) {
    vp9->largePictureID = (*dataPtr & 0x80) ? true : false;  // M bit
    vp9->pictureID = (*dataPtr & 0x7F);
    if (vp9->largePictureID) {
      dataPtr++;
      len--;
      vp9->pictureID = ntohs((vp9->pictureID << 16) + (*dataPtr & 0xFF));
    }
    dataPtr++;
    len--;
  }
  vp9->temporalID = 0;
  vp9->spatialID = 0;

  if (vp9->hasLayerIndices) {
    vp9->temporalID = (*dataPtr & 0xE0) >> 5;  // T bits
    vp9->isSwitchingUp = (*dataPtr & 0x10) ? true : false;  // U bit
    vp9->spatialID = (*dataPtr & 0x0E) >> 1;  // S bits
    vp9->isInterLayeredDepUsed = (*dataPtr & 0x01) ? true : false;  // D bit
    if (vp9->flexibleMode) {
      do {
        dataPtr++;
        len--;
        vp9->referenceIdx = (*dataPtr & 0xFE) >> 1;
        vp9->additionalReferenceIdx = (*dataPtr & 0x01) ? true : false;  // D bit
      } while (vp9->additionalReferenceIdx);
    } else {
      dataPtr++;
      len--;
      vp9->tl0PicIdx = (*dataPtr & 0xFF);
    }
    dataPtr++;
    len--;
  }

  if (vp9->hasScalabilityStructure) {
    vp9->spatialLayers = (*dataPtr & 0xE0) >> 5;  // N_S bits
    vp9->hasResolution = (*dataPtr & 0x10) ? true : false;  // Y bit
    vp9->hasGof = (*dataPtr & 0x08) ? true : false;  // Y bit
    dataPtr++;
    len--;
    if (vp9->hasResolution) {
      for (int i = 0; i <= vp9->spatialLayers; i++) {
        int width = *dataPtr & 0xFF;
        dataPtr++;
        len--;
        width = (width << 8) + (*dataPtr & 0xFF);
        dataPtr++;
        len--;
        int height = *dataPtr & 0xFF;
        dataPtr++;
        len--;
        height = (height << 8) + (*dataPtr & 0xFF);
        dataPtr++;
        len--;
        vp9->resolutions.push_back({width, height});
      }
    }
    if (vp9->hasGof) {
      vp9->numberOfFramesInGof = *dataPtr & 0xFF;  // N_G bits
      dataPtr++;
      len--;
      for (int frame_index = 0; frame_index < vp9->numberOfFramesInGof; frame_index++) {
        // TODO(javierc): Read these values if needed
        int reference_indices = (*dataPtr & 0x0C) >> 2;  // R bits
        dataPtr++;
        len--;
        for (int reference_index = 0; reference_index < reference_indices; reference_index++) {
          dataPtr++;
          len--;
        }
      }
    }
  }

  vp9->data = dataPtr;
  vp9->dataLength = (unsigned int) len;

  return vp9;
}

}  // namespace erizo
