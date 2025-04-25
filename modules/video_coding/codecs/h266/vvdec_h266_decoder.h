/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_H266_VVDEC_H266_DECODER_H_
#define MODULES_VIDEO_CODING_CODECS_H266_VVDEC_H266_DECODER_H_

#include <memory>
#include <string>

#include "api/video_codecs/video_decoder.h"
#include "common_video/h266/h266_common.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/system/rtc_export.h"

// Forward declaration for VVdeC library types
struct vvdec_config;
struct vvdec_decoder;

namespace webrtc {

// VVdeC H.266 decoder implementation
class RTC_EXPORT VVdecH266Decoder : public VideoDecoder {
 public:
  VVdecH266Decoder();
  ~VVdecH266Decoder() override;

  int32_t InitDecode(const VideoCodec* codec_settings,
                     int32_t number_of_cores) override;
  int32_t Release() override;

  int32_t RegisterDecodeCompleteCallback(
      DecodedImageCallback* callback) override;
  int32_t Decode(const EncodedImage& input_image,
                 bool missing_frames,
                 int64_t render_time_ms) override;

  const char* ImplementationName() const override;

  static bool IsSupported();

 private:
  // Configures the decoder with the settings provided by InitDecode
  bool ConfigureDecoder(int32_t number_of_cores);

  // Decoder configuration
  vvdec_config* config_;
  vvdec_decoder* decoder_;

  // Decoded image callback
  DecodedImageCallback* decoded_image_callback_;

  // Indicates if the decoder is initialized
  bool initialized_;

  // Buffer for storing partial NAL units
  std::unique_ptr<uint8_t[]> decode_buffer_;
  size_t decode_buffer_size_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_H266_VVDEC_H266_DECODER_H_