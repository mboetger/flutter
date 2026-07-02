// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include <memory>
#include "flutter/shell/common/thread_host.h"
#include "flutter/shell/platform/android/android_context_gl_skia.h"
#include "flutter/shell/platform/android/android_egl_surface.h"
#include "flutter/shell/platform/android/android_environment_gl.h"
#include "flutter/shell/platform/android/android_surface_gl_skia.h"
#include "fml/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "impeller/core/runtime_types.h"
#include "shell/platform/android/context/android_context.h"
#include "third_party/skia/include/gpu/ganesh/mock/GrMockTypes.h"

#include <dlfcn.h>
#include <vector>

// Global variables to record the last passed attributes.
static std::vector<EGLint> g_last_context_attributes;
static std::vector<EGLint> g_last_window_surface_attributes;

#ifndef EGL_PROTECTED_CONTENT_EXT
#define EGL_PROTECTED_CONTENT_EXT 0x32C0
#endif

// Hook eglQueryString to simulate EGL_EXT_protected_content support.
extern "C" const char* eglQueryString(EGLDisplay dpy, EGLint name) {
  typedef const char* (*eglQueryString_t)(EGLDisplay, EGLint);
  static eglQueryString_t real_eglQueryString = nullptr;
  if (!real_eglQueryString) {
    real_eglQueryString = (eglQueryString_t)dlsym(RTLD_NEXT, "eglQueryString");
  }
  const char* result = real_eglQueryString(dpy, name);
  if (name == EGL_EXTENSIONS) {
    static std::string extensions;
    if (result) {
      extensions = result;
      extensions += " EGL_EXT_protected_content";
    } else {
      extensions = "EGL_EXT_protected_content";
    }
    return extensions.c_str();
  }
  return result;
}

// Hook eglCreateContext
extern "C" EGLContext eglCreateContext(EGLDisplay dpy,
                                       EGLConfig config,
                                       EGLContext share_context,
                                       const EGLint* attrib_list) {
  g_last_context_attributes.clear();
  std::vector<EGLint> stripped_attribs;
  if (attrib_list) {
    for (int i = 0; attrib_list[i] != EGL_NONE; i += 2) {
      g_last_context_attributes.push_back(attrib_list[i]);
      g_last_context_attributes.push_back(attrib_list[i + 1]);
      if (attrib_list[i] != EGL_PROTECTED_CONTENT_EXT) {
        stripped_attribs.push_back(attrib_list[i]);
        stripped_attribs.push_back(attrib_list[i + 1]);
      }
    }
    g_last_context_attributes.push_back(EGL_NONE);
    stripped_attribs.push_back(EGL_NONE);
  }

  // Call real eglCreateContext
  typedef EGLContext (*eglCreateContext_t)(EGLDisplay, EGLConfig, EGLContext,
                                           const EGLint*);
  static eglCreateContext_t real_eglCreateContext = nullptr;
  if (!real_eglCreateContext) {
    real_eglCreateContext =
        (eglCreateContext_t)dlsym(RTLD_NEXT, "eglCreateContext");
  }
  return real_eglCreateContext(
      dpy, config, share_context,
      stripped_attribs.empty() ? nullptr : stripped_attribs.data());
}

// Hook eglCreateWindowSurface
extern "C" EGLSurface eglCreateWindowSurface(EGLDisplay dpy,
                                             EGLConfig config,
                                             EGLNativeWindowType win,
                                             const EGLint* attrib_list) {
  g_last_window_surface_attributes.clear();
  std::vector<EGLint> stripped_attribs;
  if (attrib_list) {
    for (int i = 0; attrib_list[i] != EGL_NONE; i += 2) {
      g_last_window_surface_attributes.push_back(attrib_list[i]);
      g_last_window_surface_attributes.push_back(attrib_list[i + 1]);
      if (attrib_list[i] != EGL_PROTECTED_CONTENT_EXT) {
        stripped_attribs.push_back(attrib_list[i]);
        stripped_attribs.push_back(attrib_list[i + 1]);
      }
    }
    g_last_window_surface_attributes.push_back(EGL_NONE);
    stripped_attribs.push_back(EGL_NONE);
  }

  typedef EGLSurface (*eglCreateWindowSurface_t)(
      EGLDisplay, EGLConfig, EGLNativeWindowType, const EGLint*);
  static eglCreateWindowSurface_t real_eglCreateWindowSurface = nullptr;
  if (!real_eglCreateWindowSurface) {
    real_eglCreateWindowSurface =
        (eglCreateWindowSurface_t)dlsym(RTLD_NEXT, "eglCreateWindowSurface");
  }
  return real_eglCreateWindowSurface(
      dpy, config, win,
      stripped_attribs.empty() ? nullptr : stripped_attribs.data());
}

