// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_EXTERNAL_TEXTURE_ADAPTER_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_EXTERNAL_TEXTURE_ADAPTER_H_

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "flutter/common/graphics/texture.h"
#include "flutter/fml/platform/android/scoped_java_ref.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/image_external_texture.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

//------------------------------------------------------------------------------
/// An adapter that sits in front of SurfaceTextureExternalTexture and
/// ImageExternalTexture, providing both the legacy Android JNI registration
/// paths and entrypoints shaped like the Embedder API's
/// FlutterEngineRegisterExternalTexture.
class AndroidExternalTextureAdapter {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnRegisterTexture(
        std::shared_ptr<flutter::Texture> texture) = 0;
    virtual void OnUnregisterTexture(int64_t texture_id) = 0;
    virtual void OnMarkTextureFrameAvailable(int64_t texture_id) = 0;
  };

  AndroidExternalTextureAdapter(
      std::shared_ptr<AndroidContext> android_context,
      std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
      Delegate* delegate);

  ~AndroidExternalTextureAdapter() = default;

  AndroidExternalTextureAdapter(const AndroidExternalTextureAdapter&) = delete;
  AndroidExternalTextureAdapter& operator=(
      const AndroidExternalTextureAdapter&) = delete;

  // Legacy Android JNI registration paths
  bool RegisterSurfaceTexture(
      int64_t texture_id,
      const fml::jni::ScopedJavaGlobalRef<jobject>& surface_texture);

  bool RegisterImageTexture(
      int64_t texture_id,
      const fml::jni::ScopedJavaGlobalRef<jobject>& image_texture_entry,
      ImageExternalTexture::ImageLifecycle lifecycle);

  // Embedder API shaped methods
  FlutterEngineResult RegisterExternalTexture(int64_t texture_identifier);
  FlutterEngineResult UnregisterExternalTexture(int64_t texture_identifier);
  FlutterEngineResult MarkExternalTextureFrameAvailable(
      int64_t texture_identifier);

  // Texture inspection / querying
  bool HasTexture(int64_t texture_id) const;
  std::shared_ptr<flutter::Texture> GetTexture(int64_t texture_id) const;
  size_t TextureCount() const;

 private:
  std::shared_ptr<AndroidContext> android_context_;
  std::shared_ptr<PlatformViewAndroidJNI> jni_facade_;
  Delegate* delegate_ = nullptr;
  std::unordered_map<int64_t, std::shared_ptr<flutter::Texture>> textures_;
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_ANDROID_EXTERNAL_TEXTURE_ADAPTER_H_
