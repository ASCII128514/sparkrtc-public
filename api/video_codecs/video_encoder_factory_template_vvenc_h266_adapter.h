/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_TEMPLATE_VVENC_H266_ADAPTER_H_
#define API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_TEMPLATE_VVENC_H266_ADAPTER_H_

#include <memory>
#include <vector>

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory_template.h"
#include "modules/video_coding/codecs/h266/vvenc_h266_encoder.h"

namespace webrtc {

// H.266 encoder factory adapter to be used with VideoEncoderFactoryTemplate.
struct VVencH266EncoderTemplateAdapter {
  static std::vector<SdpVideoFormat> SupportedFormats() {
    if (!VVencH266Encoder::IsSupported()) {
      return {};
    }
    return {SdpVideoFormat(cricket::kH266CodecName)};
  }

  static std::unique_ptr<VideoEncoder> CreateEncoder(
      const SdpVideoFormat& format) {
    if (!VVencH266Encoder::IsSupported()) {
      return nullptr;
    }
    return std::make_unique<VVencH266Encoder>(cricket::VideoCodec(format));
  }
};

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_VIDEO_ENCODER_FACTORY_TEMPLATE_VVENC_H266_ADAPTER_H_