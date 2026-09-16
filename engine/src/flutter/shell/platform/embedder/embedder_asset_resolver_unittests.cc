// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/embedder/embedder_asset_resolver.h"

#include <cstring>
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

TEST(EmbedderAssetResolverTest, BasicMappingAndReleaseCallback) {
  bool release_called = false;
  static constexpr char kData[] = "hello asset world";
  static constexpr size_t kDataSize = sizeof(kData) - 1;

  auto get_asset = [](const char* asset_name, FlutterMapping* mapping,
                      void* user_data) -> bool {
    auto* release_flag = reinterpret_cast<bool*>(user_data);
    if (std::strcmp(asset_name, "test.txt") != 0) {
      return false;
    }
    mapping->struct_size = sizeof(FlutterMapping);
    mapping->mapping = reinterpret_cast<const uint8_t*>(kData);
    mapping->size = kDataSize;
    mapping->release_callback = [](void* release_data) {
      *reinterpret_cast<bool*>(release_data) = true;
    };
    mapping->user_data = release_flag;
    return true;
  };

  FlutterAssetResolver resolver_config = {};
  resolver_config.struct_size = sizeof(FlutterAssetResolver);
  resolver_config.user_data = &release_called;
  resolver_config.type = kFlutterAssetResolverTypeCustom;
  resolver_config.get_asset_callback = get_asset;

  EmbedderAssetResolver resolver(resolver_config);
  EXPECT_TRUE(resolver.IsValid());
  EXPECT_EQ(resolver.GetType(),
            AssetResolver::AssetResolverType::kCustomResolver);

  // Asset not found.
  {
    auto mapping = resolver.GetAsMapping("missing.txt");
    EXPECT_EQ(mapping, nullptr);
    EXPECT_FALSE(release_called);
  }

  // Asset found.
  {
    auto mapping = resolver.GetAsMapping("test.txt");
    ASSERT_NE(mapping, nullptr);
    EXPECT_EQ(mapping->GetSize(), kDataSize);
    EXPECT_EQ(std::memcmp(mapping->GetMapping(), kData, kDataSize), 0);
    EXPECT_FALSE(release_called);
  }
  // Upon destruction of mapping, release_callback must be called.
  EXPECT_TRUE(release_called);
}

TEST(EmbedderAssetResolverTest, MappingWithoutReleaseCallback) {
  static constexpr char kData[] = "static buffer";
  static constexpr size_t kDataSize = sizeof(kData) - 1;

  auto get_asset = [](const char* asset_name, FlutterMapping* mapping,
                      void* user_data) -> bool {
    mapping->struct_size = sizeof(FlutterMapping);
    mapping->mapping = reinterpret_cast<const uint8_t*>(kData);
    mapping->size = kDataSize;
    mapping->release_callback = nullptr;
    mapping->user_data = nullptr;
    return true;
  };

  FlutterAssetResolver resolver_config = {};
  resolver_config.struct_size = sizeof(FlutterAssetResolver);
  resolver_config.type = kFlutterAssetResolverTypeCustom;
  resolver_config.get_asset_callback = get_asset;

  EmbedderAssetResolver resolver(resolver_config);
  auto mapping = resolver.GetAsMapping("static.txt");
  ASSERT_NE(mapping, nullptr);
  EXPECT_EQ(mapping->GetSize(), kDataSize);
}

TEST(EmbedderAssetResolverTest, EmptyMapping) {
  bool release_called = false;
  static const uint8_t kDummyByte = 0;

  auto get_asset = [](const char* asset_name, FlutterMapping* mapping,
                      void* user_data) -> bool {
    auto* release_flag = reinterpret_cast<bool*>(user_data);
    mapping->struct_size = sizeof(FlutterMapping);
    mapping->mapping = &kDummyByte;
    mapping->size = 0;
    mapping->release_callback = [](void* release_data) {
      *reinterpret_cast<bool*>(release_data) = true;
    };
    mapping->user_data = release_flag;
    return true;
  };

  FlutterAssetResolver resolver_config = {};
  resolver_config.struct_size = sizeof(FlutterAssetResolver);
  resolver_config.user_data = &release_called;
  resolver_config.type = kFlutterAssetResolverTypeCustom;
  resolver_config.get_asset_callback = get_asset;

  EmbedderAssetResolver resolver(resolver_config);
  {
    auto mapping = resolver.GetAsMapping("empty.txt");
    ASSERT_NE(mapping, nullptr);
    EXPECT_EQ(mapping->GetSize(), 0u);
    EXPECT_FALSE(release_called);
  }
  EXPECT_TRUE(release_called);
}

TEST(EmbedderAssetResolverTest, NullDataInvokesReleaseCallback) {
  bool release_called = false;

  auto get_asset = [](const char* asset_name, FlutterMapping* mapping,
                      void* user_data) -> bool {
    auto* release_flag = reinterpret_cast<bool*>(user_data);
    mapping->struct_size = sizeof(FlutterMapping);
    mapping->mapping = nullptr;
    mapping->size = 0;
    mapping->release_callback = [](void* release_data) {
      *reinterpret_cast<bool*>(release_data) = true;
    };
    mapping->user_data = release_flag;
    return true;
  };

  FlutterAssetResolver resolver_config = {};
  resolver_config.struct_size = sizeof(FlutterAssetResolver);
  resolver_config.user_data = &release_called;
  resolver_config.type = kFlutterAssetResolverTypeCustom;
  resolver_config.get_asset_callback = get_asset;

  EmbedderAssetResolver resolver(resolver_config);
  auto mapping = resolver.GetAsMapping("null_asset.txt");
  EXPECT_EQ(mapping, nullptr);
  EXPECT_TRUE(release_called);
}

TEST(EmbedderAssetResolverTest, IsValidAfterAssetManagerChange) {
  FlutterAssetResolver resolver_config = {};
  resolver_config.struct_size = sizeof(FlutterAssetResolver);
  resolver_config.type = kFlutterAssetResolverTypeCustom;
  resolver_config.get_asset_callback = [](const char*, FlutterMapping*, void*) {
    return false;
  };

  // Defaults to true when callback is null.
  {
    EmbedderAssetResolver resolver(resolver_config);
    EXPECT_TRUE(resolver.IsValidAfterAssetManagerChange());
  }

  // Respects callback when provided.
  resolver_config.is_valid_after_asset_manager_change = [](void*) {
    return false;
  };
  {
    EmbedderAssetResolver resolver(resolver_config);
    EXPECT_FALSE(resolver.IsValidAfterAssetManagerChange());
  }
}

TEST(EmbedderAssetResolverTest, ResolverEquality) {
  auto cb1 = [](const char*, FlutterMapping*, void*) { return false; };
  auto cb2 = [](const char*, FlutterMapping*, void*) { return true; };

  FlutterAssetResolver config1 = {};
  config1.struct_size = sizeof(FlutterAssetResolver);
  config1.type = kFlutterAssetResolverTypeCustom;
  config1.get_asset_callback = cb1;
  config1.user_data = nullptr;

  FlutterAssetResolver config2 = config1;

  FlutterAssetResolver config3 = config1;
  config3.get_asset_callback = cb2;

  EmbedderAssetResolver res1(config1);
  EmbedderAssetResolver res2(config2);
  EmbedderAssetResolver res3(config3);

  EXPECT_TRUE(res1 == res2);
  EXPECT_FALSE(res1 == res3);
}

}  // namespace testing
}  // namespace flutter
