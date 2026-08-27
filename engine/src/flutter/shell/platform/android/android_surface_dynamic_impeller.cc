// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_surface_dynamic_impeller.h"

#include <memory>

namespace flutter {

AndroidSurfaceDynamicImpeller::AndroidSurfaceDynamicImpeller(
    std::shared_ptr<AndroidContextDynamicImpeller>& android_context)
    : android_context_(android_context) {}

AndroidSurfaceDynamicImpeller::~AndroidSurfaceDynamicImpeller() = default;

bool AndroidSurfaceDynamicImpeller::IsValid() const {
  return android_context_->IsValid();
}

void AndroidSurfaceDynamicImpeller::TeardownOnScreenContext() {
  if (vulkan_surface_) {
    vulkan_surface_->TeardownOnScreenContext();
  }
  if (gl_surface_) {
    gl_surface_->TeardownOnScreenContext();
  }
}

void AndroidSurfaceDynamicImpeller::SetupImpellerSurface() {
  if (android_context_->GetVKContext()) {
    vulkan_surface_ = std::make_unique<AndroidSurfaceVKImpeller>(
        android_context_->GetVKContext());
  } else if (android_context_->GetGLContext()) {
    gl_surface_ = std::make_unique<AndroidSurfaceGLImpeller>(
        android_context_->GetGLContext());
  }
}

bool AndroidSurfaceDynamicImpeller::OnScreenSurfaceResize(const DlISize& size) {
  if (vulkan_surface_) {
    return vulkan_surface_->OnScreenSurfaceResize(size);
  }
  if (gl_surface_) {
    return gl_surface_->OnScreenSurfaceResize(size);
  }
  return false;
}

bool AndroidSurfaceDynamicImpeller::ResourceContextMakeCurrent() {
  if (vulkan_surface_) {
    return vulkan_surface_->ResourceContextMakeCurrent();
  }
  if (gl_surface_) {
    return gl_surface_->ResourceContextMakeCurrent();
  }
  return false;
}

bool AndroidSurfaceDynamicImpeller::ResourceContextClearCurrent() {
  if (vulkan_surface_) {
    return vulkan_surface_->ResourceContextClearCurrent();
  }
  if (gl_surface_) {
    return gl_surface_->ResourceContextClearCurrent();
  }
  return false;
}

bool AndroidSurfaceDynamicImpeller::OnGLContextMakeCurrent() {
  if (gl_surface_) {
    return gl_surface_->OnGLContextMakeCurrent();
  }
  return false;
}

bool AndroidSurfaceDynamicImpeller::GLContextClearCurrent() {
  if (gl_surface_) {
    return gl_surface_->GLContextClearCurrent();
  }
  return false;
}

std::shared_ptr<impeller::Context>
AndroidSurfaceDynamicImpeller::GetImpellerContext() {
  if (vulkan_surface_) {
    return vulkan_surface_->GetImpellerContext();
  }
  if (gl_surface_) {
    return gl_surface_->GetImpellerContext();
  }
  return nullptr;
}

bool AndroidSurfaceDynamicImpeller::SetNativeWindow(
    fml::RefPtr<AndroidNativeWindow> window,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade) {
  if (vulkan_surface_) {
    return vulkan_surface_->SetNativeWindow(window, jni_facade);
  }
  if (gl_surface_) {
    return gl_surface_->SetNativeWindow(window, jni_facade);
  }
  return false;
}

bool AndroidSurfaceDynamicImpeller::PresentOnscreenSurface() {
  if (vulkan_surface_) {
    return vulkan_surface_->PresentOnscreenSurface();
  }
  if (gl_surface_) {
    return gl_surface_->PresentOnscreenSurface();
  }
  return false;
}

}  // namespace flutter
