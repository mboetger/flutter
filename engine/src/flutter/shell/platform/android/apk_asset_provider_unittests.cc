// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/apk_asset_provider.h"

#include <cstring>
#include <memory>
#include <string>

#include "flutter/fml/mapping.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {
namespace {

class MockAPKAssetProviderImpl : public APKAssetProviderInternal {
 public:
  MOCK_METHOD(std::unique_ptr<fml::Mapping>,
              GetAsMapping,
              (const std::string& asset_name),
              (const, override));
};

}  // namespace

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

TEST(APKAssetProvider, GetAssetResolverConfigStructure) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  auto provider = std::make_unique<APKAssetProvider>(mock_impl);

  FlutterAssetResolverConfig config = provider->GetAssetResolverConfig();

  EXPECT_EQ(config.struct_size, sizeof(FlutterAssetResolverConfig));
  EXPECT_EQ(config.type, kFlutterAssetResolverTypeAPK);
  EXPECT_EQ(config.user_data, provider.get());
  EXPECT_NE(config.get_asset, nullptr);
  EXPECT_NE(config.is_valid_after_asset_manager_change, nullptr);

  EXPECT_TRUE(config.is_valid_after_asset_manager_change(config.user_data));
  EXPECT_TRUE(config.is_valid_after_asset_manager_change(nullptr));
}

TEST(APKAssetProvider, GetAssetSuccess) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  const std::string asset_data = "Flutter APK Asset Content 12345";
  EXPECT_CALL(*mock_impl, GetAsMapping("assets/test.json"))
      .WillOnce([&asset_data](const std::string&) {
        return std::make_unique<fml::NonOwnedMapping>(
            reinterpret_cast<const uint8_t*>(asset_data.data()),
            asset_data.size());
      });

  auto provider = std::make_unique<APKAssetProvider>(mock_impl);

  FlutterAssetMapping mapping = {};
  bool result = provider->GetAsset("assets/test.json", &mapping);

  EXPECT_TRUE(result);
  EXPECT_EQ(mapping.struct_size, sizeof(FlutterAssetMapping));
  EXPECT_EQ(mapping.size, asset_data.size());
  ASSERT_NE(mapping.data, nullptr);
  EXPECT_EQ(std::memcmp(mapping.data, asset_data.data(), asset_data.size()), 0);
  ASSERT_NE(mapping.release_proc, nullptr);

  // Invoke release_proc to ensure it safely destructs the mapping.
  mapping.release_proc(mapping.release_user_data);
}

TEST(APKAssetProvider, GetAssetCallbackSuccess) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  const std::string asset_data = "Callback Test Data";
  EXPECT_CALL(*mock_impl, GetAsMapping("AssetManifest.json"))
      .WillOnce([&asset_data](const std::string&) {
        return std::make_unique<fml::NonOwnedMapping>(
            reinterpret_cast<const uint8_t*>(asset_data.data()),
            asset_data.size());
      });

  auto provider = std::make_unique<APKAssetProvider>(mock_impl);
  FlutterAssetResolverConfig config = provider->GetAssetResolverConfig();

  FlutterAssetMapping mapping = {};
  bool result =
      config.get_asset("AssetManifest.json", &mapping, config.user_data);

  EXPECT_TRUE(result);
  EXPECT_EQ(mapping.struct_size, sizeof(FlutterAssetMapping));
  EXPECT_EQ(mapping.size, asset_data.size());
  ASSERT_NE(mapping.data, nullptr);
  EXPECT_EQ(std::memcmp(mapping.data, asset_data.data(), asset_data.size()), 0);
  ASSERT_NE(mapping.release_proc, nullptr);

  mapping.release_proc(mapping.release_user_data);
}

TEST(APKAssetProvider, GetAssetNotFound) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  EXPECT_CALL(*mock_impl, GetAsMapping("missing_asset.png"))
      .WillOnce(::testing::Return(nullptr));

  auto provider = std::make_unique<APKAssetProvider>(mock_impl);

  FlutterAssetMapping mapping = {};
  bool result = provider->GetAsset("missing_asset.png", &mapping);

  EXPECT_FALSE(result);
}

TEST(APKAssetProvider, GetAssetNullArguments) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  auto provider = std::make_unique<APKAssetProvider>(mock_impl);

  FlutterAssetMapping mapping = {};
  EXPECT_FALSE(provider->GetAsset(nullptr, &mapping));
  EXPECT_FALSE(provider->GetAsset("test.json", nullptr));

  FlutterAssetResolverConfig config = provider->GetAssetResolverConfig();
  EXPECT_FALSE(config.get_asset("test.json", &mapping, nullptr));
  EXPECT_FALSE(config.get_asset(nullptr, &mapping, config.user_data));
  EXPECT_FALSE(config.get_asset("test.json", nullptr, config.user_data));
}

TEST(APKAssetProvider, GetAssetEmptyBuffer) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  EXPECT_CALL(*mock_impl, GetAsMapping("empty.txt"))
      .WillOnce([](const std::string&) {
        return std::make_unique<fml::NonOwnedMapping>(nullptr, 0);
      });

  auto provider = std::make_unique<APKAssetProvider>(mock_impl);

  FlutterAssetMapping mapping = {};
  bool result = provider->GetAsset("empty.txt", &mapping);

  EXPECT_TRUE(result);
  EXPECT_EQ(mapping.struct_size, sizeof(FlutterAssetMapping));
  EXPECT_EQ(mapping.size, 0u);
  EXPECT_NE(mapping.data, nullptr);
  ASSERT_NE(mapping.release_proc, nullptr);

  mapping.release_proc(mapping.release_user_data);
}

TEST(APKAssetProvider, GetAssetCorruptedNullBufferWithPositiveSize) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  EXPECT_CALL(*mock_impl, GetAsMapping("corrupted.bin"))
      .WillOnce([](const std::string&) {
        return std::make_unique<fml::NonOwnedMapping>(nullptr, 128);
      });

  auto provider = std::make_unique<APKAssetProvider>(mock_impl);

  FlutterAssetMapping mapping = {};
  bool result = provider->GetAsset("corrupted.bin", &mapping);

  EXPECT_FALSE(result);
}

}  // namespace testing
}  // namespace flutter
