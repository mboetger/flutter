// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "flutter/fml/mapping.h"
#include "flutter/shell/platform/android/apk_asset_provider.h"
#include "flutter/shell/platform/embedder/embedder.h"
#include "flutter/shell/platform/embedder/embedder_asset_resolver.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

class MockAPKAssetProviderImpl : public APKAssetProviderInternal {
 public:
  MOCK_METHOD(std::unique_ptr<fml::Mapping>,
              GetAsMapping,
              (const std::string& asset_name),
              (const, override));
};

TEST(APKAssetProvider, CloneAndEquals) {
  auto first_provider = std::make_unique<APKAssetProvider>(
      std::make_shared<MockAPKAssetProviderImpl>());
  auto second_provider = std::make_unique<APKAssetProvider>(
      std::make_shared<MockAPKAssetProviderImpl>());
  auto third_provider = first_provider->Clone();

  ASSERT_NE(first_provider->GetImpl(), second_provider->GetImpl());
  ASSERT_EQ(first_provider->GetImpl(), third_provider->GetImpl());
}

TEST(APKAssetProvider, GetAssetResolverProperties) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  APKAssetProvider provider(mock_impl);

  FlutterAssetResolver resolver = provider.GetAssetResolver();
  EXPECT_EQ(resolver.struct_size, sizeof(FlutterAssetResolver));
  EXPECT_EQ(resolver.user_data, mock_impl.get());
  EXPECT_NE(resolver.get_asset, nullptr);

  FlutterAssetResolver resolver_config = provider.GetAssetResolverConfig();
  EXPECT_EQ(resolver_config.struct_size, sizeof(FlutterAssetResolver));
  EXPECT_EQ(resolver_config.user_data, mock_impl.get());
  EXPECT_NE(resolver_config.get_asset, nullptr);

  APKAssetProvider null_impl_provider(nullptr);
  FlutterAssetResolver null_resolver = null_impl_provider.GetAssetResolver();
  EXPECT_EQ(null_resolver.struct_size, sizeof(FlutterAssetResolver));
  EXPECT_EQ(null_resolver.user_data, nullptr);
  EXPECT_EQ(null_resolver.get_asset, nullptr);
}

TEST(APKAssetProvider, GetAssetResolverResolution) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  APKAssetProvider provider(mock_impl);
  FlutterAssetResolver resolver = provider.GetAssetResolver();

  const std::string payload = "flutter_asset_data_payload";
  EXPECT_CALL(*mock_impl, GetAsMapping("AssetManifest.json"))
      .WillOnce([&payload](const std::string&) {
        return std::make_unique<fml::DataMapping>(
            std::vector<uint8_t>(payload.begin(), payload.end()));
      });

  FlutterMapping mapping_out = {};
  mapping_out.struct_size = sizeof(FlutterMapping);

  bool success = resolver.get_asset("AssetManifest.json", &mapping_out,
                                    resolver.user_data);
  EXPECT_TRUE(success);
  EXPECT_NE(mapping_out.mapping, nullptr);
  EXPECT_EQ(mapping_out.size, payload.size());
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(mapping_out.mapping),
                        mapping_out.size),
            payload);
  EXPECT_NE(mapping_out.release_callback, nullptr);
  EXPECT_NE(mapping_out.user_data, nullptr);

  // Invoke release callback to verify clean resource teardown.
  mapping_out.release_callback(mapping_out.mapping, mapping_out.size,
                               mapping_out.user_data);
}

TEST(APKAssetProvider, GetAssetResolverNotFound) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  APKAssetProvider provider(mock_impl);
  FlutterAssetResolver resolver = provider.GetAssetResolver();

  EXPECT_CALL(*mock_impl, GetAsMapping("nonexistent_asset.bin"))
      .WillOnce(::testing::Return(nullptr));

  FlutterMapping mapping_out = {};
  mapping_out.struct_size = sizeof(FlutterMapping);

  bool success = resolver.get_asset("nonexistent_asset.bin", &mapping_out,
                                    resolver.user_data);
  EXPECT_FALSE(success);
}

