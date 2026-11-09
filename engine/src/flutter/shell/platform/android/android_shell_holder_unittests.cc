// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include <android/api-level.h>
#include <android/native_window.h>
#include <dlfcn.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

#include "flutter/fml/message_loop.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/shell/common/rasterizer.h"
#include "flutter/shell/common/shell.h"
#include "flutter/shell/common/thread_host.h"
#include "flutter/shell/platform/android/android_context_dynamic_impeller.h"

#include "flutter/shell/platform/android/android_shell_holder.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "shell/platform/android/jni/platform_view_android_jni.h"

namespace flutter {
namespace testing {
namespace {
class MockPlatformViewAndroidJNI : public PlatformViewAndroidJNI {
 public:
  MOCK_METHOD(void,
              FlutterViewHandlePlatformMessage,
              (std::unique_ptr<flutter::PlatformMessage> message,
               int responseId),
              (override));
  MOCK_METHOD(void,
              FlutterViewHandlePlatformMessageResponse,
              (int responseId, std::unique_ptr<fml::Mapping> data),
              (override));
  MOCK_METHOD(void,
              FlutterViewUpdateSemantics,
              (std::vector<uint8_t> buffer,
               std::vector<std::string> strings,
               std::vector<std::vector<uint8_t>> string_attribute_args),
              (override));
  MOCK_METHOD(void,
              FlutterViewSetSemanticsTreeEnabled,
              (bool enabled),
              (override));
  MOCK_METHOD(void,
              FlutterViewSetApplicationLocale,
              (const std::string locale),
              (override));
  MOCK_METHOD(void,
              FlutterViewUpdateCustomAccessibilityActions,
              (std::vector<uint8_t> actions_buffer,
               std::vector<std::string> strings),
              (override));
  MOCK_METHOD(void, FlutterViewOnFirstFrame, (), (override));
  MOCK_METHOD(void, FlutterViewOnPreEngineRestart, (), (override));
  MOCK_METHOD(void,
              SurfaceTextureAttachToGLContext,
              (JavaLocalRef surface_texture, int textureId),
              (override));
  MOCK_METHOD(bool,
              SurfaceTextureShouldUpdate,
              (JavaLocalRef surface_texture),
              (override));
  MOCK_METHOD(void,
              SurfaceTextureUpdateTexImage,
              (JavaLocalRef surface_texture),
              (override));
  MOCK_METHOD(SkM44,
              SurfaceTextureGetTransformMatrix,
              (JavaLocalRef surface_texture),
              (override));
  MOCK_METHOD(void,
              SurfaceTextureDetachFromGLContext,
              (JavaLocalRef surface_texture),
              (override));
  MOCK_METHOD(JavaLocalRef,
              ImageProducerTextureEntryAcquireLatestImage,
              (JavaLocalRef image_texture_entry),
              (override));
  MOCK_METHOD(JavaLocalRef,
              ImageGetHardwareBuffer,
              (JavaLocalRef image),
              (override));
  MOCK_METHOD(void, ImageClose, (JavaLocalRef image), (override));
  MOCK_METHOD(void,
              HardwareBufferClose,
              (JavaLocalRef hardware_buffer),
              (override));
  MOCK_METHOD(void,
              FlutterViewOnDisplayPlatformView,
              (int view_id,
               int x,
               int y,
               int width,
               int height,
               int viewWidth,
               int viewHeight,
               MutatorsStack mutators_stack),
              (override));
  MOCK_METHOD(void,
              FlutterViewDisplayOverlaySurface,
              (int surface_id, int x, int y, int width, int height),
              (override));
  MOCK_METHOD(void, FlutterViewBeginFrame, (), (override));
  MOCK_METHOD(void, FlutterViewEndFrame, (), (override));
  MOCK_METHOD(std::unique_ptr<PlatformViewAndroidJNI::OverlayMetadata>,
              FlutterViewCreateOverlaySurface,
              (),
              (override));
  MOCK_METHOD(void, FlutterViewDestroyOverlaySurfaces, (), (override));
  MOCK_METHOD(ASurfaceTransaction*,
              createTransaction,
              (std::function<void()>&),
              (override));
  MOCK_METHOD(void, sealFrameTransactions, (), (override));
  MOCK_METHOD(void, swapTransaction, (), (override));
  MOCK_METHOD(void, destroyOverlaySurface2, (), (override));
  MOCK_METHOD(std::unique_ptr<PlatformViewAndroidJNI::OverlayMetadata>,
              createOverlaySurface2,
              (),
              (override));
  MOCK_METHOD(void,
              onDisplayPlatformView2,
              (int32_t view_id,
               int32_t x,
               int32_t y,
               int32_t width,
               int32_t height,
               int32_t viewWidth,
               int32_t viewHeight,
               MutatorsStack mutators_stack),
              (override));
  MOCK_METHOD(void, hidePlatformView2, (int32_t view_id), (override));
  MOCK_METHOD(void, onEndFrame2, (), (override));
  MOCK_METHOD(void, showOverlaySurface2, (), (override));
  MOCK_METHOD(void, hideOverlaySurface2, (), (override));
  MOCK_METHOD(std::unique_ptr<std::vector<std::string>>,
              FlutterViewComputePlatformResolvedLocale,
              (std::vector<std::string> supported_locales_data),
              (override));
  MOCK_METHOD(double, GetDisplayRefreshRate, (), (override));
  MOCK_METHOD(double, GetDisplayWidth, (), (override));
  MOCK_METHOD(double, GetDisplayHeight, (), (override));
  MOCK_METHOD(double, GetDisplayDensity, (), (override));
  MOCK_METHOD(bool,
              RequestDartDeferredLibrary,
              (int loading_unit_id),
              (override));
  MOCK_METHOD(double,
              FlutterViewGetScaledFontSize,
              (double font_size, int configuration_id),
              (const, override));
  MOCK_METHOD(void,
              MaybeResizeSurfaceView,
              (int32_t width, int32_t height),
              (const, override));
};

class MockPlatformMessageResponse : public PlatformMessageResponse {
 public:
  static fml::RefPtr<MockPlatformMessageResponse> Create() {
    return fml::AdoptRef(new MockPlatformMessageResponse());
  }
  MOCK_METHOD(void, Complete, (std::unique_ptr<fml::Mapping> data), (override));
  MOCK_METHOD(void, CompleteEmpty, (), (override));
};
}  // namespace

TEST(AndroidShellHolder, Create) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  EXPECT_NE(holder.get(), nullptr);
  EXPECT_TRUE(holder->IsValid());
  EXPECT_NE(holder->GetPlatformView().get(), nullptr);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->GetPlatformView()->NotifyCreated(window);
}

TEST(AndroidShellHolder, HandlePlatformMessage) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  EXPECT_NE(holder.get(), nullptr);
  EXPECT_TRUE(holder->IsValid());
  EXPECT_NE(holder->GetPlatformView().get(), nullptr);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->GetPlatformView()->NotifyCreated(window);
  EXPECT_TRUE(holder->GetPlatformMessageHandler());
  size_t data_size = 4;
  fml::MallocMapping bytes =
      fml::MallocMapping(static_cast<uint8_t*>(malloc(data_size)), data_size);
  fml::RefPtr<MockPlatformMessageResponse> response =
      MockPlatformMessageResponse::Create();
  auto message = std::make_unique<PlatformMessage>(
      /*channel=*/"foo", /*data=*/std::move(bytes), /*response=*/response);
  int response_id = 1;
  EXPECT_CALL(*jni,
              FlutterViewHandlePlatformMessage(::testing::_, response_id));
  EXPECT_CALL(*response, CompleteEmpty());
  holder->GetPlatformMessageHandler()->HandlePlatformMessage(
      std::move(message));
  holder->GetPlatformMessageHandler()
      ->InvokePlatformMessageEmptyResponseCallback(response_id);
}

