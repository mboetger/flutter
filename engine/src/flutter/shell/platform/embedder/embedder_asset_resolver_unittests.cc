// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/embedder/embedder_asset_resolver.h"

#include <cstring>
#include <string>

#include "flutter/testing/testing.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

namespace {

class DummyNonEmbedderAssetResolver : public AssetResolver {
 public:
  DummyNonEmbedderAssetResolver() = default;
  ~DummyNonEmbedderAssetResolver() override = default;

  bool IsValid() const override { return true; }
  bool IsValidAfterAssetManagerChange() const override { return true; }
  AssetResolver::AssetResolverType GetType() const override {
    return AssetResolver::AssetResolverType::kCustomResolver;
  }
  std::unique_ptr<fml::Mapping> GetAsMapping(
      const std::string& asset_name) const override {
    return nullptr;
  }
  bool operator==(const AssetResolver& other) const override {
    return this == &other;
  }
};

}  // namespace

TEST(EmbedderAssetResolverTest, CustomResolverLifecycleAndResolution) {
  const std::string kAssetContent = "Hello from embedder asset resolver!";

  auto get_asset = [](const char* asset_name, FlutterAssetMapping* mapping,
                      void* user_data) -> bool {
    if (std::strcmp(asset_name, "test_asset.txt") != 0) {
      return false;
    }
    const auto* content = reinterpret_cast<const std::string*>(user_data);
    mapping->struct_size = sizeof(FlutterAssetMapping);
    mapping->data = reinterpret_cast<const uint8_t*>(content->data());
    mapping->size = content->size();
    mapping->release_proc = nullptr;
    mapping->release_user_data = nullptr;
    return true;
  };

  FlutterAssetResolverConfig config = {};
  config.struct_size = sizeof(FlutterAssetResolverConfig);
  config.user_data = const_cast<std::string*>(&kAssetContent);
  config.type = kFlutterAssetResolverTypeCustom;
  config.get_asset = get_asset;
  config.is_valid_after_asset_manager_change = [](void*) { return true; };

  EmbedderAssetResolver resolver(config);
  EXPECT_TRUE(resolver.IsValid());
  EXPECT_TRUE(resolver.IsValidAfterAssetManagerChange());
  EXPECT_EQ(resolver.GetType(),
            AssetResolver::AssetResolverType::kCustomResolver);

  // Asset not found.
  auto not_found_mapping = resolver.GetAsMapping("non_existent.txt");
  EXPECT_EQ(not_found_mapping, nullptr);

  // Asset found.
  {
    auto mapping = resolver.GetAsMapping("test_asset.txt");
    ASSERT_NE(mapping, nullptr);
    EXPECT_EQ(mapping->GetSize(), kAssetContent.size());
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(mapping->GetMapping()),
                          mapping->GetSize()),
              kAssetContent);
  }

  // Equality operator.
  FlutterAssetResolverConfig config_copy = config;
  EmbedderAssetResolver resolver_same(config_copy);
  EXPECT_TRUE(resolver == resolver_same);

  FlutterAssetResolverConfig config_diff_type = config;
  config_diff_type.type = kFlutterAssetResolverTypeAPK;
  EmbedderAssetResolver resolver_diff_type(config_diff_type);
  EXPECT_FALSE(resolver == resolver_diff_type);

  // Compare against a non-EmbedderAssetResolver with matching GetType().
  DummyNonEmbedderAssetResolver non_embedder_resolver;
  EXPECT_FALSE(resolver == non_embedder_resolver);
}

