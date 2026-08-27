// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_ASSET_RESOLVER_H_
#define FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_ASSET_RESOLVER_H_

#include <memory>
#include <string>

#include "flutter/assets/asset_resolver.h"
#include "flutter/fml/macros.h"
#include "flutter/fml/mapping.h"
#include "flutter/shell/platform/embedder/embedder.h"

namespace flutter {

//------------------------------------------------------------------------------
/// @brief An AssetResolver implementation that wraps a FlutterAssetResolver
///        provided through the C Embedder API.
///
class EmbedderAssetResolver final : public AssetResolver {
 public:
  explicit EmbedderAssetResolver(FlutterAssetResolver resolver);

  ~EmbedderAssetResolver() override = default;

  // |AssetResolver|
  bool IsValid() const override;

  // |AssetResolver|
  bool IsValidAfterAssetManagerChange() const override;

  // |AssetResolver|
  AssetResolverType GetType() const override;

  // |AssetResolver|
  std::unique_ptr<fml::Mapping> GetAsMapping(
      const std::string& asset_name) const override;

  // |AssetResolver|
  bool operator==(const AssetResolver& other) const override;

  const FlutterAssetResolver& GetResolver() const { return resolver_; }

 private:
  FlutterAssetResolver resolver_;

  FML_DISALLOW_COPY_AND_ASSIGN(EmbedderAssetResolver);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_ASSET_RESOLVER_H_
