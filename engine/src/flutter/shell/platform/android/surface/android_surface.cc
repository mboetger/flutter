// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/surface/android_surface.h"
#include "flutter/fml/logging.h"

namespace flutter {

AndroidSurface::AndroidSurface() = default;

AndroidSurface::~AndroidSurface() = default;

std::unique_ptr<Surface> AndroidSurface::CreateGPUSurface() {
  return CreateGPUSurface(nullptr);
}

std::unique_ptr<Surface> AndroidSurface::CreateSnapshotSurface() {
  return nullptr;
}

std::shared_ptr<impeller::Context> AndroidSurface::GetImpellerContext() {
  return nullptr;
}

std::shared_ptr<impeller::Context> AndroidSurface::CreateImpellerContext()
    const {
  return const_cast<AndroidSurface*>(this)->GetImpellerContext();
}

void AndroidSurface::SetupImpellerSurface() {}

void AndroidSurface::SetupImpellerContext() {
  SetupImpellerSurface();
}

}  // namespace flutter
