// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_image_generator.h"

#include <vector>

#include "flutter/fml/mapping.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

TEST(AndroidImageGenerator, HeaderDecodeDimensionMismatch) {
  constexpr int kHeaderW = 2;
  constexpr int kHeaderH = 2;

  constexpr int kDecodedW = 64;
  constexpr int kDecodedH = 64;
  std::vector<uint8_t> zeros(kDecodedW * kDecodedH * sizeof(uint32_t), 0);
  auto decoded = std::make_unique<fml::MallocMapping>(
      fml::MallocMapping::Copy(zeros.data(), zeros.size()));

  // Create an AndroidImageGenerator where the dimensions in the header
  // ImageInfo do not match the dimensions of the decoded data.
  auto gen = AndroidImageGenerator::MakeForTesting(kHeaderW, kHeaderH,
                                                   std::move(decoded));

  FlutterImageGeneratorInfo info = {};
  ASSERT_TRUE(gen->GetInfo(&info));
  ASSERT_EQ(info.width, static_cast<uint32_t>(kHeaderW));
  ASSERT_EQ(info.height, static_cast<uint32_t>(kHeaderH));

  // AndroidImageGenerator should detect that the buffer size derived from the
  // FlutterImageGeneratorInfo is insufficient for the decoded data.
  std::vector<uint8_t> dst(kHeaderW * kHeaderH * sizeof(uint32_t), 0);
  EXPECT_FALSE(gen->GetPixels(&info, dst.data(), kHeaderW * sizeof(uint32_t)));
}

}  // namespace testing
}  // namespace flutter
