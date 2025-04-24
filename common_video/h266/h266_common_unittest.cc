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

#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(H266CommonTest, ParseNaluType) {
  // Test parsing NAL unit type from a byte
  // NAL unit type is in bits 3-7 of the second byte
  uint8_t data = 0x08;  // 00001000 - should be NAL type 1 (STASH_NUT)
  EXPECT_EQ(H266Common::ParseNaluType(data), H266NaluType::kH266StashNut);
  
  data = 0x28;  // 00101000 - should be NAL type 5 (IDR_N_RADL_NUT)
  EXPECT_EQ(H266Common::ParseNaluType(data), H266NaluType::kH266IdrNRadlNut);
  
  data = 0xF8;  // 11111000 - should be NAL type 31 (unspecified)
  EXPECT_EQ(H266Common::ParseNaluType(data), static_cast<H266NaluType>(31));
}

TEST(H266CommonTest, ParseNaluTypeFromData) {
  // Test parsing NAL unit type from a buffer
  // NAL unit type is in bits 3-7 of the second byte
  uint8_t data[2] = {0x00, 0x08};  // Second byte 00001000 - should be NAL type 1 (STASH_NUT)
  EXPECT_EQ(H266Common::ParseNaluType(data), H266NaluType::kH266StashNut);
  
  data[1] = 0x28;  // 00101000 - should be NAL type 5 (IDR_N_RADL_NUT)
  EXPECT_EQ(H266Common::ParseNaluType(data), H266NaluType::kH266IdrNRadlNut);
  
  data[1] = 0xF8;  // 11111000 - should be NAL type 31 (unspecified)
  EXPECT_EQ(H266Common::ParseNaluType(data), static_cast<H266NaluType>(31));
}

}  // namespace
}  // namespace webrtc