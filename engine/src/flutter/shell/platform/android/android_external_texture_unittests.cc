// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cstring>
#include <memory>
#include <string>

#include "flutter/shell/platform/android/android_hardware_buffer.h"
#include "gtest/gtest.h"

namespace flutter {
namespace android {
namespace testing {

namespace {

// Named constants for test dimensions, handles, and Vulkan formats.
constexpr uint32_t kTestTextureWidth = 640;
constexpr uint32_t kTestTextureHeight = 480;
constexpr uint32_t kGlTextureName = 42;
constexpr uint32_t kGlTextureExternalOesTarget = 0x8D65;
constexpr int64_t kTestTextureId = 101;

constexpr uintptr_t kDummyAhbRawHandle = 0xA001;
constexpr uintptr_t kDummyVkDevice = 0xB003;

constexpr uint64_t kExternalYcbcrFormat = 0x55AA1234ULL;
constexpr uint32_t kAllocatedMemoryTypeBits = 0x00000004u;
constexpr uint64_t kAllocationByteSize = 1024 * 1024;
constexpr uint32_t kVkFilterNearest = 0u;
constexpr uint32_t kVkFilterLinear = 1u;

struct MockNdkAhbEnv {
  int allocate_calls = 0;
  int acquire_calls = 0;
  int release_calls = 0;
  int lock_calls = 0;
  int unlock_calls = 0;
  uint8_t pixel_storage[256] = {};
};

MockNdkAhbEnv* g_ahb_env = nullptr;

int StubAHardwareBufferAllocate(const AndroidHardwareBufferDesc* desc,
                                void** out_buffer) {
  if (desc == nullptr || out_buffer == nullptr || !desc->IsValid()) {
    return -1;
  }
  if (g_ahb_env != nullptr) {
    ++g_ahb_env->allocate_calls;
  }
  *out_buffer = reinterpret_cast<void*>(kDummyAhbRawHandle);
  return 0;
}

void StubAHardwareBufferAcquire(void* /*buffer*/) {
  if (g_ahb_env != nullptr) {
    ++g_ahb_env->acquire_calls;
  }
}

void StubAHardwareBufferRelease(void* /*buffer*/) {
  if (g_ahb_env != nullptr) {
    ++g_ahb_env->release_calls;
  }
}

void StubAHardwareBufferDescribe(const void* /*buffer*/,
                                 AndroidHardwareBufferDesc* out_desc) {
  if (out_desc != nullptr) {
    *out_desc = AndroidHardwareBufferDesc::MakeRGBA8(kTestTextureWidth,
                                                     kTestTextureHeight);
  }
}

int StubAHardwareBufferLock(void* /*buffer*/,
                            uint64_t /*usage*/,
                            int32_t /*fence*/,
                            const AndroidHardwareBufferRect* /*rect*/,
                            void** out_virtual_address) {
  if (g_ahb_env != nullptr) {
    ++g_ahb_env->lock_calls;
    if (out_virtual_address != nullptr) {
      *out_virtual_address = g_ahb_env->pixel_storage;
    }
  }
  return 0;
}

int StubAHardwareBufferUnlock(void* /*buffer*/, int32_t* fence) {
  if (g_ahb_env != nullptr) {
    ++g_ahb_env->unlock_calls;
  }
  if (fence != nullptr) {
    *fence = -1;
  }
  return 0;
}

int StubAHardwareBufferSendHandle(const void* /*buffer*/, int /*socket_fd*/) {
  return 0;
}

int StubAHardwareBufferRecvHandle(int /*socket_fd*/, void** out_buffer) {
  if (out_buffer != nullptr) {
    *out_buffer = reinterpret_cast<void*>(kDummyAhbRawHandle);
  }
  return 0;
}

std::shared_ptr<MockOSLibrary> CreateMockAhbAndroidLibrary() {
  auto lib = std::make_shared<MockOSLibrary>();
  lib->RegisterSymbol("AHardwareBuffer_allocate",
                      reinterpret_cast<void*>(&StubAHardwareBufferAllocate));
  lib->RegisterSymbol("AHardwareBuffer_acquire",
                      reinterpret_cast<void*>(&StubAHardwareBufferAcquire));
  lib->RegisterSymbol("AHardwareBuffer_release",
                      reinterpret_cast<void*>(&StubAHardwareBufferRelease));
  lib->RegisterSymbol("AHardwareBuffer_describe",
                      reinterpret_cast<void*>(&StubAHardwareBufferDescribe));
  lib->RegisterSymbol("AHardwareBuffer_lock",
                      reinterpret_cast<void*>(&StubAHardwareBufferLock));
  lib->RegisterSymbol("AHardwareBuffer_unlock",
                      reinterpret_cast<void*>(&StubAHardwareBufferUnlock));
  lib->RegisterSymbol("AHardwareBuffer_sendHandleToUnixSocket",
                      reinterpret_cast<void*>(&StubAHardwareBufferSendHandle));
  lib->RegisterSymbol("AHardwareBuffer_recvHandleFromUnixSocket",
                      reinterpret_cast<void*>(&StubAHardwareBufferRecvHandle));
  return lib;
}

}  // namespace

// =============================================================================
// AndroidHardwareBuffer & OpenGL External Texture Tests
// =============================================================================

TEST(AndroidExternalTextureTest,
     InMemoryAndDefaultHardwareBufferProvidersManageLifecycle) {
  // 1. InMemoryAndroidHardwareBufferProvider
  auto mem_provider = std::make_shared<InMemoryAndroidHardwareBufferProvider>();
  EXPECT_TRUE(mem_provider->IsAvailable());

  auto desc = AndroidHardwareBufferDesc::MakeRGBA8(kTestTextureWidth,
                                                   kTestTextureHeight);
  EXPECT_TRUE(desc.IsValid());

  auto mem_buf = mem_provider->Allocate(desc);
  ASSERT_NE(mem_buf, nullptr);
  EXPECT_TRUE(mem_buf->IsValid());
  EXPECT_EQ(mem_buf->GetDescription(), desc);

  void* locked_addr = nullptr;
  EXPECT_EQ(mem_buf->Lock(AndroidHardwareBufferUsage::kCpuWriteOften, -1,
                          nullptr, &locked_addr),
            0);
  EXPECT_NE(locked_addr, nullptr);
  int32_t unlock_fence = -1;
  EXPECT_EQ(mem_buf->Unlock(&unlock_fence), 0);

  // 2. DefaultAndroidHardwareBufferProvider with mocked libandroid.so
  MockNdkAhbEnv ahb_env;
  g_ahb_env = &ahb_env;

  auto loader = std::make_shared<MockOSLibraryLoader>();
  loader->RegisterLibrary("libandroid.so", CreateMockAhbAndroidLibrary());

  auto default_provider =
      std::make_shared<DefaultAndroidHardwareBufferProvider>(loader);
  EXPECT_TRUE(default_provider->IsAvailable());

  auto ndk_buf = default_provider->Allocate(desc);
  ASSERT_NE(ndk_buf, nullptr);
  EXPECT_TRUE(ndk_buf->IsValid());
  EXPECT_EQ(ahb_env.allocate_calls, 1);

  void* ndk_locked = nullptr;
  EXPECT_EQ(ndk_buf->Lock(AndroidHardwareBufferUsage::kCpuReadOften, -1,
                          nullptr, &ndk_locked),
            0);
  EXPECT_EQ(ndk_locked, ahb_env.pixel_storage);
  EXPECT_EQ(ndk_buf->Unlock(nullptr), 0);

  ndk_buf.reset();
  EXPECT_EQ(ahb_env.release_calls, 1);
  g_ahb_env = nullptr;
}

TEST(AndroidExternalTextureTest,
     OpenGLExternalTexturePopulatesSurfaceTextureUvTransformAndAhbTarget) {
  // 1. SurfaceProducer AHB on OpenGL ES -> `has_uv_transform = false`, identity
  // matrix.
  auto mem_provider = std::make_shared<InMemoryAndroidHardwareBufferProvider>();
  auto ahb = mem_provider->Allocate(AndroidHardwareBufferDesc::MakeRGBA8(
      kTestTextureWidth, kTestTextureHeight));
  ASSERT_NE(ahb, nullptr);

  FlutterOpenGLExternalTexture gl_tex_ahb = {};
  EXPECT_TRUE(PopulateFlutterOpenGLExternalTextureFromHardwareBuffer(
      ahb.get(), kGlTextureName, &gl_tex_ahb));
  EXPECT_EQ(gl_tex_ahb.target, kGlTextureExternalOesTarget);
  EXPECT_EQ(gl_tex_ahb.name, kGlTextureName);
  EXPECT_EQ(gl_tex_ahb.width, kTestTextureWidth);
  EXPECT_EQ(gl_tex_ahb.height, kTestTextureHeight);
  EXPECT_FALSE(gl_tex_ahb.has_uv_transform);
  EXPECT_FLOAT_EQ(gl_tex_ahb.uv_transform[0], 1.0f);
  EXPECT_FLOAT_EQ(gl_tex_ahb.uv_transform[5], 1.0f);

  // 2. SurfaceTexture on OpenGL ES -> `has_uv_transform = true` and copies the
  // 4x4 column-major transform matrix from `SurfaceTexture.getTransformMatrix`.
  const std::array<float, 16> vertical_flip_matrix = {
      1.0f, 0.0f,  0.0f, 0.0f,  // col 0
      0.0f, -1.0f, 0.0f, 0.0f,  // col 1 (flip V)
      0.0f, 0.0f,  1.0f, 0.0f,  // col 2
      0.0f, 1.0f,  0.0f, 1.0f,  // col 3 (translate V by +1)
  };

  FlutterOpenGLExternalTexture gl_tex_st = {};
  EXPECT_TRUE(PopulateFlutterOpenGLExternalTextureFromSurfaceTexture(
      kGlTextureName, kTestTextureWidth, kTestTextureHeight,
      vertical_flip_matrix.data(), &gl_tex_st));
  EXPECT_EQ(gl_tex_st.target, kGlTextureExternalOesTarget);
  EXPECT_EQ(gl_tex_st.name, kGlTextureName);
  EXPECT_TRUE(gl_tex_st.has_uv_transform);
  for (size_t i = 0; i < vertical_flip_matrix.size(); ++i) {
    EXPECT_FLOAT_EQ(gl_tex_st.uv_transform[i], vertical_flip_matrix[i]);
  }
}

}  // namespace testing
}  // namespace android
}  // namespace flutter
