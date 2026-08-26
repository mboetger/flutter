// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/apk_asset_provider.h"

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include <algorithm>
#include <cstring>
#include <utility>

#include "flutter/fml/logging.h"
#include "flutter/fml/platform/android/jni_util.h"

namespace flutter {

namespace {

class APKAssetMapping : public fml::Mapping {
 public:
  explicit APKAssetMapping(AAsset* asset) : asset_(asset) {}

  ~APKAssetMapping() override {
    if (asset_ != nullptr) {
      AAsset_close(asset_);
      asset_ = nullptr;
    }
  }

  size_t GetSize() const override {
    return asset_ == nullptr ? 0 : AAsset_getLength(asset_);
  }

  const uint8_t* GetMapping() const override {
    return asset_ == nullptr
               ? nullptr
               : reinterpret_cast<const uint8_t*>(AAsset_getBuffer(asset_));
  }

  bool IsDontNeedSafe() const override { return false; }

 private:
  AAsset* asset_;

  FML_DISALLOW_COPY_AND_ASSIGN(APKAssetMapping);
};

class APKAssetProviderImpl : public APKAssetProviderInternal {
 public:
  APKAssetProviderImpl(JNIEnv* env, jobject assetManager, std::string directory)
      : asset_manager_(AAssetManager_fromJava(env, assetManager)),
        directory_(std::move(directory)) {
    if (asset_manager_ == nullptr) {
      FML_LOG(ERROR)
          << "Could not create asset manager from java asset manager.";
      return;
    }
    if (!directory_.empty()) {
      directory_ += "/";
    }
  }

  ~APKAssetProviderImpl() override = default;

  std::unique_ptr<fml::Mapping> GetAsMapping(
      const std::string& asset_name) const override {
    if (asset_manager_ == nullptr) {
      return nullptr;
    }
    std::string file_path = directory_ + asset_name;
    AAsset* asset = AAssetManager_open(asset_manager_, file_path.c_str(),
                                       AASSET_MODE_BUFFER);
    if (asset == nullptr) {
      return nullptr;
    }
    return std::make_unique<APKAssetMapping>(asset);
  }

 private:
  AAssetManager* asset_manager_ = nullptr;
  std::string directory_;
};

}  // namespace

APKAssetProvider::APKAssetProvider(JNIEnv* env,
                                   jobject assetManager,
                                   std::string directory)
    : impl_(std::make_shared<APKAssetProviderImpl>(env,
                                                   assetManager,
                                                   std::move(directory))) {}

APKAssetProvider::APKAssetProvider(
    std::shared_ptr<APKAssetProviderInternal> impl)
    : impl_(std::move(impl)) {}

std::unique_ptr<APKAssetProvider> APKAssetProvider::Clone() const {
  return std::make_unique<APKAssetProvider>(impl_);
}

std::unique_ptr<fml::Mapping> APKAssetProvider::GetAsMapping(
    const std::string& asset_name) const {
  if (!impl_) {
    return nullptr;
  }
  return impl_->GetAsMapping(asset_name);
}

bool APKAssetProvider::GetAsset(const char* asset_name,
                                FlutterAssetMapping* mapping_out) const {
  if (asset_name == nullptr || mapping_out == nullptr || !impl_) {
    return false;
  }

  auto mapping = impl_->GetAsMapping(asset_name);
  if (!mapping) {
    return false;
  }

  static const uint8_t dummy_empty_byte = 0;
  const uint8_t* mapping_data = mapping->GetMapping();
  size_t mapping_size = mapping->GetSize();

  if (mapping_data == nullptr && mapping_size > 0) {
    return false;
  }

  mapping_out->struct_size = sizeof(FlutterAssetMapping);
  mapping_out->data = mapping_data ? mapping_data : &dummy_empty_byte;
  mapping_out->size = mapping_size;
  mapping_out->release_user_data = mapping.release();
  mapping_out->release_proc = [](void* release_user_data) {
    if (release_user_data != nullptr) {
      delete static_cast<fml::Mapping*>(release_user_data);
    }
  };

  return true;
}

// static
bool APKAssetProvider::GetAssetCallback(const char* asset_name,
                                        FlutterAssetMapping* mapping_out,
                                        void* user_data) {
  if (user_data == nullptr) {
    return false;
  }
  auto* provider = static_cast<APKAssetProvider*>(user_data);
  return provider->GetAsset(asset_name, mapping_out);
}

// static
bool APKAssetProvider::IsValidAfterAssetManagerChangeCallback(void* user_data) {
  return true;
}

FlutterAssetResolverConfig APKAssetProvider::GetAssetResolverConfig() const {
  FlutterAssetResolverConfig config = {};
  config.struct_size = sizeof(FlutterAssetResolverConfig);
  config.type = kFlutterAssetResolverTypeAPK;
  config.user_data = const_cast<APKAssetProvider*>(this);
  config.get_asset = &APKAssetProvider::GetAssetCallback;
  config.is_valid_after_asset_manager_change =
      &APKAssetProvider::IsValidAfterAssetManagerChangeCallback;
  return config;
}

}  // namespace flutter
