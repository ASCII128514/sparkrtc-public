/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_H266_H266_COMMON_H_
#define COMMON_VIDEO_H266_H266_COMMON_H_

#include <stdint.h>

#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// The size of a VVC NAL unit header.
const int kH266NalHeaderSize = 2;

// VVC NAL Unit Type codes (VVC spec Table 7-1)
enum H266NaluType : uint8_t {
  kH266TrailNut = 0,
  kH266StashNut = 1,
  kH266RadlNut = 2,
  kH266RaslNut = 3,
  kH266IdrWRadlNut = 4,
  kH266IdrNRadlNut = 5,
  kH266CraNut = 6,
  kH266GdrNut = 7,
  kH266PrefixApsNut = 16,
  kH266SuffixApsNut = 17,
  kH266PrefixSeiNut = 18,
  kH266SuffixSeiNut = 19,
  kH266PhNut = 20,
  kH266VpsNut = 32,
  kH266SpsNut = 33,
  kH266PpsNut = 34,
  kH266PrefixNut = 35,
  kH266SuffixNut = 36,
  kH266EosNut = 37,
  kH266EobNut = 38,
  kH266FdNut = 39,
  kH266UnspecifiedNut = 63,
};

// A class for common VVC parsing functions.
class RTC_EXPORT H266Common {
 public:
  // The size of the NAL header (2 byte).
  static const size_t kNalHeaderSize = 2;

  // Method for parsing the type from the VVC NAL header.
  // The type represents the NAL unit type.
  static H266NaluType ParseNaluType(uint8_t data);

  // Method for parsing the VVC NAL header.
  // Returns the NAL unit type.
  static H266NaluType ParseNaluType(const uint8_t* data);
};

}  // namespace webrtc

#endif  // COMMON_VIDEO_H266_H266_COMMON_H_