// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <memory>
#include <string>
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
  ASSERT_FALSE(*first_provider == *second_provider);
  ASSERT_TRUE(*first_provider == *third_provider);
}

TEST(APKAssetProvider, GetAssetResolverFields) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  APKAssetProvider provider(mock_impl);

  FlutterAssetResolver resolver = provider.GetAssetResolver();
  EXPECT_EQ(resolver.struct_size, sizeof(FlutterAssetResolver));
  EXPECT_EQ(resolver.user_data, mock_impl.get());
  EXPECT_NE(resolver.get_asset_callback, nullptr);
}

TEST(APKAssetProvider, GetAssetResolverCallbackSuccessAndRelease) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  APKAssetProvider provider(mock_impl);

  // 12-byte arbitrary payload data for testing.
  const std::string test_payload = "asset_data!!";
  auto mapping = std::make_unique<fml::DataMapping>(
      std::vector<uint8_t>(test_payload.begin(), test_payload.end()));

  EXPECT_CALL(*mock_impl, GetAsMapping("flutter_assets/kernel_blob.bin"))
      .WillOnce(::testing::Return(::testing::ByMove(std::move(mapping))));

  FlutterAssetResolver resolver = provider.GetAssetResolver();
  ASSERT_NE(resolver.get_asset_callback, nullptr);

  FlutterMapping flutter_mapping = resolver.get_asset_callback(
      "flutter_assets/kernel_blob.bin", resolver.user_data);
  EXPECT_EQ(flutter_mapping.struct_size, sizeof(FlutterMapping));
  EXPECT_NE(flutter_mapping.mapping, nullptr);
  EXPECT_EQ(flutter_mapping.size, test_payload.size());
  EXPECT_EQ(std::memcmp(flutter_mapping.mapping, test_payload.data(),
                        test_payload.size()),
            0);
  EXPECT_NE(flutter_mapping.user_data, nullptr);
  ASSERT_NE(flutter_mapping.release_callback, nullptr);

  // Verify release callback executes cleanly without leaking memory or
  // crashing.
  flutter_mapping.release_callback(flutter_mapping.user_data);
}

TEST(APKAssetProvider, GetAssetResolverCallbackNotFound) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  APKAssetProvider provider(mock_impl);

  EXPECT_CALL(*mock_impl, GetAsMapping("missing_asset.txt"))
      .WillOnce(::testing::Return(::testing::ByMove(nullptr)));

  FlutterAssetResolver resolver = provider.GetAssetResolver();
  ASSERT_NE(resolver.get_asset_callback, nullptr);

  FlutterMapping flutter_mapping =
      resolver.get_asset_callback("missing_asset.txt", resolver.user_data);
  EXPECT_EQ(flutter_mapping.struct_size, sizeof(FlutterMapping));
  EXPECT_EQ(flutter_mapping.mapping, nullptr);
  EXPECT_EQ(flutter_mapping.size, 0u);
  EXPECT_EQ(flutter_mapping.user_data, nullptr);
  EXPECT_EQ(flutter_mapping.release_callback, nullptr);
}

namespace {

class NullBufferMapping : public fml::Mapping {
 public:
  ~NullBufferMapping() override = default;
  size_t GetSize() const override { return 0; }
  const uint8_t* GetMapping() const override { return nullptr; }
  bool IsDontNeedSafe() const override { return false; }
};

}  // namespace

TEST(APKAssetProvider, GetAssetResolverCallbackNullBufferHandledSafely) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  APKAssetProvider provider(mock_impl);

  EXPECT_CALL(*mock_impl, GetAsMapping("unmapped_asset.bin"))
      .WillOnce(::testing::Return(
          ::testing::ByMove(std::make_unique<NullBufferMapping>())));

  FlutterAssetResolver resolver = provider.GetAssetResolver();
  ASSERT_NE(resolver.get_asset_callback, nullptr);

  FlutterMapping flutter_mapping =
      resolver.get_asset_callback("unmapped_asset.bin", resolver.user_data);
  EXPECT_EQ(flutter_mapping.struct_size, sizeof(FlutterMapping));
  EXPECT_EQ(flutter_mapping.mapping, nullptr);
  EXPECT_EQ(flutter_mapping.size, 0u);
  EXPECT_EQ(flutter_mapping.user_data, nullptr);
  EXPECT_EQ(flutter_mapping.release_callback, nullptr);
}

TEST(APKAssetProvider, GetAssetResolverCallbackArgumentValidation) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  APKAssetProvider provider(mock_impl);

  FlutterAssetResolver resolver = provider.GetAssetResolver();
  ASSERT_NE(resolver.get_asset_callback, nullptr);

  // 1. Null user data.
  FlutterMapping null_user_data_mapping =
      resolver.get_asset_callback("test.txt", nullptr);
  EXPECT_EQ(null_user_data_mapping.struct_size, sizeof(FlutterMapping));
  EXPECT_EQ(null_user_data_mapping.mapping, nullptr);
  EXPECT_EQ(null_user_data_mapping.size, 0u);
  EXPECT_EQ(null_user_data_mapping.user_data, nullptr);
  EXPECT_EQ(null_user_data_mapping.release_callback, nullptr);

  // 2. Null asset name.
  FlutterMapping null_asset_mapping =
      resolver.get_asset_callback(nullptr, resolver.user_data);
  EXPECT_EQ(null_asset_mapping.struct_size, sizeof(FlutterMapping));
  EXPECT_EQ(null_asset_mapping.mapping, nullptr);
  EXPECT_EQ(null_asset_mapping.size, 0u);
  EXPECT_EQ(null_asset_mapping.user_data, nullptr);
  EXPECT_EQ(null_asset_mapping.release_callback, nullptr);
}

TEST(APKAssetProvider, IntegrationWithEmbedderAssetResolver) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  APKAssetProvider provider(mock_impl);

  const std::string payload = "embedded_payload";
  auto data_mapping = std::make_unique<fml::DataMapping>(
      std::vector<uint8_t>(payload.begin(), payload.end()));

  EXPECT_CALL(*mock_impl, GetAsMapping("assets/Font.ttf"))
      .WillOnce(::testing::Return(::testing::ByMove(std::move(data_mapping))));

  FlutterAssetResolver resolver = provider.GetAssetResolver();
  EmbedderAssetResolver embedder_resolver(resolver);

  EXPECT_TRUE(embedder_resolver.IsValid());
  EXPECT_EQ(embedder_resolver.GetType(),
            AssetResolver::AssetResolverType::kEmbedderAssetResolver);

  auto resolved_mapping = embedder_resolver.GetAsMapping("assets/Font.ttf");
  ASSERT_NE(resolved_mapping, nullptr);
  EXPECT_EQ(resolved_mapping->GetSize(), payload.size());
  EXPECT_EQ(std::memcmp(resolved_mapping->GetMapping(), payload.data(),
                        payload.size()),
            0);
}

}  // namespace testing
}  // namespace flutter
