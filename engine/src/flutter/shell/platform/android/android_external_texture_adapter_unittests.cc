// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_external_texture_adapter.h"

#include "flutter/shell/platform/embedder/embedder.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

namespace {

class MockExternalTextureDelegate
    : public AndroidExternalTextureAdapter::Delegate {
 public:
  bool on_register_called = false;
  std::shared_ptr<flutter::Texture> registered_texture;
  bool on_unregister_called = false;
  int64_t unregister_texture_id = -1;
  bool on_mark_available_called = false;
  int64_t mark_available_texture_id = -1;

  void OnRegisterTexture(std::shared_ptr<flutter::Texture> texture) override {
    on_register_called = true;
    registered_texture = std::move(texture);
  }

  void OnUnregisterTexture(int64_t texture_id) override {
    on_unregister_called = true;
    unregister_texture_id = texture_id;
  }

  void OnMarkTextureFrameAvailable(int64_t texture_id) override {
    on_mark_available_called = true;
    mark_available_texture_id = texture_id;
  }
};

}  // namespace

TEST(AndroidExternalTextureAdapterTest, EmbedderApiShapeRegistration) {
  MockExternalTextureDelegate delegate;
  AndroidExternalTextureAdapter adapter(nullptr, nullptr, &delegate);

  EXPECT_EQ(adapter.TextureCount(), 0u);
  EXPECT_FALSE(adapter.HasTexture(10));

  // Invalid IDs should return kInvalidArguments.
  EXPECT_EQ(adapter.RegisterExternalTexture(0), kInvalidArguments);
  EXPECT_EQ(adapter.RegisterExternalTexture(-5), kInvalidArguments);
  EXPECT_EQ(adapter.UnregisterExternalTexture(0), kInvalidArguments);
  EXPECT_EQ(adapter.UnregisterExternalTexture(-1), kInvalidArguments);
  EXPECT_EQ(adapter.MarkExternalTextureFrameAvailable(0), kInvalidArguments);
  EXPECT_EQ(adapter.MarkExternalTextureFrameAvailable(-1), kInvalidArguments);

  // Valid registration via Embedder API shape.
  EXPECT_EQ(adapter.RegisterExternalTexture(10), kSuccess);
  EXPECT_TRUE(adapter.HasTexture(10));
  EXPECT_EQ(adapter.TextureCount(), 1u);

  // Re-registration of same ID is idempotent and returns kSuccess.
  EXPECT_EQ(adapter.RegisterExternalTexture(10), kSuccess);
  EXPECT_EQ(adapter.TextureCount(), 1u);

  // Mark frame available delegates to Delegate.
  EXPECT_FALSE(delegate.on_mark_available_called);
  EXPECT_EQ(adapter.MarkExternalTextureFrameAvailable(10), kSuccess);
  EXPECT_TRUE(delegate.on_mark_available_called);
  EXPECT_EQ(delegate.mark_available_texture_id, 10);

  // Unregister delegates to Delegate and removes from adapter.
  EXPECT_FALSE(delegate.on_unregister_called);
  EXPECT_EQ(adapter.UnregisterExternalTexture(10), kSuccess);
  EXPECT_TRUE(delegate.on_unregister_called);
  EXPECT_EQ(delegate.unregister_texture_id, 10);
  EXPECT_FALSE(adapter.HasTexture(10));
  EXPECT_EQ(adapter.TextureCount(), 0u);
}

TEST(AndroidExternalTextureAdapterTest, NullContextSafelyFailsRegistration) {
  MockExternalTextureDelegate delegate;
  AndroidExternalTextureAdapter adapter(nullptr, nullptr, &delegate);

  fml::jni::ScopedJavaGlobalRef<jobject> null_ref;
  EXPECT_FALSE(adapter.RegisterSurfaceTexture(10, null_ref));
  EXPECT_FALSE(adapter.RegisterImageTexture(
      11, null_ref, ImageExternalTexture::ImageLifecycle::kReset));

  EXPECT_EQ(adapter.TextureCount(), 0u);
  EXPECT_FALSE(delegate.on_register_called);
}

TEST(AndroidExternalTextureAdapterTest, NullDelegateHandling) {
  AndroidExternalTextureAdapter adapter(nullptr, nullptr, nullptr);

  EXPECT_EQ(adapter.RegisterExternalTexture(20), kSuccess);
  EXPECT_TRUE(adapter.HasTexture(20));

  // Should not crash even with null delegate.
  EXPECT_EQ(adapter.MarkExternalTextureFrameAvailable(20), kSuccess);
  EXPECT_EQ(adapter.UnregisterExternalTexture(20), kSuccess);
  EXPECT_FALSE(adapter.HasTexture(20));
}

}  // namespace testing
}  // namespace flutter
