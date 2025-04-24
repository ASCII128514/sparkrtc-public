/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_H266_PROFILE_TIER_LEVEL_H_
#define API_VIDEO_CODECS_H266_PROFILE_TIER_LEVEL_H_

#include <string>

#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// H.266 profiles
enum class H266Profile {
  kProfileMain,
  kProfileMain10,
  kProfileMain10Still,
  kProfileMultiLayer,
  kProfileMultiLayerMain10,
};

// H.266 tiers
enum class H266Tier {
  kTierMain,
  kTierHigh,
};

// H.266 levels
enum class H266Level {
  kLevel1,    // Level 1
  kLevel2,    // Level 2
  kLevel2_1,  // Level 2.1
  kLevel3,    // Level 3
  kLevel3_1,  // Level 3.1
  kLevel4,    // Level 4
  kLevel4_1,  // Level 4.1
  kLevel5,    // Level 5
  kLevel5_1,  // Level 5.1
  kLevel5_2,  // Level 5.2
  kLevel5_3,  // Level 5.3
  kLevel6,    // Level 6
  kLevel6_1,  // Level 6.1
  kLevel6_2,  // Level 6.2
  kLevel6_3,  // Level 6.3
};

// Helper functions for converting between profile/tier/level enums and strings
RTC_EXPORT std::string H266ProfileToString(H266Profile profile);
RTC_EXPORT H266Profile StringToH266Profile(const std::string& profile_str);

RTC_EXPORT std::string H266TierToString(H266Tier tier);
RTC_EXPORT H266Tier StringToH266Tier(const std::string& tier_str);

RTC_EXPORT std::string H266LevelToString(H266Level level);
RTC_EXPORT H266Level StringToH266Level(const std::string& level_str);

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_H266_PROFILE_TIER_LEVEL_H_