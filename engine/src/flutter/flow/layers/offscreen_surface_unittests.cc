// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/flow/layers/offscreen_surface.h"

#include <memory>

#include "gtest/gtest.h"

namespace flutter::testing {

TEST(OffscreenSurfaceTest, EmptySurfaceIsInvalid) {
  auto surface = std::make_unique<OffscreenSurface>(nullptr, DlISize());
  ASSERT_FALSE(surface->IsValid());
}

TEST(OffscreenSurfaceTest, OnexOneSurfaceIsValid) {
  auto surface = std::make_unique<OffscreenSurface>(nullptr, DlISize(1, 1));
  ASSERT_TRUE(surface->IsValid());
}

TEST(OffscreenSurfaceTest, PaintSurfaceBlack) {
  auto surface = std::make_unique<OffscreenSurface>(nullptr, DlISize(1, 1));

  DlCanvas* canvas = surface->GetCanvas();
  canvas->Clear(DlColor::kBlack());
  canvas->Flush();

  auto raster_data = surface->GetRasterData(false);
  const uint32_t* actual =
      reinterpret_cast<const uint32_t*>(raster_data->data());

  // picking black as the color since byte ordering seems to matter.
  ASSERT_EQ(actual[0], 0xFF000000u);
}

TEST(OffscreenSurfaceTest, PaintSurfaceWithAlpha) {
  auto surface = std::make_unique<OffscreenSurface>(nullptr, DlISize(1, 1));

  DlCanvas* canvas = surface->GetCanvas();
  canvas->Clear(DlColor(0x80FFFFFF));
  canvas->Flush();

  auto raster_data = surface->GetRasterData(false);
  const uint32_t* actual =
      reinterpret_cast<const uint32_t*>(raster_data->data());

  // In straight (unpremultiplied) alpha, the RGB components of 50% white
  // (#80FFFFFF) should be 0xFF, not premultiplied (0x80). When
  // OffscreenSurface::GetRasterData(false) is called by Android's GetBitmap()
  // (via ScreenshotType::UncompressedImage), returning premultiplied pixels
  // (0x80808080u) causes Android Bitmap.copyPixelsFromBuffer() to generate an
  // incorrect bitmap (#80808080 instead of #80FFFFFF), reproducing issue
  // #73036.
  ASSERT_NE(actual[0], 0x80808080u);
}

}  // namespace flutter::testing
