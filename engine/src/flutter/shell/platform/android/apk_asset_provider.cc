// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/apk_asset_provider.h"

#include <unistd.h>

#include <algorithm>
#include <sstream>
#include <utility>

#include "flutter/assets/asset_resolver.h"
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

// |AssetResolver|
bool APKAssetProvider::IsValid() const {
  return true;
}

// |AssetResolver|
bool APKAssetProvider::IsValidAfterAssetManagerChange() const {
  return true;
}

// |AssetResolver|
AssetResolver::AssetResolverType APKAssetProvider::GetType() const {
  return AssetResolver::AssetResolverType::kApkAssetProvider;
}

// |AssetResolver|
std::unique_ptr<fml::Mapping> APKAssetProvider::GetAsMapping(
    const std::string& asset_name) const {
  return impl_->GetAsMapping(asset_name);
}

std::unique_ptr<APKAssetProvider> APKAssetProvider::Clone() const {
  return std::make_unique<APKAssetProvider>(impl_);
}

bool APKAssetProvider::GetAsset(const char* asset_name,
                                FlutterAssetMapping* mapping_out) const {
  if (!asset_name || !mapping_out || !impl_) {
    return false;
  }
  auto mapping = impl_->GetAsMapping(asset_name);
  if (!mapping) {
    return false;
  }

  static const uint8_t kEmptyBuffer = 0;
  const uint8_t* data = mapping->GetMapping();
  if (data == nullptr) {
    if (mapping->GetSize() == 0) {
      data = &kEmptyBuffer;
    } else {
      return false;
    }
  }

  mapping_out->struct_size = sizeof(FlutterAssetMapping);
  mapping_out->data = data;
  mapping_out->size = mapping->GetSize();
  auto* raw_mapping = mapping.release();
  mapping_out->release_proc = [](void* release_user_data) {
    delete static_cast<fml::Mapping*>(release_user_data);
  };
  mapping_out->release_user_data = raw_mapping;
  return true;
}

bool APKAssetProvider::GetAssetCallback(const char* asset_name,
                                        FlutterAssetMapping* mapping_out,
                                        void* user_data) {
  if (!user_data || !asset_name || !mapping_out) {
    return false;
  }
  return static_cast<APKAssetProvider*>(user_data)->GetAsset(asset_name,
                                                             mapping_out);
}

bool APKAssetProvider::IsValidAfterAssetManagerChangeCallback(void* user_data) {
  if (!user_data) {
    return true;
  }
  return static_cast<APKAssetProvider*>(user_data)
      ->IsValidAfterAssetManagerChange();
}

FlutterAssetResolverConfig APKAssetProvider::GetAssetResolverConfig() const {
  FlutterAssetResolverConfig config = {};
  config.struct_size = sizeof(FlutterAssetResolverConfig);
  config.user_data = const_cast<APKAssetProvider*>(this);
  config.type = kFlutterAssetResolverTypeAPK;
  config.get_asset = &APKAssetProvider::GetAssetCallback;
  config.is_valid_after_asset_manager_change =
      &APKAssetProvider::IsValidAfterAssetManagerChangeCallback;
  return config;
}

bool APKAssetProvider::operator==(const AssetResolver& other) const {
  auto other_provider = other.as_apk_asset_provider();
  if (!other_provider) {
    return false;
  }
  return impl_ == other_provider->impl_;
}

}  // namespace flutter
