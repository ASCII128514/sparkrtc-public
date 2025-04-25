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

#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(H266ProfileTierLevelTest, ProfileToString) {
  EXPECT_EQ(H266ProfileToString(H266Profile::kProfileMain), "Main");
  EXPECT_EQ(H266ProfileToString(H266Profile::kProfileMain10), "Main10");
  EXPECT_EQ(H266ProfileToString(H266Profile::kProfileMain10Still), "Main10Still");
  EXPECT_EQ(H266ProfileToString(H266Profile::kProfileMultiLayer), "MultiLayer");
  EXPECT_EQ(H266ProfileToString(H266Profile::kProfileMultiLayerMain10), "MultiLayerMain10");
}

TEST(H266ProfileTierLevelTest, StringToProfile) {
  EXPECT_EQ(StringToH266Profile("Main"), H266Profile::kProfileMain);
  EXPECT_EQ(StringToH266Profile("Main10"), H266Profile::kProfileMain10);
  EXPECT_EQ(StringToH266Profile("Main10Still"), H266Profile::kProfileMain10Still);
  EXPECT_EQ(StringToH266Profile("MultiLayer"), H266Profile::kProfileMultiLayer);
  EXPECT_EQ(StringToH266Profile("MultiLayerMain10"), H266Profile::kProfileMultiLayerMain10);
}

TEST(H266ProfileTierLevelTest, TierToString) {
  EXPECT_EQ(H266TierToString(H266Tier::kTierMain), "Main");
  EXPECT_EQ(H266TierToString(H266Tier::kTierHigh), "High");
}

TEST(H266ProfileTierLevelTest, StringToTier) {
  EXPECT_EQ(StringToH266Tier("Main"), H266Tier::kTierMain);
  EXPECT_EQ(StringToH266Tier("High"), H266Tier::kTierHigh);
}

TEST(H266ProfileTierLevelTest, LevelToString) {
  EXPECT_EQ(H266LevelToString(H266Level::kLevel1), "1");
  EXPECT_EQ(H266LevelToString(H266Level::kLevel2), "2");
  EXPECT_EQ(H266LevelToString(H266Level::kLevel2_1), "2.1");
  EXPECT_EQ(H266LevelToString(H266Level::kLevel3), "3");
  EXPECT_EQ(H266LevelToString(H266Level::kLevel3_1), "3.1");
  EXPECT_EQ(H266LevelToString(H266Level::kLevel4), "4");
  EXPECT_EQ(H266LevelToString(H266Level::kLevel4_1), "4.1");
  EXPECT_EQ(H266LevelToString(H266Level::kLevel5), "5");
  EXPECT_EQ(H266LevelToString(H266Level::kLevel5_1), "5.1");
  EXPECT_EQ(H266LevelToString(H266Level::kLevel5_2), "5.2");
  EXPECT_EQ(H266LevelToString(H266Level::kLevel5_3), "5.3");
  EXPECT_EQ(H266LevelToString(H266Level::kLevel6), "6");
  EXPECT_EQ(H266LevelToString(H266Level::kLevel6_1), "6.1");
  EXPECT_EQ(H266LevelToString(H266Level::kLevel6_2), "6.2");
  EXPECT_EQ(H266LevelToString(H266Level::kLevel6_3), "6.3");
}

TEST(H266ProfileTierLevelTest, StringToLevel) {
  EXPECT_EQ(StringToH266Level("1"), H266Level::kLevel1);
  EXPECT_EQ(StringToH266Level("2"), H266Level::kLevel2);
  EXPECT_EQ(StringToH266Level("2.1"), H266Level::kLevel2_1);
  EXPECT_EQ(StringToH266Level("3"), H266Level::kLevel3);
  EXPECT_EQ(StringToH266Level("3.1"), H266Level::kLevel3_1);
  EXPECT_EQ(StringToH266Level("4"), H266Level::kLevel4);
  EXPECT_EQ(StringToH266Level("4.1"), H266Level::kLevel4_1);
  EXPECT_EQ(StringToH266Level("5"), H266Level::kLevel5);
  EXPECT_EQ(StringToH266Level("5.1"), H266Level::kLevel5_1);
  EXPECT_EQ(StringToH266Level("5.2"), H266Level::kLevel5_2);
  EXPECT_EQ(StringToH266Level("5.3"), H266Level::kLevel5_3);
  EXPECT_EQ(StringToH266Level("6"), H266Level::kLevel6);
  EXPECT_EQ(StringToH266Level("6.1"), H266Level::kLevel6_1);
  EXPECT_EQ(StringToH266Level("6.2"), H266Level::kLevel6_2);
  EXPECT_EQ(StringToH266Level("6.3"), H266Level::kLevel6_3);
}

}  // namespace
}  // namespace webrtc