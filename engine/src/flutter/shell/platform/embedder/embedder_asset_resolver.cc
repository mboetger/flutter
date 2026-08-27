// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/embedder/embedder_asset_resolver.h"

namespace flutter {

EmbedderAssetResolver::EmbedderAssetResolver(
    const FlutterAssetResolver* resolver) {
  if (resolver != nullptr &&
      resolver->struct_size == sizeof(FlutterAssetResolver) &&
      resolver->get_asset != nullptr) {
    resolver_ = *resolver;
    is_valid_ = true;
  }
}

EmbedderAssetResolver::~EmbedderAssetResolver() = default;

bool EmbedderAssetResolver::IsValid() const {
  return is_valid_;
}

bool EmbedderAssetResolver::IsValidAfterAssetManagerChange() const {
  return true;
}

AssetResolver::AssetResolverType EmbedderAssetResolver::GetType() const {
  return AssetResolver::AssetResolverType::kCustomResolver;
}

std::unique_ptr<fml::Mapping> EmbedderAssetResolver::GetAsMapping(
    const std::string& asset_name) const {
  if (!is_valid_) {
    return nullptr;
  }

  FlutterMapping mapping = {};
  mapping.struct_size = sizeof(FlutterMapping);

  if (!resolver_.get_asset(asset_name.c_str(), &mapping, resolver_.user_data)) {
    return nullptr;
  }

  if (mapping.struct_size != sizeof(FlutterMapping)) {
    if (mapping.release_callback != nullptr) {
      mapping.release_callback(mapping.mapping, mapping.size,
                               mapping.user_data);
    }
    return nullptr;
  }

  if (mapping.mapping == nullptr && mapping.size > 0) {
    if (mapping.release_callback != nullptr) {
      mapping.release_callback(mapping.mapping, mapping.size,
                               mapping.user_data);
    }
    return nullptr;
  }

  auto release_callback = mapping.release_callback;
  void* user_data = mapping.user_data;

  return std::make_unique<fml::NonOwnedMapping>(
      mapping.mapping, mapping.size,
      [release_callback, user_data](const uint8_t* data, size_t size) {
        if (release_callback != nullptr) {
          release_callback(data, size, user_data);
        }
      });
}

bool EmbedderAssetResolver::operator==(const AssetResolver& other) const {
  if (other.GetType() != GetType()) {
    return false;
  }
  const auto* other_resolver =
      static_cast<const EmbedderAssetResolver*>(&other);
  return is_valid_ == other_resolver->is_valid_ &&
         resolver_.user_data == other_resolver->resolver_.user_data &&
         resolver_.get_asset == other_resolver->resolver_.get_asset;
}

}  // namespace flutter