TEST(AndroidShellHolder, CreateWithMergedPlatformAndUIThread) {
  Settings settings;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->GetPlatformView()->NotifyCreated(window);

  EXPECT_EQ(
      holder->GetShellForTesting()->GetTaskRunners().GetUITaskRunner(),
      holder->GetShellForTesting()->GetTaskRunners().GetPlatformTaskRunner());
}

TEST(AndroidShellHolder, CreateWithUnMergedPlatformAndUIThread) {
  Settings settings;
  settings.merged_platform_ui_thread =
      Settings::MergedPlatformUIThread::kDisabled;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->GetPlatformView()->NotifyCreated(window);

  EXPECT_NE(
      holder->GetShellForTesting()->GetTaskRunners().GetUITaskRunner(),
      holder->GetShellForTesting()->GetTaskRunners().GetPlatformTaskRunner());
}

TEST(AndroidShellHolder, NotifyCreatedAndChangedDoNotBlockPlatformThread) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_NE(holder, nullptr);
  ASSERT_TRUE(holder->IsValid());

  auto raster_runner =
      holder->GetShellForTesting()->GetTaskRunners().GetRasterTaskRunner();
  fml::AutoResetWaitableEvent raster_entered;
  fml::AutoResetWaitableEvent unblock_raster;
  raster_runner->PostTask([&raster_entered, &unblock_raster]() {
    raster_entered.Signal();
    unblock_raster.Wait();
  });
  raster_entered.Wait();

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  // Both NotifyCreated and NotifyChanged must return without blocking on the
  // stalled raster thread.
  holder->GetPlatformView()->NotifyCreated(window);
  holder->GetPlatformView()->NotifyChanged(DlISize(1080, 1920));

  unblock_raster.Signal();

  fml::AutoResetWaitableEvent raster_done;
  raster_runner->PostTask([&raster_done]() { raster_done.Signal(); });
  raster_done.Wait();
}

