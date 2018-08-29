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
#include "rtp/RtpVP8Parser.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>

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
int ParseVP8PictureID(erizo::RTPPayloadVP8* vp8, const unsigned char** dataPtr, int* dataLength, int* parsedBytes) {
  if (*dataLength <= 0) {
    return -1;
  }
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

int ParseVP8Tl0PicIdx(erizo::RTPPayloadVP8* vp8, const unsigned char** dataPtr, int* dataLength, int* parsedBytes) {
  if (*dataLength <= 0) {
    return -1;
  }
  vp8->tl0PicIdx = **dataPtr;
  (*dataPtr)++;
  (*parsedBytes)++;
  (*dataLength)--;
  return 0;
}

int ParseVP8TIDAndKeyIdx(erizo::RTPPayloadVP8* vp8,
    const unsigned char** dataPtr, int* dataLength, int* parsedBytes) {
  if (*dataLength <= 0) {
    return -1;
  }
  if (vp8->hasTID) {
    vp8->tID = ((**dataPtr >> 6) & 0x03);
    vp8->layerSync = (**dataPtr & 0x20) ? true : false;  // Y bit
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
  if (vp8->frameType != kVP8IFrame) {
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
  if (dataLength <= 0) {
    return -1;
  }
  // Optional X field is present
  vp8->hasPictureID = (*dataPtr & 0x80) ? true : false;  // I bit
  vp8->hasTl0PicIdx = (*dataPtr & 0x40) ? true : false;  // L bit
  vp8->hasTID = (*dataPtr & 0x20) ? true : false;  // T bit
  vp8->hasKeyIdx = (*dataPtr & 0x10) ? true : false;  // K bit

  // ELOG_DEBUG("Parsing extension haspic %d, hastl0 %d, has TID %d, has Key %d ",
  //             vp8->hasPictureID, vp8->hasTl0PicIdx, vp8->hasTID, vp8->hasKeyIdx  );

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
    if (ParseVP8TIDAndKeyIdx(vp8, &dataPtr, &dataLength, &parsedBytes) != 0) {
      return -1;
    }
  }
  return parsedBytes;
}

void RtpVP8Parser::setVP8PictureID(unsigned char* data, int data_length, int picture_id) {
  unsigned char* data_ptr = data;

  bool extension = (*data_ptr & 0x80) ? true : false;  // X bit

  data_ptr++;
  data_length--;

  if (extension) {
    if (data_length <= 0) {
      return;
    }
    bool has_picture_id = (*data_ptr & 0x80) ? true : false;  // I bit
    data_ptr++;
    data_length--;

    if (has_picture_id) {
      if (data_length <= 0) {
        return;
      }
      const uint16_t pic_id = static_cast<uint16_t> (picture_id);
      int picture_id_len = (*data_ptr & 0x80) ? 2 : 1;
      if (picture_id_len > data_length) return;
      if (picture_id_len == 2) {
        data_ptr[0] = 0x80 | ((pic_id >> 8) & 0x7F);
        data_ptr[1] = pic_id & 0xFF;
      } else if (picture_id_len == 1) {
        data_ptr[0] = pic_id & 0x7F;
      }
    }
  }
}

void RtpVP8Parser::setVP8TL0PicIdx(unsigned char* data, int data_length, uint8_t tl0_pic_idx) {
  unsigned char* data_ptr = data;

  bool extension = (*data_ptr & 0x80) ? true : false;  // X bit

  data_ptr++;
  data_length--;

  if (extension) {
    if (data_length <= 0) {
      return;
    }
    bool has_picture_id = (*data_ptr & 0x80) ? true : false;  // I bit
    bool has_tl0_pic_idx = (*data_ptr & 0x40) ? true : false;  // L bit
    data_ptr++;
    data_length--;

    if (has_picture_id) {
      if (data_length <= 0) {
        return;
      }
      int picture_id_len = (*data_ptr & 0x80) ? 2 : 1;
      data_ptr += picture_id_len;
      data_length -= picture_id_len;
    }

    if (has_tl0_pic_idx) {
      if (data_length <= 0) {
        return;
      }
      data_ptr[0] = tl0_pic_idx;
    }
  }
}

int RtpVP8Parser::removePictureID(unsigned char* data, int data_length) {
  unsigned char* data_ptr = data;
  int previous_data_length = data_length;

  bool extension = (*data_ptr & 0x80) ? true : false;  // X bit

  data_ptr++;
  data_length--;

  if (extension) {
    if (data_length <= 0) {
      return previous_data_length;
    }
    bool has_picture_id = (*data_ptr & 0x80) ? true : false;  // I bit
    data_ptr[0] = *data_ptr & 0x7F;  // unset I bit
    data_ptr++;
    data_length--;

    if (has_picture_id) {
      if (data_length <= 0) {
        return previous_data_length;
      }
      int picture_id_len = (*data_ptr & 0x80) ? 2 : 1;
      if (picture_id_len > data_length) return previous_data_length;
      memmove(data_ptr, data_ptr + picture_id_len, data_length - picture_id_len);
      previous_data_length -= picture_id_len;
    }
  }

  return previous_data_length;
}

int RtpVP8Parser::removeTl0PicIdx(unsigned char* data, int data_length) {
  unsigned char* data_ptr = data;
  int previous_data_length = data_length;

  bool extension = (*data_ptr & 0x80) ? true : false;  // X bit

  data_ptr++;
  data_length--;

  if (extension) {
    if (data_length <= 0) {
      return previous_data_length;
    }
    bool has_picture_id = (*data_ptr & 0x80) ? true : false;  // I bit
    bool has_tl0_pic_idx = (*data_ptr & 0x40) ? true : false;  // L bit
    data_ptr[0] = *data_ptr & 0xBF;  // unset L bit
    data_ptr++;
    data_length--;

    if (has_picture_id) {
      if (data_length <= 0) {
        return previous_data_length;
      }
      int picture_id_len = (*data_ptr & 0x80) ? 2 : 1;
      data_ptr += picture_id_len;
      data_length -= picture_id_len;
    }

    if (has_tl0_pic_idx) {
      if (data_length <= 0) {
        return previous_data_length;
      }
      memmove(data_ptr, data_ptr + 1, data_length - 1);
      previous_data_length--;
    }
  }
  return previous_data_length;
}

int RtpVP8Parser::removeTIDAndKeyIdx(unsigned char* data, int data_length) {
  unsigned char* data_ptr = data;
  int previous_data_length = data_length;

  bool extension = (*data_ptr & 0x80) ? true : false;  // X bit

  data_ptr++;
  data_length--;

  if (extension) {
    if (data_length <= 0) {
      return previous_data_length;
    }
    bool has_picture_id = (*data_ptr & 0x80) ? true : false;  // I bit
    bool has_tl0_pic_idx = (*data_ptr & 0x40) ? true : false;  // L bit
    bool has_tid = (*data_ptr & 0x20) ? true : false;  // T bit
    bool has_key_idx = (*data_ptr & 0x10) ? true : false;  // K bit
    data_ptr[0] = *data_ptr & 0xDF;  // unset T bit
    data_ptr[0] = *data_ptr & 0xEF;  // unset T bit
    data_ptr++;
    data_length--;

    if (has_picture_id) {
      if (data_length <= 0) {
        return previous_data_length;
      }
      int picture_id_len = (*data_ptr & 0x80) ? 2 : 1;
      data_ptr += picture_id_len;
      data_length -= picture_id_len;
    }

    if (has_tl0_pic_idx) {
      if (data_length <= 0) {
        return previous_data_length;
      }
      data_ptr++;
      data_length--;
    }

    if (has_tid || has_key_idx) {
      if (data_length <= 0) {
        return previous_data_length;
      }
      memmove(data_ptr, data_ptr + 1, data_length - 1);
      previous_data_length--;
    }
  }
  return previous_data_length;
}

RTPPayloadVP8* RtpVP8Parser::parseVP8(unsigned char* data, int dataLength) {
  // ELOG_DEBUG("Parsing VP8 %d bytes", dataLength);
  RTPPayloadVP8* vp8 = new RTPPayloadVP8;  // = &parsedPacket.info.VP8;
  const unsigned char* dataPtr = data;

  // Parse mandatory first byte of payload descriptor
  bool extension = (*dataPtr & 0x80) ? true : false;  // X bit
  vp8->nonReferenceFrame = (*dataPtr & 0x20) ? true : false;  // N bit
  vp8->beginningOfPartition = (*dataPtr & 0x10) ? true : false;  // S bit
  vp8->partitionID = (*dataPtr & 0x0F);  // PartID field

  // ELOG_DEBUG("X: %d N %d S %d PartID %d",
  //             extension, vp8->nonReferenceFrame, vp8->beginningOfPartition, vp8->partitionID);

  if (vp8->partitionID > 8) {
    // Weak check for corrupt data: PartID MUST NOT be larger than 8.
    return vp8;
  }

  // Advance dataPtr and decrease remaining payload size
  dataPtr++;
  dataLength--;

  if (extension) {
    const int parsedBytes = ParseVP8Extension(vp8, dataPtr, dataLength);
    if (parsedBytes < 0) {
      return vp8;
    }
    dataPtr += parsedBytes;
    dataLength -= parsedBytes;
    // ELOG_DEBUG("Parsed bytes in extension %d", parsedBytes);
  }

  if (dataLength <= 0) {
    ELOG_WARN("Error parsing VP8 payload descriptor; payload too short");
    return vp8;
  }

  // Read P bit from payload header (only at beginning of first partition)
  if (dataLength > 0 && vp8->beginningOfPartition && vp8->partitionID == 0) {
    // parsedPacket.frameType = (*dataPtr & 0x01) ? kVP8PFrame : kVP8IFrame;
    vp8->frameType = (*dataPtr & 0x01) ? kVP8PFrame : kVP8IFrame;
  } else {
    vp8->frameType = kVP8PFrame;
  }
  if (0 == ParseVP8FrameSize(vp8, dataPtr, dataLength)) {
    if (vp8->frameWidth != 640) {
      ELOG_WARN("VP8 Frame width changed! = %d need postprocessing", vp8->frameWidth);
    }
  }
  vp8->data = dataPtr;
  vp8->dataLength = (unsigned int) dataLength;

  return vp8;
}
}  // namespace erizo
