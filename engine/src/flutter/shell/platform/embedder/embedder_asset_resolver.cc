// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/embedder/embedder_asset_resolver.h"

#include "flutter/fml/trace_event.h"

namespace flutter {

static AssetResolver::AssetResolverType ToInternalResolverType(
    FlutterAssetResolverType type) {
  switch (type) {
    case kFlutterAssetResolverTypeDirectory:
      return AssetResolver::AssetResolverType::kDirectoryAssetBundle;
    case kFlutterAssetResolverTypeAPK:
      return AssetResolver::AssetResolverType::kApkAssetProvider;
    case kFlutterAssetResolverTypeCustom:
      return AssetResolver::AssetResolverType::kCustomResolver;
  }
  return AssetResolver::AssetResolverType::kCustomResolver;
}

EmbedderAssetResolver::EmbedderAssetResolver(
    const FlutterAssetResolverConfig& config)
    : config_(config), type_(ToInternalResolverType(config.type)) {}

EmbedderAssetResolver::~EmbedderAssetResolver() = default;

bool EmbedderAssetResolver::IsValid() const {
  return config_.get_asset != nullptr;
}

bool EmbedderAssetResolver::IsValidAfterAssetManagerChange() const {
  if (config_.is_valid_after_asset_manager_change != nullptr) {
    return config_.is_valid_after_asset_manager_change(config_.user_data);
  }
  return true;
}

AssetResolver::AssetResolverType EmbedderAssetResolver::GetType() const {
  return type_;
}

std::unique_ptr<fml::Mapping> EmbedderAssetResolver::GetAsMapping(
    const std::string& asset_name) const {
  TRACE_EVENT1("flutter", "EmbedderAssetResolver::GetAsMapping", "name",
               asset_name.c_str());
  if (config_.get_asset == nullptr) {
    return nullptr;
  }

  FlutterAssetMapping mapping = {};
  mapping.struct_size = sizeof(FlutterAssetMapping);

  if (!config_.get_asset(asset_name.c_str(), &mapping, config_.user_data)) {
    return nullptr;
  }

  if (mapping.data == nullptr) {
    if (mapping.release_proc != nullptr) {
      mapping.release_proc(mapping.release_user_data);
    }
    return nullptr;
  }

  if (mapping.release_proc != nullptr) {
    return std::make_unique<fml::NonOwnedMapping>(
        mapping.data, mapping.size,
        [release_proc = mapping.release_proc,
         release_user_data = mapping.release_user_data](
            const uint8_t*, size_t) { release_proc(release_user_data); });
  }

  return std::make_unique<fml::NonOwnedMapping>(mapping.data, mapping.size);
}

bool EmbedderAssetResolver::operator==(const AssetResolver& other) const {
  const auto* other_embedder_resolver = other.as_embedder_asset_resolver();
  if (!other_embedder_resolver) {
    return false;
  }
  return type_ == other_embedder_resolver->type_ &&
         config_.user_data == other_embedder_resolver->config_.user_data &&
         config_.get_asset == other_embedder_resolver->config_.get_asset;
}

}  // namespace flutter
