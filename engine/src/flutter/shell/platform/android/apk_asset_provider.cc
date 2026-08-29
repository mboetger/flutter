// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/apk_asset_provider.h"

#include <unistd.h>

#include <algorithm>
#include <sstream>
#include <utility>

#include "flutter/fml/logging.h"

namespace flutter {

class APKAssetMapping : public fml::Mapping {
 public:
  explicit APKAssetMapping(AAsset* asset) : asset_(asset) {}

  ~APKAssetMapping() override { AAsset_close(asset_); }

  size_t GetSize() const override { return AAsset_getLength(asset_); }

  const uint8_t* GetMapping() const override {
    return reinterpret_cast<const uint8_t*>(AAsset_getBuffer(asset_));
  }

  bool IsDontNeedSafe() const override { return !AAsset_isAllocated(asset_); }

 private:
  AAsset* const asset_;

  FML_DISALLOW_COPY_AND_ASSIGN(APKAssetMapping);
};

class APKAssetProviderImpl : public APKAssetProviderInternal {
 public:
  explicit APKAssetProviderImpl(JNIEnv* env,
                                jobject jassetManager,
                                std::string directory)
      : java_asset_manager_(env, jassetManager),
        directory_(std::move(directory)) {
    asset_manager_ = AAssetManager_fromJava(env, jassetManager);
  }

  ~APKAssetProviderImpl() = default;

  std::unique_ptr<fml::Mapping> GetAsMapping(
      const std::string& asset_name) const override {
    std::stringstream ss;
    ss << directory_.c_str() << "/" << asset_name;
    AAsset* asset = AAssetManager_open(asset_manager_, ss.str().c_str(),
                                       AASSET_MODE_BUFFER);
    if (!asset) {
      return nullptr;
    }

    return std::make_unique<APKAssetMapping>(asset);
  };

 private:
  fml::jni::ScopedJavaGlobalRef<jobject> java_asset_manager_;
  AAssetManager* asset_manager_;
  const std::string directory_;

  FML_DISALLOW_COPY_AND_ASSIGN(APKAssetProviderImpl);
};

APKAssetProvider::APKAssetProvider(JNIEnv* env,
                                   jobject assetManager,
                                   std::string directory)
    : impl_(std::make_shared<APKAssetProviderImpl>(env,
                                                   assetManager,
                                                   std::move(directory))) {}

APKAssetProvider::APKAssetProvider(
    std::shared_ptr<APKAssetProviderInternal> impl)
    : impl_(std::move(impl)) {}

std::unique_ptr<fml::Mapping> APKAssetProvider::GetAsMapping(
    const std::string& asset_name) const {
  return impl_->GetAsMapping(asset_name);
}

std::unique_ptr<APKAssetProvider> APKAssetProvider::Clone() const {
  return std::make_unique<APKAssetProvider>(impl_);
}

FlutterAssetResolver APKAssetProvider::GetAssetResolver() const {
  FlutterAssetResolver resolver = {};
  resolver.struct_size = sizeof(FlutterAssetResolver);
  resolver.user_data = impl_.get();
  resolver.get_asset_callback = [](const char* asset_name,
                                   void* user_data) -> FlutterMapping {
    FlutterMapping mapping = {};
    mapping.struct_size = sizeof(FlutterMapping);
    if (user_data == nullptr || asset_name == nullptr) {
      return mapping;
    }
    auto* internal = static_cast<APKAssetProviderInternal*>(user_data);
    auto fml_mapping = internal->GetAsMapping(asset_name);
    if (!fml_mapping || fml_mapping->GetMapping() == nullptr) {
      return mapping;
    }
    mapping.mapping = fml_mapping->GetMapping();
    mapping.size = fml_mapping->GetSize();
    mapping.user_data = fml_mapping.release();
    mapping.release_callback = [](void* data) {
      if (data != nullptr) {
        delete static_cast<fml::Mapping*>(data);
      }
    };
    return mapping;
  };
  return resolver;
}

}  // namespace flutter