namespace flutter {
namespace testing {
namespace android {
namespace {

TaskRunners MakeTaskRunners(const std::string& thread_label,
                            const ThreadHost& thread_host) {
  fml::MessageLoop::EnsureInitializedForCurrentThread();
  fml::RefPtr<fml::TaskRunner> platform_runner =
      fml::MessageLoop::GetCurrent().GetTaskRunner();

  return TaskRunners(thread_label, platform_runner,
                     thread_host.raster_thread->GetTaskRunner(),
                     thread_host.ui_thread->GetTaskRunner(),
                     thread_host.io_thread->GetTaskRunner());
}
}  // namespace

class TestImpellerContext : public impeller::Context {
 public:
  TestImpellerContext() : Context(impeller::Flags{}) {}

  ~TestImpellerContext() {}

  impeller::Context::BackendType GetBackendType() const override {
    return impeller::Context::BackendType::kOpenGLES;
  }

  std::string DescribeGpuModel() const override { return ""; }

  bool IsValid() const override { return true; }

  const std::shared_ptr<const impeller::Capabilities>& GetCapabilities()
      const override {
    FML_UNREACHABLE();
  }

  bool UpdateOffscreenLayerPixelFormat(impeller::PixelFormat format) override {
    FML_UNREACHABLE();
  }

  std::shared_ptr<impeller::Allocator> GetResourceAllocator() const override {
    FML_UNREACHABLE();
  }

  std::shared_ptr<impeller::ShaderLibrary> GetShaderLibrary() const override {
    FML_UNREACHABLE();
  }

  std::shared_ptr<impeller::SamplerLibrary> GetSamplerLibrary() const override {
    FML_UNREACHABLE();
  }

  std::shared_ptr<impeller::PipelineLibrary> GetPipelineLibrary()
      const override {
    FML_UNREACHABLE();
  }

  std::shared_ptr<impeller::CommandBuffer> CreateCommandBuffer()
      const override {
    FML_UNREACHABLE();
  }

  std::shared_ptr<impeller::CommandQueue> GetCommandQueue() const override {
    FML_UNREACHABLE();
  }

  // A stub returning false is allowed from implementations that are not
  // planned to be used in benchmarking situations.
  bool FinishQueue() override { return false; }

  void Shutdown() override { did_shutdown = true; }

  impeller::RuntimeStageBackend GetRuntimeStageBackend() const override {
    return impeller::RuntimeStageBackend::kVulkan;
  }

  bool did_shutdown = false;
};

class TestAndroidContext : public AndroidContext {
 public:
  TestAndroidContext(const std::shared_ptr<impeller::Context>& impeller_context,
                     AndroidRenderingAPI rendering_api)
      : AndroidContext(rendering_api), impeller_context_(impeller_context) {
    SetImpellerContext(impeller_context);
  }

