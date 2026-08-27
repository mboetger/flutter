// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_surface_vk_impeller.h"

#include <memory>
#include <utility>

#include "flutter/fml/logging.h"
#include "flutter/impeller/renderer/backend/vulkan/context_vk.h"
#include "flutter/impeller/renderer/backend/vulkan/swapchain/ahb/ahb_swapchain_vk.h"
#include "flutter/impeller/renderer/backend/vulkan/swapchain/swapchain_vk.h"
#include "flutter/impeller/toolkit/android/surface_transaction.h"

namespace flutter {

AndroidSurfaceVKImpeller::AndroidSurfaceVKImpeller(
    const std::shared_ptr<AndroidContextVKImpeller>& android_context) {
  is_valid_ = android_context->IsValid();

  auto& context_vk =
      impeller::ContextVK::Cast(*android_context->GetImpellerContext());
  surface_context_vk_ = context_vk.CreateSurfaceContext();
}

AndroidSurfaceVKImpeller::~AndroidSurfaceVKImpeller() = default;

bool AndroidSurfaceVKImpeller::IsValid() const {
  return is_valid_;
}

void AndroidSurfaceVKImpeller::TeardownOnScreenContext() {
  surface_context_vk_->TeardownSwapchain();
}

bool AndroidSurfaceVKImpeller::OnScreenSurfaceResize(const DlISize& size) {
  surface_context_vk_->UpdateSurfaceSize(
      impeller::ISize{size.width, size.height});
  return true;
}

bool AndroidSurfaceVKImpeller::ResourceContextMakeCurrent() {
  return true;
}

bool AndroidSurfaceVKImpeller::ResourceContextClearCurrent() {
  return true;
}

bool AndroidSurfaceVKImpeller::SetNativeWindow(
    fml::RefPtr<AndroidNativeWindow> window,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade) {
  if (window && (native_window_ == window)) {
    return OnScreenSurfaceResize(window->GetSize());
  }

  native_window_ = nullptr;

  if (!window || !window->IsValid()) {
    return false;
  }

  impeller::CreateTransactionCB cb = [jni_facade = jni_facade]() {
    FML_CHECK(jni_facade) << "JNI was nullptr";
    ASurfaceTransaction* tx = jni_facade->createTransaction();
    if (tx == nullptr) {
      return impeller::android::SurfaceTransaction();
    }
    return impeller::android::SurfaceTransaction(tx);
  };

  auto swapchain = impeller::SwapchainVK::Create(
      std::reinterpret_pointer_cast<impeller::Context>(
          surface_context_vk_->GetParent()),
      window->handle(), cb);

  if (!swapchain) {
    FML_LOG(ERROR) << "Failed to create Vulkan swapchain.";
    return false;
  }

  if (!surface_context_vk_->SetSwapchain(swapchain)) {
    return false;
  }
  native_window_ = window;
  return true;
}

std::shared_ptr<impeller::Context>
AndroidSurfaceVKImpeller::GetImpellerContext() {
  return std::reinterpret_pointer_cast<impeller::Context>(
      surface_context_vk_->GetParent());
}

}  // namespace flutter