TEST(EmbedderAssetResolverTest, ReleaseProcInvoked) {
  bool release_called = false;
  const std::string kData = "sample data";

  auto get_asset = [](const char* asset_name, FlutterAssetMapping* mapping,
                      void* user_data) -> bool {
    const auto* content = reinterpret_cast<const std::string*>(user_data);
    mapping->struct_size = sizeof(FlutterAssetMapping);
    mapping->data = reinterpret_cast<const uint8_t*>(content->data());
    mapping->size = content->size();
    mapping->release_proc = [](void* release_data) {
      *reinterpret_cast<bool*>(release_data) = true;
    };
    return true;
  };

  FlutterAssetResolverConfig config = {};
  config.struct_size = sizeof(FlutterAssetResolverConfig);
  config.user_data = const_cast<std::string*>(&kData);
  config.type = kFlutterAssetResolverTypeCustom;
  config.get_asset = get_asset;

  EmbedderAssetResolver resolver(config);
  {
    auto get_asset_with_release_data = [](const char* asset_name,
                                          FlutterAssetMapping* mapping,
                                          void* user_data) -> bool {
      auto* release_flag = reinterpret_cast<bool*>(user_data);
      static const char* kStaticData = "test";
      mapping->struct_size = sizeof(FlutterAssetMapping);
      mapping->data = reinterpret_cast<const uint8_t*>(kStaticData);
      mapping->size = std::strlen(kStaticData);
      mapping->release_proc = [](void* release_data) {
        *reinterpret_cast<bool*>(release_data) = true;
      };
      mapping->release_user_data = release_flag;
      return true;
    };

    config.user_data = &release_called;
    config.get_asset = get_asset_with_release_data;
    EmbedderAssetResolver resolver_with_release(config);
    auto mapping = resolver_with_release.GetAsMapping("foo");
    ASSERT_NE(mapping, nullptr);
    EXPECT_FALSE(release_called);
  }
  // Now mapping is destroyed, release_called must be true.
  EXPECT_TRUE(release_called);
}

TEST(EmbedderAssetResolverTest, ZeroByteAssetSupported) {
  static const uint8_t kDummyData = 0;
  bool release_called = false;

  auto get_asset = [](const char* asset_name, FlutterAssetMapping* mapping,
                      void* user_data) -> bool {
    auto* release_flag = reinterpret_cast<bool*>(user_data);
    mapping->struct_size = sizeof(FlutterAssetMapping);
    mapping->data = &kDummyData;
    mapping->size = 0;
    mapping->release_proc = [](void* release_data) {
      *reinterpret_cast<bool*>(release_data) = true;
    };
    mapping->release_user_data = release_flag;
    return true;
  };

  FlutterAssetResolverConfig config = {};
  config.struct_size = sizeof(FlutterAssetResolverConfig);
  config.user_data = &release_called;
  config.type = kFlutterAssetResolverTypeCustom;
  config.get_asset = get_asset;

  EmbedderAssetResolver resolver(config);
  {
    auto mapping = resolver.GetAsMapping("empty_file.txt");
    ASSERT_NE(mapping, nullptr);
    EXPECT_EQ(mapping->GetSize(), 0u);
    EXPECT_FALSE(release_called);
  }
  EXPECT_TRUE(release_called);
}

TEST(EmbedderAssetResolverTest, NullDataInvokesReleaseProc) {
  bool release_called = false;

  auto get_asset = [](const char* asset_name, FlutterAssetMapping* mapping,
                      void* user_data) -> bool {
    auto* release_flag = reinterpret_cast<bool*>(user_data);
    mapping->struct_size = sizeof(FlutterAssetMapping);
    mapping->data = nullptr;
    mapping->size = 0;
    mapping->release_proc = [](void* release_data) {
      *reinterpret_cast<bool*>(release_data) = true;
    };
    mapping->release_user_data = release_flag;
    return true;
  };

  FlutterAssetResolverConfig config = {};
  config.struct_size = sizeof(FlutterAssetResolverConfig);
  config.user_data = &release_called;
  config.type = kFlutterAssetResolverTypeCustom;
  config.get_asset = get_asset;

  EmbedderAssetResolver resolver(config);
  auto mapping = resolver.GetAsMapping("corrupt_asset");
  EXPECT_EQ(mapping, nullptr);
  EXPECT_TRUE(release_called);
}

}  // namespace testing
}  // namespace flutter