TEST(AndroidShellHolder, NotifyCreatedThenNotifyDestroyedWhileRasterBlocked) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_NE(holder, nullptr);
  ASSERT_TRUE(holder->IsValid());

  auto raster_runner =
      holder->GetShellForTesting()->GetTaskRunners().GetRasterTaskRunner();
  fml::AutoResetWaitableEvent raster_entered;
  fml::AutoResetWaitableEvent unblock_raster;
  raster_runner->PostTask([&raster_entered, &unblock_raster]() {
    raster_entered.Signal();
    unblock_raster.Wait();
  });
  raster_entered.Wait();

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->GetPlatformView()->NotifyCreated(window);
  holder->GetPlatformView()->NotifyChanged(DlISize(1080, 1920));

  std::thread unblocker([&unblock_raster]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    unblock_raster.Signal();
  });

  holder->GetPlatformView()->NotifyDestroyed();
  unblocker.join();

  fml::AutoResetWaitableEvent check_done;
  bool is_torn_down = false;
  raster_runner->PostTask([shell = holder->GetShellForTesting().get(),
                           &is_torn_down, &check_done]() {
    is_torn_down = shell->GetRasterizer()->IsTornDown();
    check_done.Signal();
  });
  check_done.Wait();
  EXPECT_TRUE(is_torn_down);
}

class ScopedTestImageReaderWindow {
 public:
  ScopedTestImageReaderWindow() {
    media_lib_ = dlopen("libmediandk.so", RTLD_NOW);
    if (!media_lib_) {
      return;
    }
    using NewWithUsageFn =
        int (*)(int32_t, int32_t, int32_t, uint64_t, int32_t, void**);
    using NewFn = int (*)(int32_t, int32_t, int32_t, int32_t, void**);
    using GetWindowFn = int (*)(void*, ANativeWindow**);
    delete_fn_ = reinterpret_cast<void (*)(void*)>(
        dlsym(media_lib_, "AImageReader_delete"));
    auto get_window = reinterpret_cast<GetWindowFn>(
        dlsym(media_lib_, "AImageReader_getWindow"));
    if (!delete_fn_ || !get_window) {
      return;
    }
    constexpr int32_t kRgba8888 = 1;
    // AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
    // AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER
    constexpr uint64_t kGpuUsage = (1ULL << 8) | (1ULL << 9);
    int status = -1;
    if (auto new_with_usage = reinterpret_cast<NewWithUsageFn>(
            dlsym(media_lib_, "AImageReader_newWithUsage"))) {
      status = new_with_usage(64, 64, kRgba8888, kGpuUsage, 3, &reader_);
    }
    if (status != 0 || !reader_) {
      if (auto new_fn =
              reinterpret_cast<NewFn>(dlsym(media_lib_, "AImageReader_new"))) {
        status = new_fn(64, 64, kRgba8888, 3, &reader_);
      }
    }
    if (status == 0 && reader_) {
      ANativeWindow* raw_window = nullptr;
      if (get_window(reader_, &raw_window) == 0 && raw_window != nullptr) {
        ANativeWindow_acquire(raw_window);
        window_ = fml::MakeRefCounted<AndroidNativeWindow>(raw_window);
      }
    }
  }

