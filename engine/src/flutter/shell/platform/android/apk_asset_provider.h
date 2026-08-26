// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_ANDROID_APK_ASSET_PROVIDER_H_
#define FLUTTER_SHELL_PLATFORM_ANDROID_APK_ASSET_PROVIDER_H_

#include <android/asset_manager_jni.h>
#include <jni.h>
#include <memory>
#include <string>

#include "flutter/fml/macros.h"
#include "flutter/fml/mapping.h"
#include "flutter/fml/memory/ref_counted.h"
#include "flutter/fml/platform/android/scoped_java_ref.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

class APKAssetProviderInternal {
 public:
  virtual std::unique_ptr<fml::Mapping> GetAsMapping(
      const std::string& asset_name) const = 0;

 protected:
  virtual ~APKAssetProviderInternal() = default;
};

class APKAssetProvider final {
 public:
  explicit APKAssetProvider(JNIEnv* env,
                            jobject assetManager,
                            std::string directory);

  explicit APKAssetProvider(std::shared_ptr<APKAssetProviderInternal> impl);

  ~APKAssetProvider() = default;

  // Returns a new 'std::unique_ptr<APKAssetProvider>' with the same 'impl_' as
  // this provider.
  std::unique_ptr<APKAssetProvider> Clone() const;

  // Obtain a raw pointer to the APKAssetProviderInternal.
  //
  // This method is intended for use in tests. Callers must not
  // delete the returned pointer.
  APKAssetProviderInternal* GetImpl() const { return impl_.get(); }

  // Produces a 'FlutterAssetResolverConfig' suitable for passing to the public
  // Embedder API (such as 'FlutterProjectArgs' or
  // 'FlutterEngineUpdateAssetResolver'). The caller must ensure that this
  // APKAssetProvider instance outlives the active usage of the returned
  // configuration.
  FlutterAssetResolverConfig GetAssetResolverConfig() const;

  // Resolves an asset mapping into a 'FlutterAssetMapping'. Returns true if
  // found and populates 'mapping_out'.
  bool GetAsset(const char* asset_name, FlutterAssetMapping* mapping_out) const;

  static bool GetAssetCallback(const char* asset_name,
                               FlutterAssetMapping* mapping_out,
                               void* user_data);

  static bool IsValidAfterAssetManagerChangeCallback(void* user_data);

  std::unique_ptr<fml::Mapping> GetAsMapping(
      const std::string& asset_name) const;

  bool operator==(const APKAssetProvider& other) const {
    return impl_ == other.impl_;
  }

 private:
  std::shared_ptr<APKAssetProviderInternal> impl_;

  FML_DISALLOW_COPY_AND_ASSIGN(APKAssetProvider);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_ANDROID_APK_ASSET_PROVIDER_H_
