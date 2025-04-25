/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_DECODER_FACTORY_TEMPLATE_VVDEC_H266_ADAPTER_H_
#define API_VIDEO_CODECS_VIDEO_DECODER_FACTORY_TEMPLATE_VVDEC_H266_ADAPTER_H_

#include <memory>
#include <vector>

#include "api/video_codecs/sdp_video_format.h"
#include "api/video_codecs/video_decoder.h"
#include "api/video_codecs/video_decoder_factory_template.h"
#include "modules/video_coding/codecs/h266/vvdec_h266_decoder.h"

namespace webrtc {

// H.266 decoder factory adapter to be used with VideoDecoderFactoryTemplate.
struct VVdecH266DecoderTemplateAdapter {
  static std::vector<SdpVideoFormat> SupportedFormats() {
    if (!VVdecH266Decoder::IsSupported()) {
      return {};
    }
    return {SdpVideoFormat(cricket::kH266CodecName)};
  }

  static std::unique_ptr<VideoDecoder> CreateDecoder(
      const SdpVideoFormat& format) {
    if (!VVdecH266Decoder::IsSupported()) {
      return nullptr;
    }
    return std::make_unique<VVdecH266Decoder>();
  }
};

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_VIDEO_DECODER_FACTORY_TEMPLATE_VVDEC_H266_ADAPTER_H_