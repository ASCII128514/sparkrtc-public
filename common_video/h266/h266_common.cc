/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h266/h266_common.h"

namespace webrtc {

H266NaluType H266Common::ParseNaluType(uint8_t data) {
  return static_cast<H266NaluType>((data >> 3) & 0x1F);
}

H266NaluType H266Common::ParseNaluType(const uint8_t* data) {
  return ParseNaluType(data[1]);
}

}  // namespace webrtc