// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/embedder/embedder_asset_resolver.h"

namespace flutter {

EmbedderAssetResolver::EmbedderAssetResolver(FlutterAssetResolver resolver)
    : resolver_(resolver) {}

bool EmbedderAssetResolver::IsValid() const {
  return resolver_.struct_size == sizeof(FlutterAssetResolver) &&
         resolver_.get_asset_callback != nullptr;
}

bool EmbedderAssetResolver::IsValidAfterAssetManagerChange() const {
  return true;
}

AssetResolver::AssetResolverType EmbedderAssetResolver::GetType() const {
  return AssetResolverType::kEmbedderAssetResolver;
}

std::unique_ptr<fml::Mapping> EmbedderAssetResolver::GetAsMapping(
    const std::string& asset_name) const {
  if (!IsValid() || asset_name.empty()) {
    return nullptr;
  }

  FlutterMapping mapping =
      resolver_.get_asset_callback(asset_name.c_str(), resolver_.user_data);

  if (mapping.struct_size != sizeof(FlutterMapping)) {
    FML_LOG(ERROR) << "Custom asset resolver returned FlutterMapping with "
                      "invalid struct_size: "
                   << mapping.struct_size;
    return nullptr;
  }

  if (mapping.mapping == nullptr) {
    if (mapping.release_callback != nullptr) {
      mapping.release_callback(mapping.user_data);
    }
    return nullptr;
  }

  return std::make_unique<fml::NonOwnedMapping>(
      mapping.mapping, mapping.size,
      [user_data = mapping.user_data,
       release_callback = mapping.release_callback](const uint8_t*, size_t) {
        if (release_callback != nullptr) {
          release_callback(user_data);
        }
      });
}

bool EmbedderAssetResolver::operator==(const AssetResolver& other) const {
  if (other.GetType() != GetType()) {
    return false;
  }
  const auto* other_resolver =
      static_cast<const EmbedderAssetResolver*>(&other);
  return resolver_.get_asset_callback ==
             other_resolver->resolver_.get_asset_callback &&
         resolver_.user_data == other_resolver->resolver_.user_data;
}

}  // namespace flutter