  ~ScopedTestImageReaderWindow() {
    window_ = nullptr;
    if (reader_ && delete_fn_) {
      delete_fn_(reader_);
    }
    if (media_lib_) {
      dlclose(media_lib_);
    }
  }

  fml::RefPtr<AndroidNativeWindow> window() const { return window_; }

 private:
  void* media_lib_ = nullptr;
  void* reader_ = nullptr;
  void (*delete_fn_)(void*) = nullptr;
  fml::RefPtr<AndroidNativeWindow> window_;
};

class BlockingAndroidContextDynamicImpeller
    : public AndroidContextDynamicImpeller {
 public:
  BlockingAndroidContextDynamicImpeller(
      const AndroidContext::ContextSettings& settings,
      std::shared_ptr<fml::BasicTaskRunner> io_task_runner,
      fml::AutoResetWaitableEvent& setup_entered,
      fml::AutoResetWaitableEvent& unblock_setup)
      : AndroidContextDynamicImpeller(settings, std::move(io_task_runner)),
        setup_entered_(setup_entered),
        unblock_setup_(unblock_setup) {}

  void SetupImpellerContext() override {
    setup_entered_.Signal();
    unblock_setup_.Wait();
    AndroidContextDynamicImpeller::SetupImpellerContext();
  }

 private:
  fml::AutoResetWaitableEvent& setup_entered_;
  fml::AutoResetWaitableEvent& unblock_setup_;
};

