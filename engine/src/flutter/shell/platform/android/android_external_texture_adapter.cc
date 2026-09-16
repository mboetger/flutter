// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_external_texture_adapter.h"

#include <utility>

#include "flutter/fml/logging.h"
#include "flutter/shell/platform/android/image_external_texture_gl_impeller.h"
#include "flutter/shell/platform/android/surface_texture_external_texture.h"
#include "flutter/shell/platform/android/surface_texture_external_texture_gl_impeller.h"
#include "flutter/shell/platform/android/surface_texture_external_texture_vk_impeller.h"
#if !SLIMPELLER
#include "flutter/shell/platform/android/android_context_gl_skia.h"
#include "flutter/shell/platform/android/image_external_texture_gl_skia.h"
#include "flutter/shell/platform/android/surface_texture_external_texture_gl_skia.h"
#endif  // !SLIMPELLER
#if IMPELLER_ENABLE_VULKAN
#include "flutter/shell/platform/android/image_external_texture_vk_impeller.h"
#endif  // IMPELLER_ENABLE_VULKAN

namespace flutter {

AndroidExternalTextureAdapter::AndroidExternalTextureAdapter(
    std::shared_ptr<AndroidContext> android_context,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    Delegate* delegate)
    : android_context_(std::move(android_context)),
      jni_facade_(std::move(jni_facade)),
      delegate_(delegate) {}

bool AndroidExternalTextureAdapter::RegisterSurfaceTexture(
    int64_t texture_id,
    const fml::jni::ScopedJavaGlobalRef<jobject>& surface_texture) {
  if (!android_context_) {
    return false;
  }

  std::shared_ptr<flutter::Texture> texture;
  switch (android_context_->RenderingApi()) {
    case AndroidRenderingAPI::kImpellerOpenGLES:
      texture = std::make_shared<SurfaceTextureExternalTextureGLImpeller>(
          std::static_pointer_cast<impeller::ContextGLES>(
              android_context_->GetImpellerContext()),
          texture_id, surface_texture, jni_facade_);
      break;
#if !SLIMPELLER
    case AndroidRenderingAPI::kSkiaOpenGLES:
      texture = std::make_shared<SurfaceTextureExternalTextureGLSkia>(
          texture_id, surface_texture, jni_facade_);
      break;
    case AndroidRenderingAPI::kSoftware:
      FML_LOG(INFO) << "Software rendering does not support external textures.";
      return false;
#endif  // !SLIMPELLER
    case AndroidRenderingAPI::kImpellerVulkan:
      FML_LOG(IMPORTANT)
          << "Flutter recommends migrating plugins that create and "
             "register surface textures to the new surface producer "
             "API. See https://docs.flutter.dev/release/breaking-changes/"
             "android-surface-plugins";
      texture = std::make_shared<SurfaceTextureExternalTextureVKImpeller>(
          std::static_pointer_cast<impeller::ContextVK>(
              android_context_->GetImpellerContext()),
          texture_id, surface_texture, jni_facade_);
      break;
    case AndroidRenderingAPI::kImpellerAutoselect:
    default:
      FML_CHECK(false);
      return false;
  }

  if (!texture) {
    return false;
  }

  textures_[texture_id] = texture;
  if (delegate_) {
    delegate_->OnRegisterTexture(std::move(texture));
  }
  return true;
}

bool AndroidExternalTextureAdapter::RegisterImageTexture(
    int64_t texture_id,
    const fml::jni::ScopedJavaGlobalRef<jobject>& image_texture_entry,
    ImageExternalTexture::ImageLifecycle lifecycle) {
  if (!android_context_) {
    return false;
  }

  std::shared_ptr<flutter::Texture> texture;
  switch (android_context_->RenderingApi()) {
#if !SLIMPELLER
    case AndroidRenderingAPI::kSkiaOpenGLES:
      texture = std::make_shared<ImageExternalTextureGLSkia>(
          std::static_pointer_cast<AndroidContextGLSkia>(android_context_),
          texture_id, image_texture_entry, jni_facade_, lifecycle);
      break;
    case AndroidRenderingAPI::kSoftware:
      FML_LOG(INFO) << "Software rendering does not support external textures.";
      return false;
#endif  // !SLIMPELLER
    case AndroidRenderingAPI::kImpellerOpenGLES:
      texture = std::make_shared<ImageExternalTextureGLImpeller>(
          std::static_pointer_cast<impeller::ContextGLES>(
              android_context_->GetImpellerContext()),
          texture_id, image_texture_entry, jni_facade_, lifecycle);
      break;
    case AndroidRenderingAPI::kImpellerVulkan:
#if IMPELLER_ENABLE_VULKAN
      texture = std::make_shared<ImageExternalTextureVKImpeller>(
          std::static_pointer_cast<impeller::ContextVK>(
              android_context_->GetImpellerContext()),
          texture_id, image_texture_entry, jni_facade_, lifecycle);
#else
      FML_CHECK(false);
#endif  // IMPELLER_ENABLE_VULKAN
      break;
    case AndroidRenderingAPI::kImpellerAutoselect:
    default:
      FML_CHECK(false);
      return false;
  }

  if (!texture) {
    return false;
  }

  textures_[texture_id] = texture;
  if (delegate_) {
    delegate_->OnRegisterTexture(std::move(texture));
  }
  return true;
}

FlutterEngineResult AndroidExternalTextureAdapter::RegisterExternalTexture(
    int64_t texture_identifier) {
  if (texture_identifier <= 0) {
    return kInvalidArguments;
  }
  if (textures_.find(texture_identifier) == textures_.end()) {
    textures_[texture_identifier] = nullptr;
  }
  return kSuccess;
}

FlutterEngineResult AndroidExternalTextureAdapter::UnregisterExternalTexture(
    int64_t texture_identifier) {
  if (texture_identifier <= 0) {
    return kInvalidArguments;
  }
  textures_.erase(texture_identifier);
  if (delegate_) {
    delegate_->OnUnregisterTexture(texture_identifier);
  }
  return kSuccess;
}

FlutterEngineResult
AndroidExternalTextureAdapter::MarkExternalTextureFrameAvailable(
    int64_t texture_identifier) {
  if (texture_identifier <= 0) {
    return kInvalidArguments;
  }
  if (delegate_) {
    delegate_->OnMarkTextureFrameAvailable(texture_identifier);
  }
  return kSuccess;
}

bool AndroidExternalTextureAdapter::HasTexture(int64_t texture_id) const {
  return textures_.find(texture_id) != textures_.end();
}

std::shared_ptr<flutter::Texture> AndroidExternalTextureAdapter::GetTexture(
    int64_t texture_id) const {
  auto it = textures_.find(texture_id);
  if (it != textures_.end()) {
    return it->second;
  }
  return nullptr;
}

size_t AndroidExternalTextureAdapter::TextureCount() const {
  return textures_.size();
}

}  // namespace flutter
