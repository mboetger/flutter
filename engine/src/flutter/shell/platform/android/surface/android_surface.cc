// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/surface/android_surface.h"
#include "flutter/fml/logging.h"

namespace flutter {

AndroidSurface::AndroidSurface() = default;

AndroidSurface::~AndroidSurface() = default;

std::unique_ptr<Surface> AndroidSurface::CreateSnapshotSurface() {
  return nullptr;
}

std::shared_ptr<impeller::Context> AndroidSurface::GetImpellerContext() {
  return nullptr;
}

void AndroidSurface::SetupImpellerSurface() {}

std::unique_ptr<GLContextResult> AndroidSurface::GLContextMakeCurrent() {
  return std::make_unique<GLContextDefaultResult>(false);
}

bool AndroidSurface::GLContextClearCurrent() {
  return false;
}

bool AndroidSurface::GLContextPresent(const GLPresentInfo& present_info) {
  return false;
}

}  // namespace flutter
