/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_encoder.h"
#include "media/base/media_constants.h"
#include "modules/video_coding/codecs/h266/vvdec_h266_decoder.h"
#include "modules/video_coding/codecs/h266/vvenc_h266_encoder.h"
#include "test/gtest.h"

namespace webrtc {

#if defined(RTC_USE_VVENC_H266_ENCODER)
TEST(H266CodecTest, EncoderIsSupported) {
  EXPECT_EQ(VVencH266Encoder::IsSupported(), true);
}
#endif

#if defined(RTC_USE_VVDEC_H266_DECODER)
TEST(H266CodecTest, DecoderIsSupported) {
  EXPECT_EQ(VVdecH266Decoder::IsSupported(), true);
}
#endif

}  // namespace webrtc