 private:
  std::shared_ptr<impeller::Context> impeller_context_;
};

TEST(AndroidContextGl, Create) {
  GrMockOptions main_context_options;
  sk_sp<GrDirectContext> main_context =
      GrDirectContext::MakeMock(&main_context_options);
  auto environment = fml::MakeRefCounted<AndroidEnvironmentGL>();
  std::string thread_label =
      ::testing::UnitTest::GetInstance()->current_test_info()->name();

  ThreadHost thread_host(ThreadHost::ThreadHostConfig(
      thread_label, ThreadHost::Type::kUi | ThreadHost::Type::kRaster |
                        ThreadHost::Type::kIo));
  TaskRunners task_runners = MakeTaskRunners(thread_label, thread_host);
  auto context =
      std::make_unique<AndroidContextGLSkia>(environment, task_runners);
  context->SetMainSkiaContext(main_context);
  EXPECT_NE(context.get(), nullptr);
  context.reset();
  EXPECT_TRUE(main_context->abandoned());
}

TEST(AndroidContextGl, CreateImpeller) {
  auto impeller_context = std::make_shared<TestImpellerContext>();
  auto android_context = std::make_unique<TestAndroidContext>(
      impeller_context, AndroidRenderingAPI::kImpellerOpenGLES);
  EXPECT_FALSE(impeller_context->did_shutdown);

  android_context.reset();

  EXPECT_TRUE(impeller_context->did_shutdown);
}

TEST(AndroidContextGl, CreateSingleThread) {
  GrMockOptions main_context_options;
  sk_sp<GrDirectContext> main_context =
      GrDirectContext::MakeMock(&main_context_options);
  auto environment = fml::MakeRefCounted<AndroidEnvironmentGL>();
  std::string thread_label =
      ::testing::UnitTest::GetInstance()->current_test_info()->name();
  fml::MessageLoop::EnsureInitializedForCurrentThread();
  fml::RefPtr<fml::TaskRunner> platform_runner =
      fml::MessageLoop::GetCurrent().GetTaskRunner();
  TaskRunners task_runners =
      TaskRunners(thread_label, platform_runner, platform_runner,
                  platform_runner, platform_runner);
  auto context =
      std::make_unique<AndroidContextGLSkia>(environment, task_runners);
  context->SetMainSkiaContext(main_context);
  EXPECT_NE(context.get(), nullptr);
  context.reset();
  EXPECT_TRUE(main_context->abandoned());
}

TEST(AndroidSurfaceGL, CreateSnapshopSurfaceWhenOnscreenSurfaceIsNotNull) {
  GrMockOptions main_context_options;
  sk_sp<GrDirectContext> main_context =
      GrDirectContext::MakeMock(&main_context_options);
  auto environment = fml::MakeRefCounted<AndroidEnvironmentGL>();
  std::string thread_label =
      ::testing::UnitTest::GetInstance()->current_test_info()->name();
  ThreadHost thread_host(ThreadHost::ThreadHostConfig(
      thread_label, ThreadHost::Type::kUi | ThreadHost::Type::kRaster |
                        ThreadHost::Type::kIo));
  TaskRunners task_runners = MakeTaskRunners(thread_label, thread_host);
  auto android_context =
      std::make_shared<AndroidContextGLSkia>(environment, task_runners);
  auto android_surface =
      std::make_unique<AndroidSurfaceGLSkia>(android_context);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  android_surface->SetNativeWindow(window, nullptr);
  auto onscreen_surface = android_surface->GetOnscreenSurface();
  EXPECT_NE(onscreen_surface, nullptr);
  android_surface->CreateSnapshotSurface();
  EXPECT_EQ(onscreen_surface, android_surface->GetOnscreenSurface());
}

TEST(AndroidSurfaceGL, CreateSnapshopSurfaceWhenOnscreenSurfaceIsNull) {
  GrMockOptions main_context_options;
  sk_sp<GrDirectContext> main_context =
      GrDirectContext::MakeMock(&main_context_options);
  auto environment = fml::MakeRefCounted<AndroidEnvironmentGL>();
  std::string thread_label =
      ::testing::UnitTest::GetInstance()->current_test_info()->name();

  auto mask =
      ThreadHost::Type::kUi | ThreadHost::Type::kRaster | ThreadHost::Type::kIo;
  flutter::ThreadHost::ThreadHostConfig host_config(mask);

  ThreadHost thread_host(host_config);
  TaskRunners task_runners = MakeTaskRunners(thread_label, thread_host);
  auto android_context =
      std::make_shared<AndroidContextGLSkia>(environment, task_runners);
  auto android_surface =
      std::make_unique<AndroidSurfaceGLSkia>(android_context);
  EXPECT_EQ(android_surface->GetOnscreenSurface(), nullptr);
  android_surface->CreateSnapshotSurface();
  EXPECT_NE(android_surface->GetOnscreenSurface(), nullptr);
}

TEST(AndroidContextGl, EnsureMakeCurrentChecksCurrentContextStatus) {
  GrMockOptions main_context_options;
  sk_sp<GrDirectContext> main_context =
      GrDirectContext::MakeMock(&main_context_options);
  auto environment = fml::MakeRefCounted<AndroidEnvironmentGL>();
  std::string thread_label =
      ::testing::UnitTest::GetInstance()->current_test_info()->name();

  ThreadHost thread_host(ThreadHost::ThreadHostConfig(
      thread_label, ThreadHost::Type::kUi | ThreadHost::Type::kRaster |
                        ThreadHost::Type::kIo));
  TaskRunners task_runners = MakeTaskRunners(thread_label, thread_host);
  auto context =
      std::make_unique<AndroidContextGLSkia>(environment, task_runners);

  auto pbuffer_surface = context->CreatePbufferSurface();
  auto status = pbuffer_surface->MakeCurrent();
  EXPECT_EQ(AndroidEGLSurfaceMakeCurrentStatus::kSuccessMadeCurrent, status);

  // context already current, so status must reflect that.
  status = pbuffer_surface->MakeCurrent();
  EXPECT_EQ(AndroidEGLSurfaceMakeCurrentStatus::kSuccessAlreadyCurrent, status);
}

#ifndef EGL_PROTECTED_CONTENT_EXT
#define EGL_PROTECTED_CONTENT_EXT 0x32C0
#endif

TEST(AndroidContextGl, ProtectedContentNotSupportedByDefault) {
  GrMockOptions main_context_options;
  sk_sp<GrDirectContext> main_context =
      GrDirectContext::MakeMock(&main_context_options);
  auto environment = fml::MakeRefCounted<AndroidEnvironmentGL>();

  if (!environment->IsValid()) {
    GTEST_SKIP() << "EGL not available on this host";
  }

  std::string thread_label =
      ::testing::UnitTest::GetInstance()->current_test_info()->name();
  ThreadHost thread_host(ThreadHost::ThreadHostConfig(
      thread_label, ThreadHost::Type::kUi | ThreadHost::Type::kRaster |
                        ThreadHost::Type::kIo));
  TaskRunners task_runners = MakeTaskRunners(thread_label, thread_host);
  auto context =
      std::make_unique<AndroidContextGLSkia>(environment, task_runners);

  ASSERT_TRUE(context->IsValid());

  EGLDisplay display = context->GetEGLDisplay();
  EGLContext egl_context = context->GetEGLContext();

  // Query the context to see if it is protected.
  EGLint value = EGL_FALSE;
  EGLBoolean result =
      eglQueryContext(display, egl_context, EGL_PROTECTED_CONTENT_EXT, &value);
  // If the query succeeds, it should be EGL_FALSE because we didn't request it.
  // If it fails with EGL_BAD_ATTRIBUTE, it means the extension is not
  // supported, which also means it's not protected.
  if (result) {
    EXPECT_EQ(value, EGL_FALSE);
  } else {
    EGLint error = eglGetError();
    EXPECT_EQ(error, EGL_BAD_ATTRIBUTE);
  }

  // Now check the surface.
  auto pbuffer_surface = context->CreatePbufferSurface();
  ASSERT_NE(pbuffer_surface, nullptr);
  ASSERT_TRUE(pbuffer_surface->IsValid());

  EGLSurface egl_surface = pbuffer_surface->GetHandle();
  value = EGL_FALSE;
  result =
      eglQuerySurface(display, egl_surface, EGL_PROTECTED_CONTENT_EXT, &value);
  if (result) {
    EXPECT_EQ(value, EGL_FALSE);
  } else {
    EGLint error = eglGetError();
    EXPECT_EQ(error, EGL_BAD_ATTRIBUTE);
  }
}

TEST(AndroidContextGl, ProtectedContextRequestedButNotSupported) {
  GrMockOptions main_context_options;
  sk_sp<GrDirectContext> main_context =
      GrDirectContext::MakeMock(&main_context_options);
  auto environment = fml::MakeRefCounted<AndroidEnvironmentGL>();

  if (!environment->IsValid()) {
    GTEST_SKIP() << "EGL not available on this host";
  }

  std::string thread_label =
      ::testing::UnitTest::GetInstance()->current_test_info()->name();
  ThreadHost thread_host(ThreadHost::ThreadHostConfig(
      thread_label, ThreadHost::Type::kUi | ThreadHost::Type::kRaster |
                        ThreadHost::Type::kIo));
  TaskRunners task_runners = MakeTaskRunners(thread_label, thread_host);

  // Clear hooks.
  g_last_context_attributes.clear();
  g_last_window_surface_attributes.clear();

  // Request protected context.
  auto context = std::make_unique<AndroidContextGLSkia>(
      environment, task_runners, /*use_protected_context=*/true);

  ASSERT_TRUE(context->IsValid());

  // Verify that EGL_PROTECTED_CONTENT_EXT was passed to eglCreateContext.
  bool found_protected_context = false;
  for (size_t i = 0; i + 1 < g_last_context_attributes.size(); i += 2) {
    if (g_last_context_attributes[i] == EGL_PROTECTED_CONTENT_EXT) {
      if (g_last_context_attributes[i + 1] == EGL_TRUE) {
        found_protected_context = true;
      }
    }
  }
  EXPECT_TRUE(found_protected_context)
      << "EGL_PROTECTED_CONTENT_EXT was not passed to eglCreateContext";

  // Also verify for the surface.
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/false);
  auto onscreen_surface = context->CreateOnscreenSurface(window);

  bool found_protected_surface = false;
  for (size_t i = 0; i + 1 < g_last_window_surface_attributes.size(); i += 2) {
    if (g_last_window_surface_attributes[i] == EGL_PROTECTED_CONTENT_EXT) {
      if (g_last_window_surface_attributes[i + 1] == EGL_TRUE) {
        found_protected_surface = true;
      }
    }
  }
  EXPECT_TRUE(found_protected_surface)
      << "EGL_PROTECTED_CONTENT_EXT was not passed to eglCreateWindowSurface";
}
}  // namespace android
}  // namespace testing
}  // namespace flutter