TEST(APKAssetProvider, GetAssetResolverInvalidArguments) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  APKAssetProvider provider(mock_impl);
  FlutterAssetResolver resolver = provider.GetAssetResolver();

  FlutterMapping mapping_out = {};
  mapping_out.struct_size = sizeof(FlutterMapping);

  // Null asset name
  EXPECT_FALSE(resolver.get_asset(nullptr, &mapping_out, resolver.user_data));

  // Null output mapping
  EXPECT_FALSE(resolver.get_asset("some_asset", nullptr, resolver.user_data));

  // Null user data
  EXPECT_FALSE(resolver.get_asset("some_asset", &mapping_out, nullptr));

  // Struct size mismatch
  mapping_out.struct_size = sizeof(FlutterMapping) - 1;
  EXPECT_FALSE(
      resolver.get_asset("some_asset", &mapping_out, resolver.user_data));
}

TEST(APKAssetProvider, EmbedderAssetResolverIntegration) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  APKAssetProvider provider(mock_impl);
  FlutterAssetResolver c_resolver = provider.GetAssetResolver();

  EmbedderAssetResolver embedder_resolver(&c_resolver);
  EXPECT_TRUE(embedder_resolver.IsValid());
  EXPECT_EQ(embedder_resolver.GetType(),
            AssetResolver::AssetResolverType::kCustomResolver);

  const std::string payload = "integrated_asset_bytes";
  EXPECT_CALL(*mock_impl, GetAsMapping("kernel_blob.bin"))
      .WillOnce([&payload](const std::string&) {
        return std::make_unique<fml::DataMapping>(
            std::vector<uint8_t>(payload.begin(), payload.end()));
      });

  auto mapping = embedder_resolver.GetAsMapping("kernel_blob.bin");
  ASSERT_NE(mapping, nullptr);
  EXPECT_EQ(mapping->GetSize(), payload.size());
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(mapping->GetMapping()),
                        mapping->GetSize()),
            payload);

  // Verify equality operators
  auto second_provider = std::make_unique<APKAssetProvider>(mock_impl);
  FlutterAssetResolver second_c_resolver = second_provider->GetAssetResolver();
  EmbedderAssetResolver second_embedder_resolver(&second_c_resolver);
  EXPECT_TRUE(embedder_resolver == second_embedder_resolver);
}

TEST(APKAssetProvider, GetAssetResolverCorruptMapping) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  APKAssetProvider provider(mock_impl);
  FlutterAssetResolver resolver = provider.GetAssetResolver();

  // Return a mapping where GetMapping() == nullptr but GetSize() > 0.
  EXPECT_CALL(*mock_impl, GetAsMapping("corrupt_asset.bin"))
      .WillOnce([](const std::string&) {
        return std::make_unique<fml::NonOwnedMapping>(nullptr, 1024);
      });

  FlutterMapping mapping_out = {};
  mapping_out.struct_size = sizeof(FlutterMapping);

  bool success =
      resolver.get_asset("corrupt_asset.bin", &mapping_out, resolver.user_data);
  EXPECT_FALSE(success);
}

TEST(APKAssetProvider, ConcurrentAssetResolution) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  APKAssetProvider provider(mock_impl);
  FlutterAssetResolver resolver = provider.GetAssetResolver();

  constexpr int kNumThreads = 8;
  constexpr int kResolutionsPerThread = 50;
  std::atomic<int> completed_resolutions{0};

  EXPECT_CALL(*mock_impl, GetAsMapping(::testing::_))
      .WillRepeatedly([](const std::string& asset_name) {
        return std::make_unique<fml::DataMapping>(
            std::vector<uint8_t>(asset_name.begin(), asset_name.end()));
      });

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);

  for (int t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([&, t]() {
      for (int i = 0; i < kResolutionsPerThread; ++i) {
        std::string asset_name =
            "asset_t" + std::to_string(t) + "_i" + std::to_string(i) + ".bin";
        FlutterMapping mapping_out = {};
        mapping_out.struct_size = sizeof(FlutterMapping);

        bool success = resolver.get_asset(asset_name.c_str(), &mapping_out,
                                          resolver.user_data);
        EXPECT_TRUE(success);
        EXPECT_NE(mapping_out.mapping, nullptr);
        EXPECT_EQ(mapping_out.size, asset_name.size());
        EXPECT_NE(mapping_out.release_callback, nullptr);

        mapping_out.release_callback(mapping_out.mapping, mapping_out.size,
                                     mapping_out.user_data);
        completed_resolutions.fetch_add(1);
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(completed_resolutions.load(), kNumThreads * kResolutionsPerThread);
}

}  // namespace testing
}  // namespace flutter
