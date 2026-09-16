// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/apk_asset_provider.h"
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

TEST(APKAssetProvider, FlutterAssetResolverStructure) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  auto provider = std::make_unique<APKAssetProvider>(mock_impl);

  const FlutterAssetResolver* resolver = provider->GetFlutterAssetResolver();
  ASSERT_NE(resolver, nullptr);
  EXPECT_EQ(resolver->struct_size, sizeof(FlutterAssetResolver));
  EXPECT_EQ(resolver->type, kFlutterAssetResolverTypeAPK);
  EXPECT_EQ(resolver->user_data, mock_impl.get());
  EXPECT_NE(resolver->get_asset_callback, nullptr);
  EXPECT_NE(resolver->is_valid_after_asset_manager_change, nullptr);
  EXPECT_TRUE(
      resolver->is_valid_after_asset_manager_change(resolver->user_data));
}

TEST(APKAssetProvider, FlutterAssetResolverGetAssetSuccess) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  auto provider = std::make_unique<APKAssetProvider>(mock_impl);

  const std::string test_data = "flutter_asset_payload";
  EXPECT_CALL(*mock_impl, GetAsMapping("kernel_blob.bin"))
      .WillOnce([&test_data](const std::string&) {
        return std::make_unique<fml::NonOwnedMapping>(
            reinterpret_cast<const uint8_t*>(test_data.data()),
            test_data.size());
      });

  const FlutterAssetResolver* resolver = provider->GetFlutterAssetResolver();
  ASSERT_NE(resolver, nullptr);

  FlutterMapping mapping = {};
  bool success = resolver->get_asset_callback("kernel_blob.bin", &mapping,
                                              resolver->user_data);
  EXPECT_TRUE(success);
  EXPECT_EQ(mapping.struct_size, sizeof(FlutterMapping));
  EXPECT_EQ(mapping.size, test_data.size());
  ASSERT_NE(mapping.mapping, nullptr);
  EXPECT_EQ(
      std::string(reinterpret_cast<const char*>(mapping.mapping), mapping.size),
      test_data);
  ASSERT_NE(mapping.release_callback, nullptr);

  mapping.release_callback(mapping.user_data);
}

TEST(APKAssetProvider, FlutterAssetResolverGetAssetFailure) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  auto provider = std::make_unique<APKAssetProvider>(mock_impl);

  EXPECT_CALL(*mock_impl, GetAsMapping("nonexistent.bin"))
      .WillOnce(::testing::Return(nullptr));

  const FlutterAssetResolver* resolver = provider->GetFlutterAssetResolver();
  ASSERT_NE(resolver, nullptr);

  FlutterMapping mapping = {};
  bool success = resolver->get_asset_callback("nonexistent.bin", &mapping,
                                              resolver->user_data);
  EXPECT_FALSE(success);
}

TEST(APKAssetProvider, FlutterAssetResolverCloneMaintainsValidResolver) {
  auto mock_impl = std::make_shared<MockAPKAssetProviderImpl>();
  auto provider = std::make_unique<APKAssetProvider>(mock_impl);
  auto clone = provider->Clone();

  const FlutterAssetResolver* orig_resolver =
      provider->GetFlutterAssetResolver();
  const FlutterAssetResolver* clone_resolver = clone->GetFlutterAssetResolver();

  ASSERT_NE(orig_resolver, nullptr);
  ASSERT_NE(clone_resolver, nullptr);
  EXPECT_EQ(orig_resolver->user_data, clone_resolver->user_data);
  EXPECT_EQ(orig_resolver->type, clone_resolver->type);
  EXPECT_EQ(orig_resolver->get_asset_callback,
            clone_resolver->get_asset_callback);
}
}  // namespace testing
}  // namespace flutter