TEST(AndroidShellHolder,
     AutoselectHcppShellCreateAndNotifyCreatedDoNotWaitForImpellerSetup) {
  Settings settings;
  settings.enable_software_rendering = false;
  settings.enable_impeller = true;
  settings.enable_surface_control = true;

  fml::MessageLoop::EnsureInitializedForCurrentThread();
  ThreadHost thread_host("io.flutter.test.hcpp", ThreadHost::Type::kRaster |
                                                     ThreadHost::Type::kIo |
                                                     ThreadHost::Type::kUi);
  TaskRunners task_runners("io.flutter.test.hcpp",
                           fml::MessageLoop::GetCurrent().GetTaskRunner(),
                           thread_host.raster_thread->GetTaskRunner(),
                           thread_host.ui_thread->GetTaskRunner(),
                           thread_host.io_thread->GetTaskRunner());

  fml::AutoResetWaitableEvent setup_entered;
  fml::AutoResetWaitableEvent unblock_setup;

  fml::AutoResetWaitableEvent cancel_watchdog;
  std::atomic<bool> watchdog_fired{false};
  std::thread watchdog([&]() {
    if (cancel_watchdog.WaitWithTimeout(fml::TimeDelta::FromSeconds(5))) {
      watchdog_fired.store(true);
      unblock_setup.Signal();
    }
  });

  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  fml::WeakPtr<PlatformViewAndroid> weak_platform_view;
  Shell::CreateCallback<PlatformView> on_create_platform_view =
      [&jni, &weak_platform_view, &setup_entered,
       &unblock_setup](Shell& shell) {
        AndroidContext::ContextSettings ctx_settings;
        ctx_settings.enable_surface_control = true;
        auto dynamic_context =
            std::make_shared<BlockingAndroidContextDynamicImpeller>(
                ctx_settings,
                shell.GetIsGpuDisabledSyncSwitch()
                    ? static_cast<PlatformView::Delegate&>(shell)
                          .OnPlatformViewGetShutdownSafeIOTaskRunner()
                    : nullptr,
                setup_entered, unblock_setup);
        auto view = std::make_unique<PlatformViewAndroid>(
            shell, shell.GetTaskRunners(), jni, dynamic_context);
        weak_platform_view = view->GetWeakPtr();
        return view;
      };
  Shell::CreateCallback<Rasterizer> on_create_rasterizer = [](Shell& shell) {
    return std::make_unique<Rasterizer>(shell);
  };

  // On API 34+ HCPP-eligible devices, AndroidExternalViewEmbedderWrapper
  // queries AndroidContextDynamicImpeller::RenderingApi() -> WaitForSetup()
  // inside SupportsDynamicThreadMerging(). Neither Shell::Create nor
  // NotifyCreated/NotifyChanged may trigger that wait on the platform thread.
  std::unique_ptr<Shell> shell =
      Shell::Create(PlatformData{}, task_runners, settings,
                    on_create_platform_view, on_create_rasterizer);
  ASSERT_NE(shell, nullptr);
  ASSERT_TRUE(weak_platform_view);
  setup_entered.Wait();
  EXPECT_FALSE(watchdog_fired.load());
  EXPECT_EQ(weak_platform_view->GetAndroidContext()->GetImpellerContext(),
            nullptr);

  ScopedTestImageReaderWindow reader_window;
  auto window = reader_window.window()
                    ? reader_window.window()
                    : fml::MakeRefCounted<AndroidNativeWindow>(
                          nullptr, /*is_fake_window=*/true);
  weak_platform_view->NotifyCreated(window);
  weak_platform_view->NotifyChanged(DlISize(64, 64));
  EXPECT_FALSE(watchdog_fired.load());
  EXPECT_EQ(weak_platform_view->GetAndroidContext()->GetImpellerContext(),
            nullptr);

  cancel_watchdog.Signal();
  watchdog.join();

  unblock_setup.Signal();

  fml::AutoResetWaitableEvent raster_drained;
  bool has_merger = false;
  task_runners.GetRasterTaskRunner()->PostTask(
      [shell_ptr = shell.get(), &has_merger, &raster_drained]() {
        has_merger =
            shell_ptr->GetRasterizer()->GetRasterThreadMerger().get() !=
            nullptr;
        raster_drained.Signal();
      });
  ASSERT_FALSE(raster_drained.WaitWithTimeout(fml::TimeDelta::FromSeconds(10)));
  EXPECT_NE(weak_platform_view->GetAndroidContext()->GetImpellerContext(),
            nullptr);
  if (reader_window.window() && android_get_device_api_level() >= 34) {
    // On API 34+ with HCPP enabled and a real ANativeWindow, Rasterizer::Setup
    // runs on the raster thread after SetupImpellerContext and creates the
    // dynamic thread merger only if HCPP is NOT supported (Vulkan HCPP uses
    // SurfaceControl without merging threads, whereas GLES merges threads).
    if (weak_platform_view->GetAndroidContext()->RenderingApi() !=
        AndroidRenderingAPI::kImpellerVulkan) {
      EXPECT_TRUE(has_merger);
    }
  }

  weak_platform_view->NotifyDestroyed();
  shell.reset();
}

TEST(AndroidShellHolder,
     NotifyCreatedWithInvalidWindowFollowedByNotifyChangedDoesNotCrash) {
  Settings settings;
  settings.enable_software_rendering = false;
  settings.enable_impeller = true;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerAutoselect);
  ASSERT_NE(holder, nullptr);
  ASSERT_TRUE(holder->IsValid());

  auto invalid_window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->GetPlatformView()->NotifyCreated(invalid_window);
  holder->GetPlatformView()->NotifyChanged(DlISize(1080, 1920));

  auto raster_runner =
      holder->GetShellForTesting()->GetTaskRunners().GetRasterTaskRunner();
  fml::AutoResetWaitableEvent raster_done;
  raster_runner->PostTask([&raster_done]() { raster_done.Signal(); });
  ASSERT_FALSE(raster_done.WaitWithTimeout(fml::TimeDelta::FromSeconds(10)));
}

