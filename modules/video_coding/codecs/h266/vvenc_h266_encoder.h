/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_H266_VVENC_H266_ENCODER_H_
#define MODULES_VIDEO_CODING_CODECS_H266_VVENC_H266_ENCODER_H_

#include <memory>
#include <string>
#include <vector>

#include "api/video_codecs/video_encoder.h"
#include "common_video/h266/h266_common.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/system/rtc_export.h"

// Forward declaration for VVenC library types
struct vvenc_config;
struct vvenc_encoder;

namespace webrtc {

// VVenC H.266 encoder implementation
class RTC_EXPORT VVencH266Encoder : public VideoEncoder {
 public:
  explicit VVencH266Encoder(const cricket::VideoCodec& codec);
  ~VVencH266Encoder() override;

  int32_t InitEncode(const VideoCodec* codec_settings,
                     const Settings& settings) override;
  int32_t Release() override;

  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override;
  int32_t SetRates(const RateControlParameters& parameters) override;
  int32_t Encode(const VideoFrame& input_image,
                 const std::vector<VideoFrameType>* frame_types) override;

  EncoderInfo GetEncoderInfo() const override;

  static bool IsSupported();

 private:
  // Configures the encoder with the settings provided by InitEncode
  bool ConfigureEncoder();

  // Converts from VideoCodecMode to appropriate encoder preset
  void SetEncoderPreset(vvenc_config* config, VideoCodecMode mode);

  // Callback function for VVenC encoder
  static void EncoderCallback(void* encoder,
                              void* param,
                              const uint8_t* encoded_data,
                              size_t encoded_size,
                              bool is_keyframe);

  // Encoder configuration
  VideoCodec codec_settings_;
  Settings encoder_settings_;
  cricket::VideoCodec codec_;

  // VVenC encoder state
  vvenc_encoder* encoder_;
  vvenc_config* config_;

  // Encoded image callback
  EncodedImageCallback* encoded_image_callback_;

  // Rate control state
  uint32_t target_bitrate_bps_;
  uint32_t max_bitrate_bps_;
  uint32_t framerate_fps_;

  // Frame counter for determining when to insert keyframes
  uint32_t frames_since_keyframe_;

  // Indicates if the encoder is initialized
  bool initialized_;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_H266_VVENC_H266_ENCODER_H_