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
    const FlutterAssetResolver& resolver)
    : resolver_(resolver), type_(ToInternalResolverType(resolver.type)) {}

EmbedderAssetResolver::~EmbedderAssetResolver() = default;

bool EmbedderAssetResolver::IsValid() const {
  return resolver_.get_asset_callback != nullptr;
}

bool EmbedderAssetResolver::IsValidAfterAssetManagerChange() const {
  if (resolver_.is_valid_after_asset_manager_change != nullptr) {
    return resolver_.is_valid_after_asset_manager_change(resolver_.user_data);
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
  if (resolver_.get_asset_callback == nullptr) {
    return nullptr;
  }

  FlutterMapping mapping = {};
  mapping.struct_size = sizeof(FlutterMapping);

  if (!resolver_.get_asset_callback(asset_name.c_str(), &mapping,
                                    resolver_.user_data)) {
    return nullptr;
  }

  if (mapping.mapping == nullptr) {
    if (mapping.release_callback != nullptr) {
      mapping.release_callback(mapping.user_data);
    }
    return nullptr;
  }

  if (mapping.release_callback != nullptr) {
    return std::make_unique<fml::NonOwnedMapping>(
        mapping.mapping, mapping.size,
        [release_callback = mapping.release_callback,
         user_data = mapping.user_data](const uint8_t*, size_t) {
          release_callback(user_data);
        });
  }

  return std::make_unique<fml::NonOwnedMapping>(mapping.mapping, mapping.size);
}

bool EmbedderAssetResolver::operator==(const AssetResolver& other) const {
  const auto* other_embedder_resolver = other.as_embedder_asset_resolver();
  if (!other_embedder_resolver) {
    return false;
  }
  return type_ == other_embedder_resolver->type_ &&
         resolver_.user_data == other_embedder_resolver->resolver_.user_data &&
         resolver_.get_asset_callback ==
             other_embedder_resolver->resolver_.get_asset_callback;
}

}  // namespace flutter