TEST(AndroidShellHolder,
     OpenGLESNotifyCreatedWithFailedWindowStillSetsUpRasterizer) {
  Settings settings;
  settings.enable_software_rendering = false;
  settings.enable_impeller = true;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_NE(holder, nullptr);
  ASSERT_TRUE(holder->IsValid());

  // A non-fake window with a null native handle makes
  // `AndroidSurfaceGLImpeller::SetNativeWindow()` fail
  // (`eglCreateWindowSurface` fails), while `CreateGPUSurface()` still succeeds
  // so `Rasterizer::Setup()` runs and subsequent `NotifyChanged()` can attempt
  // recovery via `OnScreenSurfaceResize()`.
  auto failed_window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/false);
  holder->GetPlatformView()->NotifyCreated(failed_window);
  holder->GetPlatformView()->NotifyChanged(DlISize(640, 480));

  auto* shell = holder->GetShellForTesting().get();
  auto raster_runner = shell->GetTaskRunners().GetRasterTaskRunner();
  fml::AutoResetWaitableEvent raster_done;
  bool rasterizer_set_up_with_merger = false;
  raster_runner->PostTask(
      [shell, &rasterizer_set_up_with_merger, &raster_done]() {
        rasterizer_set_up_with_merger =
            shell->GetRasterizer()->GetRasterThreadMerger().get() != nullptr;
        raster_done.Signal();
      });
  ASSERT_FALSE(raster_done.WaitWithTimeout(fml::TimeDelta::FromSeconds(10)));
  EXPECT_TRUE(rasterizer_set_up_with_merger);

  holder->GetPlatformView()->NotifyDestroyed();
}

TEST(AndroidShellHolder, NotifyCreatedDoesNotBlockPlatformThread) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  fml::AutoResetWaitableEvent raster_task_started;
  fml::AutoResetWaitableEvent raster_task_finish;

  holder->GetShellForTesting()
      ->GetTaskRunners()
      .GetRasterTaskRunner()
      ->PostTask([&]() {
        raster_task_started.Signal();
        raster_task_finish.Wait();
      });

  // Wait for the raster thread to start and block.
  raster_task_started.Wait();

  fml::AutoResetWaitableEvent notify_started;
  fml::AutoResetWaitableEvent notify_finished;
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);

  std::thread notify_thread([&]() {
    notify_started.Signal();
    holder->GetPlatformView()->NotifyCreated(window);
    notify_finished.Signal();
  });

  notify_started.Wait();

  bool did_not_block =
      notify_finished.WaitWithTimeout(fml::TimeDelta::FromMilliseconds(100));

  // Clean up.
  raster_task_finish.Signal();
  notify_thread.join();

  EXPECT_TRUE(did_not_block);
}

TEST(AndroidShellHolder, NotifyChangedDoesNotBlockPlatformThread) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  fml::AutoResetWaitableEvent raster_task_started;
  fml::AutoResetWaitableEvent raster_task_finish;

  holder->GetShellForTesting()
      ->GetTaskRunners()
      .GetRasterTaskRunner()
      ->PostTask([&]() {
        raster_task_started.Signal();
        raster_task_finish.Wait();
      });

  // Wait for the raster thread to start and block.
  raster_task_started.Wait();

  fml::AutoResetWaitableEvent notify_started;
  fml::AutoResetWaitableEvent notify_finished;

  std::thread notify_thread([&]() {
    notify_started.Signal();
    holder->GetPlatformView()->NotifyChanged(flutter::DlISize(100, 100));
    notify_finished.Signal();
  });

  notify_started.Wait();

  bool did_not_block =
      notify_finished.WaitWithTimeout(fml::TimeDelta::FromMilliseconds(100));

  // Clean up.
  raster_task_finish.Signal();
  notify_thread.join();

  EXPECT_TRUE(did_not_block);
}
}  // namespace flutter
}  // namespace testing
}  // namespace flutter
