/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/h266_profile_tier_level.h"

#include <map>
#include <utility>

#include "rtc_base/checks.h"

namespace webrtc {

namespace {

// Maps for converting between enum values and strings
const std::map<H266Profile, std::string> kProfileToString = {
    {H266Profile::kProfileMain, "Main"},
    {H266Profile::kProfileMain10, "Main10"},
    {H266Profile::kProfileMain10Still, "Main10Still"},
    {H266Profile::kProfileMultiLayer, "MultiLayer"},
    {H266Profile::kProfileMultiLayerMain10, "MultiLayerMain10"},
};

const std::map<H266Tier, std::string> kTierToString = {
    {H266Tier::kTierMain, "Main"},
    {H266Tier::kTierHigh, "High"},
};

const std::map<H266Level, std::string> kLevelToString = {
    {H266Level::kLevel1, "1"},
    {H266Level::kLevel2, "2"},
    {H266Level::kLevel2_1, "2.1"},
    {H266Level::kLevel3, "3"},
    {H266Level::kLevel3_1, "3.1"},
    {H266Level::kLevel4, "4"},
    {H266Level::kLevel4_1, "4.1"},
    {H266Level::kLevel5, "5"},
    {H266Level::kLevel5_1, "5.1"},
    {H266Level::kLevel5_2, "5.2"},
    {H266Level::kLevel5_3, "5.3"},
    {H266Level::kLevel6, "6"},
    {H266Level::kLevel6_1, "6.1"},
    {H266Level::kLevel6_2, "6.2"},
    {H266Level::kLevel6_3, "6.3"},
};

// Helper function to find key by value in a map
template <typename K, typename V>
K FindKeyByValue(const std::map<K, V>& map, const V& value) {
  for (const auto& pair : map) {
    if (pair.second == value) {
      return pair.first;
    }
  }
  RTC_DCHECK_NOTREACHED();
  return K();
}

}  // namespace

std::string H266ProfileToString(H266Profile profile) {
  auto it = kProfileToString.find(profile);
  if (it != kProfileToString.end()) {
    return it->second;
  }
  RTC_DCHECK_NOTREACHED();
  return "Unknown";
}

H266Profile StringToH266Profile(const std::string& profile_str) {
  return FindKeyByValue(kProfileToString, profile_str);
}

std::string H266TierToString(H266Tier tier) {
  auto it = kTierToString.find(tier);
  if (it != kTierToString.end()) {
    return it->second;
  }
  RTC_DCHECK_NOTREACHED();
  return "Unknown";
}

H266Tier StringToH266Tier(const std::string& tier_str) {
  return FindKeyByValue(kTierToString, tier_str);
}

std::string H266LevelToString(H266Level level) {
  auto it = kLevelToString.find(level);
  if (it != kLevelToString.end()) {
    return it->second;
  }
  RTC_DCHECK_NOTREACHED();
  return "Unknown";
}

H266Level StringToH266Level(const std::string& level_str) {
  return FindKeyByValue(kLevelToString, level_str);
}

}  // namespace webrtc