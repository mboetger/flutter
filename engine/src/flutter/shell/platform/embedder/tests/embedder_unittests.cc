// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include <string>
#include <utility>
#include <vector>

#include "embedder.h"
#include "embedder_engine.h"
#include "flutter/common/constants.h"
#include "flutter/flow/raster_cache.h"
#include "flutter/fml/file.h"
#include "flutter/fml/make_copyable.h"
#include "flutter/fml/mapping.h"
#include "flutter/fml/message_loop.h"
#include "flutter/fml/paths.h"
#include "flutter/fml/synchronization/count_down_latch.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/fml/task_runner.h"
#include "flutter/fml/thread.h"
#include "flutter/fml/time/time_delta.h"
#include "flutter/fml/time/time_point.h"
#include "flutter/lib/ui/plugins/callback_cache.h"
#include "flutter/runtime/dart_vm.h"
#include "flutter/shell/platform/embedder/embedder_struct_macros.h"
#include "flutter/shell/platform/embedder/tests/embedder_assertions.h"
#include "flutter/shell/platform/embedder/tests/embedder_config_builder.h"
#include "flutter/shell/platform/embedder/tests/embedder_test.h"
#include "flutter/shell/platform/embedder/tests/embedder_test_backingstore_producer_software.h"
#include "flutter/shell/platform/embedder/tests/embedder_unittests_util.h"
#include "flutter/testing/assertions_skia.h"
#include "flutter/testing/testing.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/tonic/converter/dart_converter.h"

#if defined(FML_OS_MACOSX)
#include <pthread.h>
#endif

// CREATE_FFI_LAMBDA is leaky by design
// NOLINTBEGIN(clang-analyzer-core.StackAddressEscape)

namespace {

static uint64_t NanosFromEpoch(int millis_from_now) {
  const auto now = fml::TimePoint::Now();
  const auto delta = fml::TimeDelta::FromMilliseconds(millis_from_now);
  return (now + delta).ToEpochDelta().ToNanoseconds();
}

}  // namespace

namespace flutter {
namespace testing {

using EmbedderTest = testing::EmbedderTest;

TEST(EmbedderTestNoFixture, MustNotRunWithInvalidArgs) {
  EmbedderTestContextSoftware context;
  EmbedderConfigBuilder builder(
      context, EmbedderConfigBuilder::InitializationPreference::kNoInitialize);
  auto engine = builder.LaunchEngine();
  ASSERT_FALSE(engine.is_valid());
}

TEST(EmbedderTestNoFixture, RendererConfigSetupCallbackFields) {
  FlutterOpenGLRendererConfig gl_config = {};
  gl_config.struct_size = sizeof(gl_config);
  gl_config.setup_callback = [](void* user_data) -> bool { return true; };
  ASSERT_NE(gl_config.setup_callback, nullptr);

  FlutterVulkanRendererConfig vk_config = {};
  vk_config.struct_size = sizeof(vk_config);
  vk_config.setup_callback = [](void* user_data) -> bool { return true; };
  ASSERT_NE(vk_config.setup_callback, nullptr);

  FlutterMetalRendererConfig metal_config = {};
  metal_config.struct_size = sizeof(metal_config);
  metal_config.setup_callback = [](void* user_data) -> bool { return true; };
  ASSERT_NE(metal_config.setup_callback, nullptr);
}

TEST(EmbedderTestNoFixture, RendererConfigVulkanExternalTextureFields) {
  FlutterVulkanExternalTexture texture = {};
  texture.struct_size = sizeof(texture);
  texture.width = 100;
  texture.height = 100;
  texture.image = 42;
  texture.format = 37;  // VK_FORMAT_R8G8B8A8_UNORM
  texture.user_data = reinterpret_cast<void*>(0x1234);
  texture.destruction_callback = [](void* user_data) {};

  EXPECT_EQ(texture.width, 100u);
  EXPECT_EQ(texture.height, 100u);
  EXPECT_EQ(texture.image, 42u);
  EXPECT_EQ(texture.format, 37u);
  EXPECT_EQ(texture.user_data, reinterpret_cast<void*>(0x1234));
  EXPECT_NE(texture.destruction_callback, nullptr);

  FlutterVulkanRendererConfig vk_config = {};
  vk_config.struct_size = sizeof(vk_config);
  vk_config.vulkan_external_texture_frame_callback =
      [](void* user_data, int64_t texture_identifier, size_t width,
         size_t height, FlutterVulkanExternalTexture* out) -> bool {
    out->struct_size = sizeof(FlutterVulkanExternalTexture);
    out->width = width;
    out->height = height;
    out->image = 1;
    out->format = 37;
    return true;
  };
  ASSERT_NE(vk_config.vulkan_external_texture_frame_callback, nullptr);
}

TEST(EmbedderTestNoFixture, RendererConfigOpenGLTextureTransformationFields) {
  FlutterOpenGLTexture texture = {};
  for (int i = 0; i < 16; ++i) {
    EXPECT_DOUBLE_EQ(texture.transformation[i], 0.0);
  }

  // Populate column-major 4x4 UV transformation matrix (e.g. 90-degree
  // rotation). Column 0: (0, 1, 0, 0) Column 1: (-1, 0, 0, 0) Column 2: (0, 0,
  // 1, 0) Column 3: (1, 0, 0, 1)
  texture.transformation[0] = 0.0;
  texture.transformation[1] = 1.0;
  texture.transformation[4] = -1.0;
  texture.transformation[5] = 0.0;
  texture.transformation[10] = 1.0;
  texture.transformation[12] = 1.0;
  texture.transformation[15] = 1.0;

  EXPECT_DOUBLE_EQ(texture.transformation[1], 1.0);
  EXPECT_DOUBLE_EQ(texture.transformation[4], -1.0);
  EXPECT_DOUBLE_EQ(texture.transformation[10], 1.0);
  EXPECT_DOUBLE_EQ(texture.transformation[12], 1.0);
  EXPECT_DOUBLE_EQ(texture.transformation[15], 1.0);
}

TEST_F(EmbedderTest, CanLaunchAndShutdownWithValidProjectArgs) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  fml::AutoResetWaitableEvent latch;
  context.AddIsolateCreateCallback([&latch]() { latch.Signal(); });
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());
  // Wait for the root isolate to launch.
  latch.Wait();
  engine.reset();
}

TEST_F(EmbedderTest, SurfaceLifecycleAndGpuAvailability) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  fml::AutoResetWaitableEvent latch;
  context.AddIsolateCreateCallback([&latch]() { latch.Signal(); });
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());
  latch.Wait();

  // Test SetGpuAvailability across all valid states.
  ASSERT_EQ(FlutterEngineSetGpuAvailability(
                engine.get(), kFlutterGpuAvailabilityFlushAndMakeUnavailable),
            kSuccess);
  ASSERT_EQ(FlutterEngineSetGpuAvailability(engine.get(),
                                            kFlutterGpuAvailabilityUnavailable),
            kSuccess);
  ASSERT_EQ(FlutterEngineSetGpuAvailability(engine.get(),
                                            kFlutterGpuAvailabilityAvailable),
            kSuccess);

  // Test NotifySurfaceDestroyed and NotifySurfaceCreated.
  ASSERT_EQ(
      FlutterEngineNotifySurfaceDestroyed(engine.get(), kFlutterImplicitViewId),
      kSuccess);
  ASSERT_EQ(
      FlutterEngineNotifySurfaceCreated(engine.get(), kFlutterImplicitViewId),
      kSuccess);

  // Test invalid view ID rejection.
  ASSERT_NE(FlutterEngineNotifySurfaceDestroyed(engine.get(), /*invalid*/ 42),
            kSuccess);
  ASSERT_NE(FlutterEngineNotifySurfaceCreated(engine.get(), /*invalid*/ 42),
            kSuccess);

  engine.reset();
}

// TODO(41999): Disabled because flaky.
TEST_F(EmbedderTest, DISABLED_CanLaunchAndShutdownMultipleTimes) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  for (size_t i = 0; i < 3; ++i) {
    auto engine = builder.LaunchEngine();
    ASSERT_TRUE(engine.is_valid());
    FML_LOG(INFO) << "Engine launch count: " << i + 1;
  }
}

TEST_F(EmbedderTest, CanInvokeCustomEntrypoint) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  static fml::AutoResetWaitableEvent latch;
  auto entrypoint = []() { latch.Signal(); };
  context.AddFfiNativeCallback("SayHiFromCustomEntrypoint",
                               reinterpret_cast<void*>(+entrypoint));
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("customEntrypoint");
  auto engine = builder.LaunchEngine();
  latch.Wait();
  ASSERT_TRUE(engine.is_valid());
}

TEST_F(EmbedderTest, CanInvokeCustomEntrypointMacro) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  fml::AutoResetWaitableEvent latch1;
  fml::AutoResetWaitableEvent latch2;
  fml::AutoResetWaitableEvent latch3;

  // Can be defined separately.
  auto entry1 = [&latch1]() {
    FML_LOG(INFO) << "In Callback 1";
    latch1.Signal();
  };
  auto native_entry1 = CREATE_FFI_LAMBDA(entry1);
  context.AddFfiNativeCallback("SayHiFromCustomEntrypoint1", native_entry1);

  // Can be wrapped in the args.
  auto entry2 = [&latch2]() {
    FML_LOG(INFO) << "In Callback 2";
    latch2.Signal();
  };
  context.AddFfiNativeCallback("SayHiFromCustomEntrypoint2",
                               CREATE_FFI_LAMBDA(entry2));

  // Everything can be inline.
  context.AddFfiNativeCallback("SayHiFromCustomEntrypoint3",
                               CREATE_FFI_LAMBDA([&latch3]() {
                                 FML_LOG(INFO) << "In Callback 3";
                                 latch3.Signal();
                               }));

  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("customEntrypoint1");
  auto engine = builder.LaunchEngine();
  latch1.Wait();
  latch2.Wait();
  latch3.Wait();
  ASSERT_TRUE(engine.is_valid());
}

TEST_F(EmbedderTest, CanTerminateCleanly) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("terminateExitCodeHandler");
  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());
}

TEST_F(EmbedderTest, ExecutableNameNotNull) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  // Supply a callback to Dart for the test fixture to pass Platform.executable
  // back to us.
  fml::AutoResetWaitableEvent latch;
  context.AddFfiNativeCallback(
      "NotifyStringValue", CREATE_FFI_LAMBDA([&](Dart_Handle value) {
        const auto dart_string =
            tonic::DartConverter<std::string>::FromDart(value);
        EXPECT_EQ("/path/to/binary", dart_string);
        latch.Signal();
      }));

  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("executableNameNotNull");
  builder.SetExecutableName("/path/to/binary");
  auto engine = builder.LaunchEngine();
  latch.Wait();
}

TEST_F(EmbedderTest, ImplicitViewNotNull) {
  // TODO(loicsharma): Update this test when embedders can opt-out
  // of the implicit view.
  // See: https://github.com/flutter/flutter/issues/120306
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  bool implicitViewNotNull = false;
  fml::AutoResetWaitableEvent latch;
  context.AddFfiNativeCallback("NotifyBoolValue",
                               CREATE_FFI_LAMBDA([&](bool value) {
                                 implicitViewNotNull = value;
                                 latch.Signal();
                               }));

  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("implicitViewNotNull");
  auto engine = builder.LaunchEngine();
  latch.Wait();

  EXPECT_TRUE(implicitViewNotNull);
}

std::atomic_size_t EmbedderTestTaskRunner::sEmbedderTaskRunnerIdentifiers = {};

TEST_F(EmbedderTest, CanSpecifyCustomUITaskRunner) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  auto ui_thread = std::make_unique<fml::Thread>("test_ui_thread");
  auto ui_task_runner = ui_thread->GetTaskRunner();
  std::mutex ui_task_runner_mutex;
  bool ui_task_runner_destroyed = false;
  auto platform_thread = std::make_unique<fml::Thread>("test_platform_thread");
  auto platform_task_runner = platform_thread->GetTaskRunner();
  UniqueEngine engine;

  EmbedderTestTaskRunner test_ui_task_runner =
      EmbedderTestTaskRunnerBuilder()
          .SetRealTaskRunner(ui_task_runner)
          .SetTaskExpiryCallback([&](FlutterTask task) {
            // The UI task runner will be destroyed during engine shutdown.  It
            // should continue dispatching tasks until the engine invokes the
            // destruction callback.  After that it must stop using the engine.
            std::scoped_lock lock(ui_task_runner_mutex);
            if (ui_task_runner_destroyed) {
              return;
            }
            FlutterEngineRunTask(engine.get(), &task);
          })
          .SetDestructionCallback([&]() {
            std::scoped_lock lock(ui_task_runner_mutex);
            ui_task_runner_destroyed = true;
          })
          .Build();

  EmbedderTestTaskRunner test_platform_task_runner =
      EmbedderTestTaskRunnerBuilder()
          .SetRealTaskRunner(platform_task_runner)
          .SetTaskExpiryCallback([&](FlutterTask task) {
            if (!engine.is_valid()) {
              return;
            }
            FlutterEngineRunTask(engine.get(), &task);
          })
          .Build();

  fml::AutoResetWaitableEvent signal_latch_ui;
  fml::AutoResetWaitableEvent signal_latch_platform;

  context.AddFfiNativeCallback(
      "SignalNativeTest", CREATE_FFI_LAMBDA([&]() {
        // Assert that the UI isolate is running on platform thread.
        ASSERT_TRUE(ui_task_runner->RunsTasksOnCurrentThread());
        signal_latch_ui.Signal();
      }));

  platform_task_runner->PostTask([&]() {
    EmbedderConfigBuilder builder(context);
    const auto ui_task_runner_description =
        test_ui_task_runner.GetFlutterTaskRunnerDescription();
    const auto platform_task_runner_description =
        test_platform_task_runner.GetFlutterTaskRunnerDescription();
    builder.SetSurface(DlISize(1, 1));
    builder.SetUITaskRunner(&ui_task_runner_description);
    builder.SetPlatformTaskRunner(&platform_task_runner_description);
    builder.SetDartEntrypoint("canSpecifyCustomUITaskRunner");
    builder.SetPlatformMessageCallback(
        [&](const FlutterPlatformMessage* message) {
          ASSERT_TRUE(platform_task_runner->RunsTasksOnCurrentThread());
          signal_latch_platform.Signal();
        });
    engine = builder.InitializeEngine();
    ASSERT_EQ(FlutterEngineRunInitialized(engine.get()), kSuccess);
    ASSERT_TRUE(engine.is_valid());
  });
  signal_latch_ui.Wait();
  signal_latch_platform.Wait();

  fml::AutoResetWaitableEvent kill_latch;
  platform_task_runner->PostTask([&] {
    engine.reset();
    platform_task_runner->PostTask([&kill_latch] { kill_latch.Signal(); });
  });
  kill_latch.Wait();

  // Shut down the threads before exiting the test.  There may still be
  // pending tasks queued to the task runners, and they must not run
  // after the engine goes out of scope.
  ui_thread.reset();
  platform_thread.reset();
}

TEST_F(EmbedderTest, IgnoresStaleTasks) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  auto ui_task_runner = CreateNewThread("test_ui_thread");
  auto platform_task_runner = CreateNewThread("test_platform_thread");
  static std::mutex engine_mutex;
  UniqueEngine engine;
  FlutterEngine engine_ptr;

  EmbedderTestTaskRunner test_ui_task_runner(
      ui_task_runner, [&](FlutterTask task) {
        // The check for engine.is_valid() is intentionally absent here.
        // FlutterEngineRunTask must be able to detect and ignore stale tasks
        // without crashing even if the engine pointer is not null.
        // Because the engine is destroyed on platform thread,
        // relying solely on engine.is_valid() in UI thread is not safe.
        FlutterEngineRunTask(engine_ptr, &task);
      });
  EmbedderTestTaskRunner test_platform_task_runner(
      platform_task_runner, [&](FlutterTask task) {
        std::scoped_lock lock(engine_mutex);
        if (!engine.is_valid()) {
          return;
        }
        FlutterEngineRunTask(engine.get(), &task);
      });

  fml::AutoResetWaitableEvent init_latch;

  platform_task_runner->PostTask([&]() {
    EmbedderConfigBuilder builder(context);
    const auto ui_task_runner_description =
        test_ui_task_runner.GetFlutterTaskRunnerDescription();
    const auto platform_task_runner_description =
        test_platform_task_runner.GetFlutterTaskRunnerDescription();
    builder.SetUITaskRunner(&ui_task_runner_description);
    builder.SetPlatformTaskRunner(&platform_task_runner_description);
    {
      std::scoped_lock lock(engine_mutex);
      engine = builder.InitializeEngine();
    }
    init_latch.Signal();
  });

  init_latch.Wait();
  engine_ptr = engine.get();

  auto flutter_engine = reinterpret_cast<EmbedderEngine*>(engine.get());

  // Schedule task on UI thread that will likely run after the engine has shut
  // down.
  flutter_engine->GetTaskRunners().GetUITaskRunner()->PostDelayedTask(
      []() {}, fml::TimeDelta::FromMilliseconds(50));

  fml::AutoResetWaitableEvent kill_latch;
  platform_task_runner->PostTask([&] {
    engine.reset();
    platform_task_runner->PostTask([&kill_latch] { kill_latch.Signal(); });
  });
  kill_latch.Wait();

  // Ensure that the schedule task indeed runs.
  kill_latch.Reset();
  ui_task_runner->PostDelayedTask([&]() { kill_latch.Signal(); },
                                  fml::TimeDelta::FromMilliseconds(50));
  kill_latch.Wait();
}

TEST_F(EmbedderTest, MergedPlatformUIThread) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  auto task_runner = CreateNewThread("test_thread");
  UniqueEngine engine;

  EmbedderTestTaskRunner test_task_runner(task_runner, [&](FlutterTask task) {
    if (!engine.is_valid()) {
      return;
    }
    FlutterEngineRunTask(engine.get(), &task);
  });

  fml::AutoResetWaitableEvent signal_latch_ui;
  fml::AutoResetWaitableEvent signal_latch_platform;

  context.AddFfiNativeCallback(
      "SignalNativeTest", CREATE_FFI_LAMBDA([&]() {
        // Assert that the UI isolate is running on platform thread.
        ASSERT_TRUE(task_runner->RunsTasksOnCurrentThread());
        signal_latch_ui.Signal();
      }));

  task_runner->PostTask([&]() {
    EmbedderConfigBuilder builder(context);
    const auto task_runner_description =
        test_task_runner.GetFlutterTaskRunnerDescription();
    builder.SetSurface(DlISize(1, 1));
    builder.SetUITaskRunner(&task_runner_description);
    builder.SetPlatformTaskRunner(&task_runner_description);
    builder.SetDartEntrypoint("mergedPlatformUIThread");
    builder.SetPlatformMessageCallback(
        [&](const FlutterPlatformMessage* message) {
          ASSERT_TRUE(task_runner->RunsTasksOnCurrentThread());
          signal_latch_platform.Signal();
        });
    engine = builder.LaunchEngine();
    ASSERT_TRUE(engine.is_valid());
  });
  signal_latch_ui.Wait();
  signal_latch_platform.Wait();

  fml::AutoResetWaitableEvent kill_latch;
  task_runner->PostTask([&] {
    engine.reset();
    task_runner->PostTask([&kill_latch] { kill_latch.Signal(); });
  });
  kill_latch.Wait();
}

TEST_F(EmbedderTest, UITaskRunnerFlushesMicrotasks) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  auto ui_task_runner = CreateNewThread("test_ui_thread");
  UniqueEngine engine;

  EmbedderTestTaskRunner test_task_runner(
      // Assert that the UI isolate is running on platform thread.
      ui_task_runner, [&](FlutterTask task) {
        if (!engine.is_valid()) {
          return;
        }
        FlutterEngineRunTask(engine.get(), &task);
      });

  fml::AutoResetWaitableEvent signal_latch;

  context.AddFfiNativeCallback(
      "SignalNativeTest", CREATE_FFI_LAMBDA([&]() {
        ASSERT_TRUE(ui_task_runner->RunsTasksOnCurrentThread());
        signal_latch.Signal();
      }));

  ui_task_runner->PostTask([&]() {
    EmbedderConfigBuilder builder(context);
    const auto task_runner_description =
        test_task_runner.GetFlutterTaskRunnerDescription();
    builder.SetSurface(DlISize(1, 1));
    builder.SetUITaskRunner(&task_runner_description);
    builder.SetDartEntrypoint("uiTaskRunnerFlushesMicrotasks");
    engine = builder.LaunchEngine();
    ASSERT_TRUE(engine.is_valid());
  });
  signal_latch.Wait();

  fml::AutoResetWaitableEvent kill_latch;
  ui_task_runner->PostTask([&] {
    engine.reset();
    ui_task_runner->PostTask([&kill_latch] { kill_latch.Signal(); });
  });
  kill_latch.Wait();
}

TEST_F(EmbedderTest, CanSpecifyCustomPlatformTaskRunner) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  fml::AutoResetWaitableEvent latch;

  // Run the test on its own thread with a message loop so that it can safely
  // pump its event loop while we wait for all the conditions to be checked.
  auto platform_task_runner = CreateNewThread("test_platform_thread");
  static std::mutex engine_mutex;
  static bool signaled_once = false;
  std::atomic<bool> destruction_callback_called = false;
  UniqueEngine engine;

  EmbedderTestTaskRunner test_task_runner =
      EmbedderTestTaskRunnerBuilder()
          .SetRealTaskRunner(platform_task_runner)
          .SetTaskExpiryCallback([&](FlutterTask task) {
            std::scoped_lock lock(engine_mutex);
            if (!engine.is_valid()) {
              return;
            }
            // There may be multiple tasks posted but we only need to check
            // assertions once.
            if (signaled_once) {
              FlutterEngineRunTask(engine.get(), &task);
              return;
            }

            signaled_once = true;
            ASSERT_TRUE(engine.is_valid());
            ASSERT_EQ(FlutterEngineRunTask(engine.get(), &task), kSuccess);
            latch.Signal();
          })
          .SetDestructionCallback(
              [&]() { destruction_callback_called.store(true); })
          .Build();

  platform_task_runner->PostTask([&]() {
    EmbedderConfigBuilder builder(context);
    const auto task_runner_description =
        test_task_runner.GetFlutterTaskRunnerDescription();
    builder.SetSurface(DlISize(1, 1));
    builder.SetPlatformTaskRunner(&task_runner_description);
    builder.SetDartEntrypoint("invokePlatformTaskRunner");
    std::scoped_lock lock(engine_mutex);
    engine = builder.LaunchEngine();
    ASSERT_TRUE(engine.is_valid());
  });

  // Signaled when all the assertions are checked.
  latch.Wait();
  ASSERT_TRUE(engine.is_valid());

  // Since the engine was started on its own thread, it must be killed there as
  // well.
  fml::AutoResetWaitableEvent kill_latch;
  platform_task_runner->PostTask(fml::MakeCopyable([&]() mutable {
    std::scoped_lock lock(engine_mutex);
    engine.reset();

    // There may still be pending tasks on the platform thread that were queued
    // by the test_task_runner.  Signal the latch after these tasks have been
    // consumed.
    platform_task_runner->PostTask([&kill_latch] { kill_latch.Signal(); });
  }));
  kill_latch.Wait();

  ASSERT_TRUE(signaled_once);
  signaled_once = false;

  ASSERT_TRUE(destruction_callback_called.load());
  destruction_callback_called = false;
}

TEST(EmbedderTestNoFixture, CanGetCurrentTimeInNanoseconds) {
  auto point1 = fml::TimePoint::FromEpochDelta(
      fml::TimeDelta::FromNanoseconds(FlutterEngineGetCurrentTime()));
  auto point2 = fml::TimePoint::Now();

  ASSERT_LT((point2 - point1), fml::TimeDelta::FromMilliseconds(1));
}

TEST_F(EmbedderTest, CanReloadSystemFonts) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  auto result = FlutterEngineReloadSystemFonts(engine.get());
  ASSERT_EQ(result, kSuccess);
}

TEST_F(EmbedderTest, IsolateServiceIdSent) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  fml::AutoResetWaitableEvent latch;

  fml::Thread thread;
  UniqueEngine engine;
  std::string isolate_message;

  thread.GetTaskRunner()->PostTask([&]() {
    EmbedderConfigBuilder builder(context);
    builder.SetSurface(DlISize(1, 1));
    builder.SetDartEntrypoint("main");
    builder.SetPlatformMessageCallback(
        [&](const FlutterPlatformMessage* message) {
          if (strcmp(message->channel, "flutter/isolate") == 0) {
            isolate_message = {reinterpret_cast<const char*>(message->message),
                               message->message_size};
            latch.Signal();
          }
        });
    engine = builder.LaunchEngine();
    ASSERT_TRUE(engine.is_valid());
  });

  // Wait for the isolate ID message and check its format.
  latch.Wait();
  ASSERT_EQ(isolate_message.find("isolates/"), 0ul);

  // Since the engine was started on its own thread, it must be killed there as
  // well.
  fml::AutoResetWaitableEvent kill_latch;
  thread.GetTaskRunner()->PostTask(
      fml::MakeCopyable([&engine, &kill_latch]() mutable {
        engine.reset();
        kill_latch.Signal();
      }));
  kill_latch.Wait();
}

//------------------------------------------------------------------------------
/// Creates a platform message response callbacks, does NOT send them, and
/// immediately collects the same.
///
TEST_F(EmbedderTest, CanCreateAndCollectCallbacks) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("platform_messages_response");
  context.AddFfiNativeCallback("SignalNativeTest", CREATE_FFI_LAMBDA([]() {}));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  FlutterPlatformMessageResponseHandle* response_handle = nullptr;
  auto callback = [](const uint8_t* data, size_t size,
                     void* user_data) -> void {};
  auto result = FlutterPlatformMessageCreateResponseHandle(
      engine.get(), callback, nullptr, &response_handle);
  ASSERT_EQ(result, kSuccess);
  ASSERT_NE(response_handle, nullptr);

  result = FlutterPlatformMessageReleaseResponseHandle(engine.get(),
                                                       response_handle);
  ASSERT_EQ(result, kSuccess);
}

//------------------------------------------------------------------------------
/// Sends platform messages to Dart code than simply echoes the contents of the
/// message back to the embedder. The embedder registers a native callback to
/// intercept that message.
///
TEST_F(EmbedderTest, PlatformMessagesCanReceiveResponse) {
  struct Captures {
    fml::AutoResetWaitableEvent latch;
    std::thread::id thread_id;
  };
  Captures captures;

  CreateNewThread()->PostTask([&]() {
    captures.thread_id = std::this_thread::get_id();
    auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
    EmbedderConfigBuilder builder(context);
    builder.SetSurface(DlISize(1, 1));
    builder.SetDartEntrypoint("platform_messages_response");

    fml::AutoResetWaitableEvent ready;
    context.AddFfiNativeCallback(
        "SignalNativeTest", CREATE_FFI_LAMBDA([&ready]() { ready.Signal(); }));

    auto engine = builder.LaunchEngine();
    ASSERT_TRUE(engine.is_valid());

    static std::string kMessageData = "Hello from embedder.";

    FlutterPlatformMessageResponseHandle* response_handle = nullptr;
    auto callback = [](const uint8_t* data, size_t size,
                       void* user_data) -> void {
      ASSERT_EQ(size, kMessageData.size());
      ASSERT_EQ(strncmp(reinterpret_cast<const char*>(kMessageData.data()),
                        reinterpret_cast<const char*>(data), size),
                0);
      auto captures = reinterpret_cast<Captures*>(user_data);
      ASSERT_EQ(captures->thread_id, std::this_thread::get_id());
      captures->latch.Signal();
    };
    auto result = FlutterPlatformMessageCreateResponseHandle(
        engine.get(), callback, &captures, &response_handle);
    ASSERT_EQ(result, kSuccess);

    FlutterPlatformMessage message = {};
    message.struct_size = sizeof(FlutterPlatformMessage);
    message.channel = "test_channel";
    message.message = reinterpret_cast<const uint8_t*>(kMessageData.data());
    message.message_size = kMessageData.size();
    message.response_handle = response_handle;

    ready.Wait();
    result = FlutterEngineSendPlatformMessage(engine.get(), &message);
    ASSERT_EQ(result, kSuccess);

    result = FlutterPlatformMessageReleaseResponseHandle(engine.get(),
                                                         response_handle);
    ASSERT_EQ(result, kSuccess);
  });

  captures.latch.Wait();
}

//------------------------------------------------------------------------------
/// Tests that a platform message can be sent with no response handle. Instead
/// of the platform message integrity checked via a response handle, a native
/// callback with the response is invoked to assert integrity.
///
TEST_F(EmbedderTest, PlatformMessagesCanBeSentWithoutResponseHandles) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("platform_messages_no_response");

  const std::string message_data = "Hello but don't call me back.";

  fml::AutoResetWaitableEvent ready, message;
  context.AddFfiNativeCallback(
      "SignalNativeTest", CREATE_FFI_LAMBDA([&ready]() { ready.Signal(); }));
  context.AddFfiNativeCallback(
      "SignalNativeMessage",
      CREATE_FFI_LAMBDA(([&message, &message_data](Dart_Handle message_handle) {
        auto received_message =
            tonic::DartConverter<std::string>::FromDart(message_handle);
        ASSERT_EQ(received_message, message_data);
        message.Signal();
      })));

  auto engine = builder.LaunchEngine();

  ASSERT_TRUE(engine.is_valid());
  ready.Wait();

  FlutterPlatformMessage platform_message = {};
  platform_message.struct_size = sizeof(FlutterPlatformMessage);
  platform_message.channel = "test_channel";
  platform_message.message =
      reinterpret_cast<const uint8_t*>(message_data.data());
  platform_message.message_size = message_data.size();
  platform_message.response_handle = nullptr;  // No response needed.

  auto result =
      FlutterEngineSendPlatformMessage(engine.get(), &platform_message);
  ASSERT_EQ(result, kSuccess);
  message.Wait();
}

//------------------------------------------------------------------------------
/// Tests that a null platform message can be sent.
///
TEST_F(EmbedderTest, NullPlatformMessagesCanBeSent) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("null_platform_messages");

  fml::AutoResetWaitableEvent ready, message;
  context.AddFfiNativeCallback(
      "SignalNativeTest", CREATE_FFI_LAMBDA([&ready]() { ready.Signal(); }));
  context.AddFfiNativeCallback(
      "SignalNativeMessage",
      CREATE_FFI_LAMBDA(([&message](Dart_Handle message_handle) {
        auto received_message =
            tonic::DartConverter<std::string>::FromDart(message_handle);
        ASSERT_EQ("true", received_message);
        message.Signal();
      })));

  auto engine = builder.LaunchEngine();

  ASSERT_TRUE(engine.is_valid());
  ready.Wait();

  FlutterPlatformMessage platform_message = {};
  platform_message.struct_size = sizeof(FlutterPlatformMessage);
  platform_message.channel = "test_channel";
  platform_message.message = nullptr;
  platform_message.message_size = 0;
  platform_message.response_handle = nullptr;  // No response needed.

  auto result =
      FlutterEngineSendPlatformMessage(engine.get(), &platform_message);
  ASSERT_EQ(result, kSuccess);
  message.Wait();
}

//------------------------------------------------------------------------------
/// Tests that a null platform message cannot be send if the message_size
/// isn't equals to 0.
///
TEST_F(EmbedderTest, InvalidPlatformMessages) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto engine = builder.LaunchEngine();

  ASSERT_TRUE(engine.is_valid());

  FlutterPlatformMessage platform_message = {};
  platform_message.struct_size = sizeof(FlutterPlatformMessage);
  platform_message.channel = "test_channel";
  platform_message.message = nullptr;
  platform_message.message_size = 1;
  platform_message.response_handle = nullptr;  // No response needed.

  auto result =
      FlutterEngineSendPlatformMessage(engine.get(), &platform_message);
  ASSERT_EQ(result, kInvalidArguments);
}

//------------------------------------------------------------------------------
/// Tests that setting a custom log callback works as expected and defaults to
/// using tag "flutter".
TEST_F(EmbedderTest, CanSetCustomLogMessageCallback) {
  fml::AutoResetWaitableEvent callback_latch;
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetDartEntrypoint("custom_logger");
  builder.SetSurface(DlISize(1, 1));
  context.SetLogMessageCallback(
      [&callback_latch](const char* tag, const char* message) {
        EXPECT_EQ(std::string(tag), "flutter");
        EXPECT_EQ(std::string(message), "hello world");
        callback_latch.Signal();
      });
  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());
  callback_latch.Wait();
}

//------------------------------------------------------------------------------
/// Tests that setting a custom log tag works.
TEST_F(EmbedderTest, CanSetCustomLogTag) {
  fml::AutoResetWaitableEvent callback_latch;
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetDartEntrypoint("custom_logger");
  builder.SetSurface(DlISize(1, 1));
  builder.SetLogTag("butterfly");
  context.SetLogMessageCallback(
      [&callback_latch](const char* tag, const char* message) {
        EXPECT_EQ(std::string(tag), "butterfly");
        EXPECT_EQ(std::string(message), "hello world");
        callback_latch.Signal();
      });
  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());
  callback_latch.Wait();
}

//------------------------------------------------------------------------------
/// Asserts behavior of FlutterProjectArgs::shutdown_dart_vm_when_done (which is
/// set to true by default in these unit-tests).
///
TEST_F(EmbedderTest, VMShutsDownWhenNoEnginesInProcess) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  const auto launch_count = DartVM::GetVMLaunchCount();

  {
    auto engine = builder.LaunchEngine();
    ASSERT_EQ(launch_count + 1u, DartVM::GetVMLaunchCount());
  }

  {
    auto engine = builder.LaunchEngine();
    ASSERT_EQ(launch_count + 2u, DartVM::GetVMLaunchCount());
  }
}

//------------------------------------------------------------------------------
///
TEST_F(EmbedderTest, DartEntrypointArgs) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.AddDartEntrypointArgument("foo");
  builder.AddDartEntrypointArgument("bar");
  builder.SetDartEntrypoint("dart_entrypoint_args");
  fml::AutoResetWaitableEvent callback_latch;
  std::vector<std::string> callback_args;
  auto nativeArgumentsCallback = [&callback_args,
                                  &callback_latch](Dart_Handle args) {
    callback_args =
        tonic::DartConverter<std::vector<std::string>>::FromDart(args);
    callback_latch.Signal();
  };
  context.AddFfiNativeCallback("NativeArgumentsCallback",
                               CREATE_FFI_LAMBDA(nativeArgumentsCallback));
  auto engine = builder.LaunchEngine();
  callback_latch.Wait();
  ASSERT_EQ(callback_args[0], "foo");
  ASSERT_EQ(callback_args[1], "bar");
}

//------------------------------------------------------------------------------
/// These snapshots may be materialized from symbols and the size field may not
/// be relevant. Since this information is redundant, engine launch should not
/// be gated on a non-zero buffer size.
///
TEST_F(EmbedderTest, VMAndIsolateSnapshotSizesAreRedundantInAOTMode) {
  if (!DartVM::IsRunningPrecompiledCode()) {
    GTEST_SKIP();
    return;
  }
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));

  // The fixture sets this up correctly. Intentionally mess up the args.
  builder.GetProjectArgs().vm_snapshot_data_size = 0;
  builder.GetProjectArgs().vm_snapshot_instructions_size = 0;
  builder.GetProjectArgs().isolate_snapshot_data_size = 0;
  builder.GetProjectArgs().isolate_snapshot_instructions_size = 0;

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());
}

TEST_F(EmbedderTest, CanRenderImplicitView) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(800, 600));
  builder.SetCompositor();
  builder.SetDartEntrypoint("render_implicit_view");
  builder.SetRenderTargetType(
      EmbedderTestBackingStoreProducer::RenderTargetType::kSoftwareBuffer);

  fml::AutoResetWaitableEvent latch;

  context.GetCompositor().SetNextPresentCallback(
      [&](FlutterViewId view_id, const FlutterLayer** layers,
          size_t layers_count) {
        ASSERT_EQ(view_id, kFlutterImplicitViewId);
        latch.Signal();
      });

  auto engine = builder.LaunchEngine();

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 300;
  event.height = 200;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);
  ASSERT_TRUE(engine.is_valid());
  latch.Wait();
}

TEST_F(EmbedderTest, CanRenderImplicitViewUsingPresentLayersCallback) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(800, 600));
  builder.SetCompositor(/* avoid_backing_store_cache = */ false,
                        /* use_present_layers_callback = */ true);
  builder.SetDartEntrypoint("render_implicit_view");
  builder.SetRenderTargetType(
      EmbedderTestBackingStoreProducer::RenderTargetType::kSoftwareBuffer);

  fml::AutoResetWaitableEvent latch;

  context.GetCompositor().SetNextPresentCallback(
      [&](FlutterViewId view_id, const FlutterLayer** layers,
          size_t layers_count) {
        ASSERT_EQ(view_id, kFlutterImplicitViewId);
        latch.Signal();
      });

  auto engine = builder.LaunchEngine();

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 300;
  event.height = 200;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);
  ASSERT_TRUE(engine.is_valid());
  latch.Wait();
}

//------------------------------------------------------------------------------
/// Test the layer structure and pixels rendered when using a custom software
/// compositor.
///
// TODO(143940): Convert this test to use SkiaGold.
#if FML_OS_MACOSX && FML_ARCH_CPU_ARM64
TEST_F(EmbedderTest,
       DISABLED_CompositorMustBeAbleToRenderKnownSceneWithSoftwareCompositor) {
#else
TEST_F(EmbedderTest,
       CompositorMustBeAbleToRenderKnownSceneWithSoftwareCompositor) {
#endif  // FML_OS_MACOSX && FML_ARCH_CPU_ARM64

  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(800, 600));
  builder.SetCompositor();
  builder.SetDartEntrypoint("can_composite_platform_views_with_known_scene");

  builder.SetRenderTargetType(
      EmbedderTestBackingStoreProducer::RenderTargetType::kSoftwareBuffer);

  fml::CountDownLatch latch(5);

  auto scene_image = context.GetNextSceneImage();

  context.GetCompositor().SetNextPresentCallback(
      [&](FlutterViewId view_id, const FlutterLayer** layers,
          size_t layers_count) {
        ASSERT_EQ(layers_count, 5u);

        // Layer Root
        {
          FlutterBackingStore backing_store = *layers[0]->backing_store;
          backing_store.type = kFlutterBackingStoreTypeSoftware;
          backing_store.did_update = true;
          backing_store.software.height = 600;

          FlutterRect paint_region_rects[] = {
              FlutterRectMakeLTRB(0, 0, 800, 600),
          };
          FlutterRegion paint_region = {
              .struct_size = sizeof(FlutterRegion),
              .rects_count = 1,
              .rects = paint_region_rects,
          };
          FlutterBackingStorePresentInfo present_info = {
              .struct_size = sizeof(FlutterBackingStorePresentInfo),
              .paint_region = &paint_region,
          };

          FlutterLayer layer = {};
          layer.struct_size = sizeof(layer);
          layer.type = kFlutterLayerContentTypeBackingStore;
          layer.backing_store = &backing_store;
          layer.size = FlutterSizeMake(800.0, 600.0);
          layer.offset = FlutterPointMake(0.0, 0.0);
          layer.backing_store_present_info = &present_info;

          ASSERT_EQ(*layers[0], layer);
        }

        // Layer 1
        {
          FlutterPlatformView platform_view = *layers[1]->platform_view;
          platform_view.struct_size = sizeof(platform_view);
          platform_view.identifier = 1;

          FlutterLayer layer = {};
          layer.struct_size = sizeof(layer);
          layer.type = kFlutterLayerContentTypePlatformView;
          layer.platform_view = &platform_view;
          layer.size = FlutterSizeMake(50.0, 150.0);
          layer.offset = FlutterPointMake(20.0, 20.0);

          ASSERT_EQ(*layers[1], layer);
        }

        // Layer 2
        {
          FlutterBackingStore backing_store = *layers[2]->backing_store;
          backing_store.type = kFlutterBackingStoreTypeSoftware;
          backing_store.did_update = true;
          backing_store.software.height = 600;

          FlutterRect paint_region_rects[] = {
              FlutterRectMakeLTRB(30, 30, 80, 180),
          };
          FlutterRegion paint_region = {
              .struct_size = sizeof(FlutterRegion),
              .rects_count = 1,
              .rects = paint_region_rects,
          };
          FlutterBackingStorePresentInfo present_info = {
              .struct_size = sizeof(FlutterBackingStorePresentInfo),
              .paint_region = &paint_region,
          };

          FlutterLayer layer = {};
          layer.struct_size = sizeof(layer);
          layer.type = kFlutterLayerContentTypeBackingStore;
          layer.backing_store = &backing_store;
          layer.size = FlutterSizeMake(800.0, 600.0);
          layer.offset = FlutterPointMake(0.0, 0.0);
          layer.backing_store_present_info = &present_info;

          ASSERT_EQ(*layers[2], layer);
        }

        // Layer 3
        {
          FlutterPlatformView platform_view = *layers[3]->platform_view;
          platform_view.struct_size = sizeof(platform_view);
          platform_view.identifier = 2;

          FlutterLayer layer = {};
          layer.struct_size = sizeof(layer);
          layer.type = kFlutterLayerContentTypePlatformView;
          layer.platform_view = &platform_view;
          layer.size = FlutterSizeMake(50.0, 150.0);
          layer.offset = FlutterPointMake(40.0, 40.0);

          ASSERT_EQ(*layers[3], layer);
        }

        // Layer 4
        {
          FlutterBackingStore backing_store = *layers[4]->backing_store;
          backing_store.type = kFlutterBackingStoreTypeSoftware;
          backing_store.did_update = true;
          backing_store.software.height = 600;

          FlutterRect paint_region_rects[] = {
              FlutterRectMakeLTRB(50, 50, 100, 200),
          };
          FlutterRegion paint_region = {
              .struct_size = sizeof(FlutterRegion),
              .rects_count = 1,
              .rects = paint_region_rects,
          };
          FlutterBackingStorePresentInfo present_info = {
              .struct_size = sizeof(FlutterBackingStorePresentInfo),
              .paint_region = &paint_region,
          };

          FlutterLayer layer = {};
          layer.struct_size = sizeof(layer);
          layer.type = kFlutterLayerContentTypeBackingStore;
          layer.backing_store = &backing_store;
          layer.size = FlutterSizeMake(800.0, 600.0);
          layer.offset = FlutterPointMake(0.0, 0.0);
          layer.backing_store_present_info = &present_info;

          ASSERT_EQ(*layers[4], layer);
        }

        latch.CountDown();
      });

  context.GetCompositor().SetPlatformViewRendererCallback(
      [&](const FlutterLayer& layer, GrDirectContext*
          /* don't use because software compositor */) -> sk_sp<SkImage> {
        auto surface = CreateRenderSurface(
            layer, nullptr /* null because software compositor */);
        auto canvas = surface->getCanvas();
        FML_CHECK(canvas != nullptr);

        switch (layer.platform_view->identifier) {
          case 1: {
            SkPaint paint;
            // See dart test for total order.
            paint.setColor(SK_ColorGREEN);
            paint.setAlpha(127);
            const auto& rect =
                SkRect::MakeWH(layer.size.width, layer.size.height);
            canvas->drawRect(rect, paint);
            latch.CountDown();
          } break;
          case 2: {
            SkPaint paint;
            // See dart test for total order.
            paint.setColor(SK_ColorMAGENTA);
            paint.setAlpha(127);
            const auto& rect =
                SkRect::MakeWH(layer.size.width, layer.size.height);
            canvas->drawRect(rect, paint);
            latch.CountDown();
          } break;
          default:
            // Asked to render an unknown platform view.
            FML_CHECK(false)
                << "Test was asked to composite an unknown platform view.";
        }

        return surface->makeImageSnapshot();
      });

  context.AddFfiNativeCallback(
      "SignalNativeTest", CREATE_FFI_LAMBDA([&latch]() { latch.CountDown(); }));

  auto engine = builder.LaunchEngine();

  // Send a window metrics events so frames may be scheduled.
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 800;
  event.height = 600;
  event.pixel_ratio = 1.0;
  event.physical_view_inset_top = 0.0;
  event.physical_view_inset_right = 0.0;
  event.physical_view_inset_bottom = 0.0;
  event.physical_view_inset_left = 0.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);
  ASSERT_TRUE(engine.is_valid());

  latch.Wait();

  ASSERT_TRUE(ImageMatchesFixture("compositor_software.png", scene_image));

  // There should no present calls on the root surface.
  ASSERT_EQ(context.GetSurfacePresentCount(), 0u);
}

//------------------------------------------------------------------------------
/// Test the layer structure and pixels rendered when using a custom software
/// compositor, with a transparent overlay
///
TEST_F(EmbedderTest, NoLayerCreatedForTransparentOverlayOnTopOfPlatformLayer) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(800, 600));
  builder.SetCompositor();
  builder.SetDartEntrypoint("can_composite_platform_views_transparent_overlay");

  builder.SetRenderTargetType(
      EmbedderTestBackingStoreProducer::RenderTargetType::kSoftwareBuffer);

  fml::CountDownLatch latch(4);

  auto scene_image = context.GetNextSceneImage();

  context.GetCompositor().SetNextPresentCallback(
      [&](FlutterViewId view_id, const FlutterLayer** layers,
          size_t layers_count) {
        ASSERT_EQ(layers_count, 2u);

        // Layer Root
        {
          FlutterBackingStore backing_store = *layers[0]->backing_store;
          backing_store.type = kFlutterBackingStoreTypeSoftware;
          backing_store.did_update = true;
          backing_store.software.height = 600;

          FlutterRect paint_region_rects[] = {
              FlutterRectMakeLTRB(0, 0, 800, 600),
          };
          FlutterRegion paint_region = {
              .struct_size = sizeof(FlutterRegion),
              .rects_count = 1,
              .rects = paint_region_rects,
          };
          FlutterBackingStorePresentInfo present_info = {
              .struct_size = sizeof(FlutterBackingStorePresentInfo),
              .paint_region = &paint_region,
          };

          FlutterLayer layer = {};
          layer.struct_size = sizeof(layer);
          layer.type = kFlutterLayerContentTypeBackingStore;
          layer.backing_store = &backing_store;
          layer.size = FlutterSizeMake(800.0, 600.0);
          layer.offset = FlutterPointMake(0.0, 0.0);
          layer.backing_store_present_info = &present_info;

          ASSERT_EQ(*layers[0], layer);
        }

        // Layer 1
        {
          FlutterPlatformView platform_view = *layers[1]->platform_view;
          platform_view.struct_size = sizeof(platform_view);
          platform_view.identifier = 1;

          FlutterLayer layer = {};
          layer.struct_size = sizeof(layer);
          layer.type = kFlutterLayerContentTypePlatformView;
          layer.platform_view = &platform_view;
          layer.size = FlutterSizeMake(50.0, 150.0);
          layer.offset = FlutterPointMake(20.0, 20.0);

          ASSERT_EQ(*layers[1], layer);
        }

        latch.CountDown();
      });

  context.GetCompositor().SetPlatformViewRendererCallback(
      [&](const FlutterLayer& layer, GrDirectContext*
          /* don't use because software compositor */) -> sk_sp<SkImage> {
        auto surface = CreateRenderSurface(
            layer, nullptr /* null because software compositor */);
        auto canvas = surface->getCanvas();
        FML_CHECK(canvas != nullptr);

        switch (layer.platform_view->identifier) {
          case 1: {
            SkPaint paint;
            // See dart test for total order.
            paint.setColor(SK_ColorGREEN);
            paint.setAlpha(127);
            const auto& rect =
                SkRect::MakeWH(layer.size.width, layer.size.height);
            canvas->drawRect(rect, paint);
            latch.CountDown();
          } break;
          default:
            // Asked to render an unknown platform view.
            FML_CHECK(false)
                << "Test was asked to composite an unknown platform view.";
        }

        return surface->makeImageSnapshot();
      });

  context.AddFfiNativeCallback(
      "SignalNativeTest", CREATE_FFI_LAMBDA([&latch]() { latch.CountDown(); }));

  auto engine = builder.LaunchEngine();

  // Send a window metrics events so frames may be scheduled.
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 800;
  event.height = 600;
  event.pixel_ratio = 1.0;
  event.physical_view_inset_top = 0.0;
  event.physical_view_inset_right = 0.0;
  event.physical_view_inset_bottom = 0.0;
  event.physical_view_inset_left = 0.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);
  ASSERT_TRUE(engine.is_valid());

  latch.Wait();

  // TODO(https://github.com/flutter/flutter/issues/53784): enable this on all
  // platforms.
#if !defined(FML_OS_LINUX)
  GTEST_SKIP() << "Skipping golden tests on non-Linux OSes";
#endif  // FML_OS_LINUX
  ASSERT_TRUE(ImageMatchesFixture(
      "compositor_platform_layer_with_no_overlay.png", scene_image));

  // There should no present calls on the root surface.
  ASSERT_EQ(context.GetSurfacePresentCount(), 0u);
}

//------------------------------------------------------------------------------
/// Test the layer structure and pixels rendered when using a custom software
/// compositor, with a no overlay
///
TEST_F(EmbedderTest, NoLayerCreatedForNoOverlayOnTopOfPlatformLayer) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(800, 600));
  builder.SetCompositor();
  builder.SetDartEntrypoint("can_composite_platform_views_no_overlay");

  builder.SetRenderTargetType(
      EmbedderTestBackingStoreProducer::RenderTargetType::kSoftwareBuffer);

  fml::CountDownLatch latch(4);

  auto scene_image = context.GetNextSceneImage();

  context.GetCompositor().SetNextPresentCallback(
      [&](FlutterViewId view_id, const FlutterLayer** layers,
          size_t layers_count) {
        ASSERT_EQ(layers_count, 2u);

        // Layer Root
        {
          FlutterBackingStore backing_store = *layers[0]->backing_store;
          backing_store.type = kFlutterBackingStoreTypeSoftware;
          backing_store.did_update = true;
          backing_store.software.height = 600;

          FlutterRect paint_region_rects[] = {
              FlutterRectMakeLTRB(0, 0, 800, 600),
          };
          FlutterRegion paint_region = {
              .struct_size = sizeof(FlutterRegion),
              .rects_count = 1,
              .rects = paint_region_rects,
          };
          FlutterBackingStorePresentInfo present_info = {
              .struct_size = sizeof(FlutterBackingStorePresentInfo),
              .paint_region = &paint_region,
          };

          FlutterLayer layer = {};
          layer.struct_size = sizeof(layer);
          layer.type = kFlutterLayerContentTypeBackingStore;
          layer.backing_store = &backing_store;
          layer.size = FlutterSizeMake(800.0, 600.0);
          layer.offset = FlutterPointMake(0.0, 0.0);
          layer.backing_store_present_info = &present_info;

          ASSERT_EQ(*layers[0], layer);
        }

        // Layer 1
        {
          FlutterPlatformView platform_view = *layers[1]->platform_view;
          platform_view.struct_size = sizeof(platform_view);
          platform_view.identifier = 1;

          FlutterLayer layer = {};
          layer.struct_size = sizeof(layer);
          layer.type = kFlutterLayerContentTypePlatformView;
          layer.platform_view = &platform_view;
          layer.size = FlutterSizeMake(50.0, 150.0);
          layer.offset = FlutterPointMake(20.0, 20.0);

          ASSERT_EQ(*layers[1], layer);
        }

        latch.CountDown();
      });

  context.GetCompositor().SetPlatformViewRendererCallback(
      [&](const FlutterLayer& layer, GrDirectContext*
          /* don't use because software compositor */) -> sk_sp<SkImage> {
        auto surface = CreateRenderSurface(
            layer, nullptr /* null because software compositor */);
        auto canvas = surface->getCanvas();
        FML_CHECK(canvas != nullptr);

        switch (layer.platform_view->identifier) {
          case 1: {
            SkPaint paint;
            // See dart test for total order.
            paint.setColor(SK_ColorGREEN);
            paint.setAlpha(127);
            const auto& rect =
                SkRect::MakeWH(layer.size.width, layer.size.height);
            canvas->drawRect(rect, paint);
            latch.CountDown();
          } break;
          default:
            // Asked to render an unknown platform view.
            FML_CHECK(false)
                << "Test was asked to composite an unknown platform view.";
        }

        return surface->makeImageSnapshot();
      });

  context.AddFfiNativeCallback(
      "SignalNativeTest", CREATE_FFI_LAMBDA([&latch]() { latch.CountDown(); }));

  auto engine = builder.LaunchEngine();

  // Send a window metrics events so frames may be scheduled.
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 800;
  event.height = 600;
  event.pixel_ratio = 1.0;
  event.physical_view_inset_top = 0.0;
  event.physical_view_inset_right = 0.0;
  event.physical_view_inset_bottom = 0.0;
  event.physical_view_inset_left = 0.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);
  ASSERT_TRUE(engine.is_valid());

  latch.Wait();

  // TODO(https://github.com/flutter/flutter/issues/53784): enable this on all
  // platforms.
#if !defined(FML_OS_LINUX)
  GTEST_SKIP() << "Skipping golden tests on non-Linux OSes";
#endif  // FML_OS_LINUX
  ASSERT_TRUE(ImageMatchesFixture(
      "compositor_platform_layer_with_no_overlay.png", scene_image));

  // There should no present calls on the root surface.
  ASSERT_EQ(context.GetSurfacePresentCount(), 0u);
}

//------------------------------------------------------------------------------
/// Test that an engine can be initialized but not run.
///
TEST_F(EmbedderTest, CanCreateInitializedEngine) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto engine = builder.InitializeEngine();
  ASSERT_TRUE(engine.is_valid());
  engine.reset();
}

//------------------------------------------------------------------------------
/// Test that an initialized engine can be run exactly once.
///
TEST_F(EmbedderTest, CanRunInitializedEngine) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto engine = builder.InitializeEngine();
  ASSERT_TRUE(engine.is_valid());
  ASSERT_EQ(FlutterEngineRunInitialized(engine.get()), kSuccess);
  // Cannot re-run an already running engine.
  ASSERT_EQ(FlutterEngineRunInitialized(engine.get()), kInvalidArguments);
  engine.reset();
}

//------------------------------------------------------------------------------
/// Test that an engine can be deinitialized.
///
TEST_F(EmbedderTest, CanDeinitializeAnEngine) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto engine = builder.InitializeEngine();
  ASSERT_TRUE(engine.is_valid());
  ASSERT_EQ(FlutterEngineRunInitialized(engine.get()), kSuccess);
  // Cannot re-run an already running engine.
  ASSERT_EQ(FlutterEngineRunInitialized(engine.get()), kInvalidArguments);
  ASSERT_EQ(FlutterEngineDeinitialize(engine.get()), kSuccess);
  // It is ok to deinitialize an engine multiple times.
  ASSERT_EQ(FlutterEngineDeinitialize(engine.get()), kSuccess);

  // Sending events to a deinitialized engine fails.
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 800;
  event.height = 600;
  event.pixel_ratio = 1.0;
  event.physical_view_inset_top = 0.0;
  event.physical_view_inset_right = 0.0;
  event.physical_view_inset_bottom = 0.0;
  event.physical_view_inset_left = 0.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kInvalidArguments);
  engine.reset();
}

//------------------------------------------------------------------------------
/// Test that an engine can be spawned from a running engine.
///
TEST_F(EmbedderTest, SpawnEngine) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto parent_engine = builder.LaunchEngine();
  ASSERT_TRUE(parent_engine.is_valid());

  fml::AutoResetWaitableEvent latch;
  auto native_entrypoint = [&latch]() { latch.Signal(); };
  context.AddFfiNativeCallback("SayHiFromCustomEntrypoint",
                               CREATE_FFI_LAMBDA(native_entrypoint));

  FlutterEngineSpawnConfig spawn_config = {};
  spawn_config.struct_size = sizeof(spawn_config);
  spawn_config.entrypoint = "customEntrypoint";
  spawn_config.initial_route = "/spawned";

  FlutterEngine spawned_engine = nullptr;
  ASSERT_EQ(
      FlutterEngineSpawn(parent_engine.get(), &spawn_config, &spawned_engine),
      kSuccess);
  ASSERT_NE(spawned_engine, nullptr);

  latch.Wait();

  ASSERT_EQ(FlutterEngineShutdown(spawned_engine), kSuccess);
}

//------------------------------------------------------------------------------
/// Test that an engine can be spawned with custom arguments.
///
TEST_F(EmbedderTest, SpawnEngineWithArgs) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto parent_engine = builder.LaunchEngine();
  ASSERT_TRUE(parent_engine.is_valid());

  fml::AutoResetWaitableEvent callback_latch;
  std::vector<std::string> callback_args;
  auto nativeArgumentsCallback = [&callback_args,
                                  &callback_latch](Dart_Handle args) {
    callback_args =
        tonic::DartConverter<std::vector<std::string>>::FromDart(args);
    callback_latch.Signal();
  };
  context.AddFfiNativeCallback("NativeArgumentsCallback",
                               CREATE_FFI_LAMBDA(nativeArgumentsCallback));

  const char* argv[] = {"spawn_arg_1", "spawn_arg_2"};
  FlutterEngineSpawnConfig spawn_config = {};
  spawn_config.struct_size = sizeof(spawn_config);
  spawn_config.entrypoint = "dart_entrypoint_args";
  spawn_config.argc = 2;
  spawn_config.argv = argv;

  FlutterEngine spawned_engine = nullptr;
  ASSERT_EQ(
      FlutterEngineSpawn(parent_engine.get(), &spawn_config, &spawned_engine),
      kSuccess);
  ASSERT_NE(spawned_engine, nullptr);

  callback_latch.Wait();
  ASSERT_EQ(callback_args.size(), 2u);
  ASSERT_EQ(callback_args[0], "spawn_arg_1");
  ASSERT_EQ(callback_args[1], "spawn_arg_2");

  ASSERT_EQ(FlutterEngineShutdown(spawned_engine), kSuccess);
}

//------------------------------------------------------------------------------
/// Test out-of-order shutdown where parent engine is shut down before spawned
/// child engine.
///
TEST_F(EmbedderTest, SpawnEngineOutOfOrderShutdownParentFirst) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto parent_engine = builder.LaunchEngine();
  ASSERT_TRUE(parent_engine.is_valid());

  fml::AutoResetWaitableEvent latch;
  auto native_entrypoint = [&latch]() { latch.Signal(); };
  context.AddFfiNativeCallback("SayHiFromCustomEntrypoint",
                               CREATE_FFI_LAMBDA(native_entrypoint));

  FlutterEngineSpawnConfig spawn_config = {};
  spawn_config.struct_size = sizeof(spawn_config);
  spawn_config.entrypoint = "customEntrypoint";

  FlutterEngine spawned_engine = nullptr;
  ASSERT_EQ(
      FlutterEngineSpawn(parent_engine.get(), &spawn_config, &spawned_engine),
      kSuccess);
  ASSERT_NE(spawned_engine, nullptr);

  latch.Wait();

  // Shut down parent engine first while child engine is still alive.
  parent_engine.reset();

  // Verify child engine shuts down cleanly without crash or deadlock.
  ASSERT_EQ(FlutterEngineShutdown(spawned_engine), kSuccess);
}

//------------------------------------------------------------------------------
/// Test spawning multiple child engines and out-of-order teardown.
///
TEST_F(EmbedderTest, SpawnMultipleEngines) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto parent_engine = builder.LaunchEngine();
  ASSERT_TRUE(parent_engine.is_valid());

  fml::CountDownLatch latch(2);
  auto native_entrypoint = [&latch]() { latch.CountDown(); };
  context.AddFfiNativeCallback("SayHiFromCustomEntrypoint",
                               CREATE_FFI_LAMBDA(native_entrypoint));

  FlutterEngineSpawnConfig spawn_config1 = {};
  spawn_config1.struct_size = sizeof(spawn_config1);
  spawn_config1.entrypoint = "customEntrypoint";

  FlutterEngine child1 = nullptr;
  ASSERT_EQ(FlutterEngineSpawn(parent_engine.get(), &spawn_config1, &child1),
            kSuccess);
  ASSERT_NE(child1, nullptr);

  FlutterEngineSpawnConfig spawn_config2 = {};
  spawn_config2.struct_size = sizeof(spawn_config2);
  spawn_config2.entrypoint = "customEntrypoint";

  FlutterEngine child2 = nullptr;
  ASSERT_EQ(FlutterEngineSpawn(parent_engine.get(), &spawn_config2, &child2),
            kSuccess);
  ASSERT_NE(child2, nullptr);

  latch.Wait();

  // Shut down child 1 first.
  ASSERT_EQ(FlutterEngineShutdown(child1), kSuccess);
  // Shut down parent second.
  parent_engine.reset();
  // Shut down child 2 last.
  ASSERT_EQ(FlutterEngineShutdown(child2), kSuccess);
}

//------------------------------------------------------------------------------
/// Test forward compatibility with truncated FlutterEngineSpawnConfig struct.
///
TEST_F(EmbedderTest, SpawnEngineSafeAccessTruncatedStruct) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto parent_engine = builder.LaunchEngine();
  ASSERT_TRUE(parent_engine.is_valid());

  fml::AutoResetWaitableEvent latch;
  auto native_entrypoint = [&latch]() { latch.Signal(); };
  context.AddFfiNativeCallback("SayHiFromCustomEntrypoint",
                               CREATE_FFI_LAMBDA(native_entrypoint));

  // Truncate struct_size to only include struct_size and entrypoint.
  FlutterEngineSpawnConfig spawn_config = {};
  spawn_config.struct_size = sizeof(size_t) + sizeof(const char*);
  spawn_config.entrypoint = "customEntrypoint";

  FlutterEngine spawned_engine = nullptr;
  ASSERT_EQ(
      FlutterEngineSpawn(parent_engine.get(), &spawn_config, &spawned_engine),
      kSuccess);
  ASSERT_NE(spawned_engine, nullptr);

  latch.Wait();

  ASSERT_EQ(FlutterEngineShutdown(spawned_engine), kSuccess);
}

//------------------------------------------------------------------------------
/// Test invalid arguments passed to FlutterEngineSpawn.
///
TEST_F(EmbedderTest, SpawnEngineInvalidArguments) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto parent_engine = builder.LaunchEngine();
  ASSERT_TRUE(parent_engine.is_valid());

  FlutterEngine spawned_engine = nullptr;
  FlutterEngineSpawnConfig spawn_config = {};
  spawn_config.struct_size = sizeof(spawn_config);

  // Null parent engine.
  ASSERT_EQ(FlutterEngineSpawn(nullptr, &spawn_config, &spawned_engine),
            kInvalidArguments);

  // Null config.
  ASSERT_EQ(FlutterEngineSpawn(parent_engine.get(), nullptr, &spawned_engine),
            kInvalidArguments);

  // Null out parameter.
  ASSERT_EQ(FlutterEngineSpawn(parent_engine.get(), &spawn_config, nullptr),
            kInvalidArguments);

  // Zero struct_size.
  FlutterEngineSpawnConfig zero_size_config = {};
  zero_size_config.struct_size = 0;
  ASSERT_EQ(FlutterEngineSpawn(parent_engine.get(), &zero_size_config,
                               &spawned_engine),
            kInvalidArguments);

  // Negative argc.
  FlutterEngineSpawnConfig negative_argc_config = {};
  negative_argc_config.struct_size = sizeof(negative_argc_config);
  negative_argc_config.argc = -1;
  ASSERT_EQ(FlutterEngineSpawn(parent_engine.get(), &negative_argc_config,
                               &spawned_engine),
            kInvalidArguments);

  // argc > 0 but argv is null.
  FlutterEngineSpawnConfig null_argv_config = {};
  null_argv_config.struct_size = sizeof(null_argv_config);
  null_argv_config.argc = 1;
  null_argv_config.argv = nullptr;
  ASSERT_EQ(FlutterEngineSpawn(parent_engine.get(), &null_argv_config,
                               &spawned_engine),
            kInvalidArguments);
}

//------------------------------------------------------------------------------
/// Test that a view can be added to a running engine.
///
TEST_F(EmbedderTest, CanAddView) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("window_metrics_event_all_view_ids");

  fml::AutoResetWaitableEvent ready_latch, message_latch;
  context.AddFfiNativeCallback(
      "SignalNativeTest",
      CREATE_FFI_LAMBDA([&ready_latch]() { ready_latch.Signal(); }));

  std::string message;
  context.AddFfiNativeCallback(
      "SignalNativeMessage", CREATE_FFI_LAMBDA([&](Dart_Handle message_handle) {
        message = tonic::DartConverter<std::string>::FromDart(message_handle);
        message_latch.Signal();
      }));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  ready_latch.Wait();

  FlutterWindowMetricsEvent metrics = {};
  metrics.struct_size = sizeof(FlutterWindowMetricsEvent);
  metrics.width = 800;
  metrics.height = 600;
  metrics.pixel_ratio = 1.0;
  metrics.view_id = 123;

  FlutterAddViewInfo info = {};
  info.struct_size = sizeof(FlutterAddViewInfo);
  info.view_id = 123;
  info.view_metrics = &metrics;
  info.add_view_callback = [](const FlutterAddViewResult* result) {
    EXPECT_TRUE(result->added);
  };
  ASSERT_EQ(FlutterEngineAddView(engine.get(), &info), kSuccess);
  message_latch.Wait();
  ASSERT_EQ("View IDs: [0, 123]", message);
}

//------------------------------------------------------------------------------
/// Test that adding a view schedules a frame.
///
TEST_F(EmbedderTest, AddViewSchedulesFrame) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("add_view_schedules_frame");
  fml::AutoResetWaitableEvent latch;
  context.AddFfiNativeCallback(
      "SignalNativeTest", CREATE_FFI_LAMBDA([&latch]() { latch.Signal(); }));

  fml::AutoResetWaitableEvent check_latch;
  context.AddFfiNativeCallback(
      "SignalNativeCount",
      CREATE_FFI_LAMBDA([&check_latch](int count) { check_latch.Signal(); }));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  // Wait for the application to attach the listener.
  latch.Wait();

  FlutterWindowMetricsEvent metrics = {};
  metrics.struct_size = sizeof(FlutterWindowMetricsEvent);
  metrics.width = 800;
  metrics.height = 600;
  metrics.pixel_ratio = 1.0;
  metrics.view_id = 123;

  FlutterAddViewInfo info = {};
  info.struct_size = sizeof(FlutterAddViewInfo);
  info.view_id = 123;
  info.view_metrics = &metrics;
  info.add_view_callback = [](const FlutterAddViewResult* result) {
    EXPECT_TRUE(result->added);
  };
  ASSERT_EQ(FlutterEngineAddView(engine.get(), &info), kSuccess);

  check_latch.Wait();
}

//------------------------------------------------------------------------------
/// Test that a view that was added can be removed.
///
TEST_F(EmbedderTest, CanRemoveView) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("window_metrics_event_all_view_ids");

  fml::AutoResetWaitableEvent ready_latch, message_latch;
  context.AddFfiNativeCallback(
      "SignalNativeTest",
      CREATE_FFI_LAMBDA([&ready_latch]() { ready_latch.Signal(); }));

  std::string message;
  context.AddFfiNativeCallback(
      "SignalNativeMessage", CREATE_FFI_LAMBDA([&](Dart_Handle message_handle) {
        message = tonic::DartConverter<std::string>::FromDart(message_handle);
        message_latch.Signal();
      }));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  ready_latch.Wait();

  // Add view 123.
  FlutterWindowMetricsEvent metrics = {};
  metrics.struct_size = sizeof(FlutterWindowMetricsEvent);
  metrics.width = 800;
  metrics.height = 600;
  metrics.pixel_ratio = 1.0;
  metrics.view_id = 123;

  FlutterAddViewInfo add_info = {};
  add_info.struct_size = sizeof(FlutterAddViewInfo);
  add_info.view_id = 123;
  add_info.view_metrics = &metrics;
  add_info.add_view_callback = [](const FlutterAddViewResult* result) {
    ASSERT_TRUE(result->added);
  };
  ASSERT_EQ(FlutterEngineAddView(engine.get(), &add_info), kSuccess);
  message_latch.Wait();
  ASSERT_EQ(message, "View IDs: [0, 123]");

  // Remove view 123.
  FlutterRemoveViewInfo remove_info = {};
  remove_info.struct_size = sizeof(FlutterAddViewInfo);
  remove_info.view_id = 123;
  remove_info.remove_view_callback = [](const FlutterRemoveViewResult* result) {
    EXPECT_TRUE(result->removed);
  };
  ASSERT_EQ(FlutterEngineRemoveView(engine.get(), &remove_info), kSuccess);
  message_latch.Wait();
  ASSERT_EQ(message, "View IDs: [0]");
}

// Regression test for:
// https://github.com/flutter/flutter/issues/164564
TEST_F(EmbedderTest, RemoveViewCallbackIsInvokedAfterRasterThreadIsDone) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  std::mutex engine_mutex;
  UniqueEngine engine;
  auto render_thread = CreateNewThread("custom_render_thread");
  EmbedderTestTaskRunner render_task_runner(
      render_thread, [&](FlutterTask task) {
        std::scoped_lock engine_lock(engine_mutex);
        if (engine.is_valid()) {
          ASSERT_EQ(FlutterEngineRunTask(engine.get(), &task), kSuccess);
        }
      });

  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("remove_view_callback_too_early");
  builder.SetRenderTaskRunner(
      &render_task_runner.GetFlutterTaskRunnerDescription());

  fml::AutoResetWaitableEvent ready_latch;
  context.AddFfiNativeCallback(
      "SignalNativeTest",
      CREATE_FFI_LAMBDA([&ready_latch]() { ready_latch.Signal(); }));

  {
    std::scoped_lock lock(engine_mutex);
    engine = builder.InitializeEngine();
  }
  ASSERT_EQ(FlutterEngineRunInitialized(engine.get()), kSuccess);
  ASSERT_TRUE(engine.is_valid());

  ready_latch.Wait();

  fml::AutoResetWaitableEvent add_view_latch;
  // Add view 123.
  FlutterWindowMetricsEvent metrics = {};
  metrics.struct_size = sizeof(FlutterWindowMetricsEvent);
  metrics.width = 800;
  metrics.height = 600;
  metrics.pixel_ratio = 1.0;
  metrics.view_id = 123;

  FlutterAddViewInfo add_info = {};
  add_info.struct_size = sizeof(FlutterAddViewInfo);
  add_info.view_id = 123;
  add_info.view_metrics = &metrics;
  add_info.user_data = &add_view_latch;
  add_info.add_view_callback = [](const FlutterAddViewResult* result) {
    ASSERT_TRUE(result->added);
    auto add_view_latch =
        reinterpret_cast<fml::AutoResetWaitableEvent*>(result->user_data);
    add_view_latch->Signal();
  };
  ASSERT_EQ(FlutterEngineAddView(engine.get(), &add_info), kSuccess);
  add_view_latch.Wait();

  std::atomic_bool view_available = true;

  // Simulate pending rasterization task scheduled before view removal request
  // that accesses view resources.
  fml::AutoResetWaitableEvent raster_thread_latch;
  render_thread->PostTask([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    // View must be available.
    EXPECT_TRUE(view_available);
    raster_thread_latch.Signal();
  });

  fml::AutoResetWaitableEvent remove_view_latch;
  FlutterRemoveViewInfo remove_view_info = {};
  remove_view_info.struct_size = sizeof(FlutterRemoveViewInfo);
  remove_view_info.view_id = 123;
  remove_view_info.user_data = &remove_view_latch;
  remove_view_info.remove_view_callback =
      [](const FlutterRemoveViewResult* result) {
        ASSERT_TRUE(result->removed);
        auto remove_view_latch =
            reinterpret_cast<fml::AutoResetWaitableEvent*>(result->user_data);
        remove_view_latch->Signal();
      };

  // Remove the view and wait until the callback is called.
  ASSERT_EQ(FlutterEngineRemoveView(engine.get(), &remove_view_info), kSuccess);
  remove_view_latch.Wait();

  // After FlutterEngineRemoveViewCallback is called it should be safe to
  // remove view - raster thread must not be accessing any view resources.
  view_available = false;
  raster_thread_latch.Wait();

  FlutterEngineDeinitialize(engine.get());
}

//------------------------------------------------------------------------------
/// The implicit view is a special view that the engine and framework assume
/// can *always* be rendered to. Test that this view cannot be removed.
///
TEST_F(EmbedderTest, CannotRemoveImplicitView) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  FlutterRemoveViewInfo info = {};
  info.struct_size = sizeof(FlutterRemoveViewInfo);
  info.view_id = kFlutterImplicitViewId;
  info.remove_view_callback = [](const FlutterRemoveViewResult* result) {
    FAIL();
  };
  ASSERT_EQ(FlutterEngineRemoveView(engine.get(), &info), kInvalidArguments);
}

//------------------------------------------------------------------------------
/// Test that a view cannot be added if its ID already exists.
///
TEST_F(EmbedderTest, CannotAddDuplicateViews) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("window_metrics_event_all_view_ids");

  fml::AutoResetWaitableEvent ready_latch, message_latch;
  context.AddFfiNativeCallback(
      "SignalNativeTest",
      CREATE_FFI_LAMBDA([&ready_latch]() { ready_latch.Signal(); }));

  std::string message;
  context.AddFfiNativeCallback(
      "SignalNativeMessage", CREATE_FFI_LAMBDA([&](Dart_Handle message_handle) {
        message = tonic::DartConverter<std::string>::FromDart(message_handle);
        message_latch.Signal();
      }));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  ready_latch.Wait();

  // Add view 123.
  struct Captures {
    std::atomic<int> count = 0;
    fml::AutoResetWaitableEvent failure_latch;
  };
  Captures captures;

  FlutterWindowMetricsEvent metrics = {};
  metrics.struct_size = sizeof(FlutterWindowMetricsEvent);
  metrics.width = 800;
  metrics.height = 600;
  metrics.pixel_ratio = 1.0;
  metrics.view_id = 123;

  FlutterAddViewInfo add_info = {};
  add_info.struct_size = sizeof(FlutterAddViewInfo);
  add_info.view_id = 123;
  add_info.view_metrics = &metrics;
  add_info.user_data = &captures;
  add_info.add_view_callback = [](const FlutterAddViewResult* result) {
    auto captures = reinterpret_cast<Captures*>(result->user_data);

    int count = captures->count.fetch_add(1);

    if (count == 0) {
      ASSERT_TRUE(result->added);
    } else {
      EXPECT_FALSE(result->added);
      captures->failure_latch.Signal();
    }
  };
  ASSERT_EQ(FlutterEngineAddView(engine.get(), &add_info), kSuccess);
  message_latch.Wait();
  ASSERT_EQ(message, "View IDs: [0, 123]");
  ASSERT_FALSE(captures.failure_latch.IsSignaledForTest());

  // Add view 123 a second time.
  ASSERT_EQ(FlutterEngineAddView(engine.get(), &add_info), kSuccess);
  captures.failure_latch.Wait();
  ASSERT_EQ(captures.count, 2);
  ASSERT_FALSE(message_latch.IsSignaledForTest());
}

//------------------------------------------------------------------------------
/// Test that a removed view's ID can be reused to add a new view.
///
TEST_F(EmbedderTest, CanReuseViewIds) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("window_metrics_event_all_view_ids");

  fml::AutoResetWaitableEvent ready_latch, message_latch;
  context.AddFfiNativeCallback(
      "SignalNativeTest",
      CREATE_FFI_LAMBDA([&ready_latch]() { ready_latch.Signal(); }));

  std::string message;
  context.AddFfiNativeCallback(
      "SignalNativeMessage", CREATE_FFI_LAMBDA([&](Dart_Handle message_handle) {
        message = tonic::DartConverter<std::string>::FromDart(message_handle);
        message_latch.Signal();
      }));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  ready_latch.Wait();

  // Add view 123.
  FlutterWindowMetricsEvent metrics = {};
  metrics.struct_size = sizeof(FlutterWindowMetricsEvent);
  metrics.width = 800;
  metrics.height = 600;
  metrics.pixel_ratio = 1.0;
  metrics.view_id = 123;

  FlutterAddViewInfo add_info = {};
  add_info.struct_size = sizeof(FlutterAddViewInfo);
  add_info.view_id = 123;
  add_info.view_metrics = &metrics;
  add_info.add_view_callback = [](const FlutterAddViewResult* result) {
    ASSERT_TRUE(result->added);
  };
  ASSERT_EQ(FlutterEngineAddView(engine.get(), &add_info), kSuccess);
  message_latch.Wait();
  ASSERT_EQ(message, "View IDs: [0, 123]");

  // Remove view 123.
  FlutterRemoveViewInfo remove_info = {};
  remove_info.struct_size = sizeof(FlutterAddViewInfo);
  remove_info.view_id = 123;
  remove_info.remove_view_callback = [](const FlutterRemoveViewResult* result) {
    ASSERT_TRUE(result->removed);
  };
  ASSERT_EQ(FlutterEngineRemoveView(engine.get(), &remove_info), kSuccess);
  message_latch.Wait();
  ASSERT_EQ(message, "View IDs: [0]");

  // Re-add view 123.
  ASSERT_EQ(FlutterEngineAddView(engine.get(), &add_info), kSuccess);
  message_latch.Wait();
  ASSERT_EQ(message, "View IDs: [0, 123]");
}

//------------------------------------------------------------------------------
/// Test that attempting to remove a view that does not exist fails as expected.
///
TEST_F(EmbedderTest, CannotRemoveUnknownView) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  fml::AutoResetWaitableEvent latch;
  FlutterRemoveViewInfo info = {};
  info.struct_size = sizeof(FlutterRemoveViewInfo);
  info.view_id = 123;
  info.user_data = &latch;
  info.remove_view_callback = [](const FlutterRemoveViewResult* result) {
    EXPECT_FALSE(result->removed);
    reinterpret_cast<fml::AutoResetWaitableEvent*>(result->user_data)->Signal();
  };
  ASSERT_EQ(FlutterEngineRemoveView(engine.get(), &info), kSuccess);
  latch.Wait();
}

//------------------------------------------------------------------------------
/// View operations - adding, removing, sending window metrics - must execute in
/// order even though they are asynchronous. This is necessary to ensure the
/// embedder's and engine's states remain synchronized.
///
TEST_F(EmbedderTest, ViewOperationsOrdered) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("window_metrics_event_all_view_ids");

  fml::AutoResetWaitableEvent ready_latch;
  context.AddFfiNativeCallback(
      "SignalNativeTest",
      CREATE_FFI_LAMBDA([&ready_latch]() { ready_latch.Signal(); }));

  std::atomic<int> message_count = 0;
  context.AddFfiNativeCallback(
      "SignalNativeMessage", CREATE_FFI_LAMBDA([&](Dart_Handle message_handle) {
        message_count.fetch_add(1);
      }));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  ready_latch.Wait();

  // Enqueue multiple view operations at once:
  //
  // 1. Add view 123 - This must succeed.
  // 2. Add duplicate view 123 - This must fail asynchronously.
  // 3. Add second view 456 - This must succeed.
  // 4. Remove second view 456 - This must succeed.
  //
  // The engine must execute view operations asynchronously in serial order.
  // If step 2 succeeds instead of step 1, this indicates the engine did not
  // execute the view operations in the correct order. If step 4 fails,
  // this indicates the engine did not wait until the add second view completed.
  FlutterWindowMetricsEvent metrics123 = {};
  metrics123.struct_size = sizeof(FlutterWindowMetricsEvent);
  metrics123.width = 800;
  metrics123.height = 600;
  metrics123.pixel_ratio = 1.0;
  metrics123.view_id = 123;

  FlutterWindowMetricsEvent metrics456 = {};
  metrics456.struct_size = sizeof(FlutterWindowMetricsEvent);
  metrics456.width = 800;
  metrics456.height = 600;
  metrics456.pixel_ratio = 1.0;
  metrics456.view_id = 456;

  struct Captures {
    fml::AutoResetWaitableEvent add_first_view;
    fml::AutoResetWaitableEvent add_duplicate_view;
    fml::AutoResetWaitableEvent add_second_view;
    fml::AutoResetWaitableEvent remove_second_view;
  };
  Captures captures;

  // Add view 123.
  FlutterAddViewInfo add_view_info = {};
  add_view_info.struct_size = sizeof(FlutterAddViewInfo);
  add_view_info.view_id = 123;
  add_view_info.view_metrics = &metrics123;
  add_view_info.user_data = &captures;
  add_view_info.add_view_callback = [](const FlutterAddViewResult* result) {
    auto captures = reinterpret_cast<Captures*>(result->user_data);

    ASSERT_TRUE(result->added);
    ASSERT_FALSE(captures->add_first_view.IsSignaledForTest());
    ASSERT_FALSE(captures->add_duplicate_view.IsSignaledForTest());
    ASSERT_FALSE(captures->add_second_view.IsSignaledForTest());
    ASSERT_FALSE(captures->remove_second_view.IsSignaledForTest());

    captures->add_first_view.Signal();
  };

  // Add duplicate view 123.
  FlutterAddViewInfo add_duplicate_view_info = {};
  add_duplicate_view_info.struct_size = sizeof(FlutterAddViewInfo);
  add_duplicate_view_info.view_id = 123;
  add_duplicate_view_info.view_metrics = &metrics123;
  add_duplicate_view_info.user_data = &captures;
  add_duplicate_view_info.add_view_callback =
      [](const FlutterAddViewResult* result) {
        auto captures = reinterpret_cast<Captures*>(result->user_data);

        ASSERT_FALSE(result->added);
        ASSERT_TRUE(captures->add_first_view.IsSignaledForTest());
        ASSERT_FALSE(captures->add_duplicate_view.IsSignaledForTest());
        ASSERT_FALSE(captures->add_second_view.IsSignaledForTest());
        ASSERT_FALSE(captures->remove_second_view.IsSignaledForTest());

        captures->add_duplicate_view.Signal();
      };

  // Add view 456.
  FlutterAddViewInfo add_second_view_info = {};
  add_second_view_info.struct_size = sizeof(FlutterAddViewInfo);
  add_second_view_info.view_id = 456;
  add_second_view_info.view_metrics = &metrics456;
  add_second_view_info.user_data = &captures;
  add_second_view_info.add_view_callback =
      [](const FlutterAddViewResult* result) {
        auto captures = reinterpret_cast<Captures*>(result->user_data);

        ASSERT_TRUE(result->added);
        ASSERT_TRUE(captures->add_first_view.IsSignaledForTest());
        ASSERT_TRUE(captures->add_duplicate_view.IsSignaledForTest());
        ASSERT_FALSE(captures->add_second_view.IsSignaledForTest());
        ASSERT_FALSE(captures->remove_second_view.IsSignaledForTest());

        captures->add_second_view.Signal();
      };

  // Remove view 456.
  FlutterRemoveViewInfo remove_second_view_info = {};
  remove_second_view_info.struct_size = sizeof(FlutterRemoveViewInfo);
  remove_second_view_info.view_id = 456;
  remove_second_view_info.user_data = &captures;
  remove_second_view_info.remove_view_callback =
      [](const FlutterRemoveViewResult* result) {
        auto captures = reinterpret_cast<Captures*>(result->user_data);

        ASSERT_TRUE(result->removed);
        ASSERT_TRUE(captures->add_first_view.IsSignaledForTest());
        ASSERT_TRUE(captures->add_duplicate_view.IsSignaledForTest());
        ASSERT_TRUE(captures->add_second_view.IsSignaledForTest());
        ASSERT_FALSE(captures->remove_second_view.IsSignaledForTest());

        captures->remove_second_view.Signal();
      };

  ASSERT_EQ(FlutterEngineAddView(engine.get(), &add_view_info), kSuccess);
  ASSERT_EQ(FlutterEngineAddView(engine.get(), &add_duplicate_view_info),
            kSuccess);
  ASSERT_EQ(FlutterEngineAddView(engine.get(), &add_second_view_info),
            kSuccess);
  ASSERT_EQ(FlutterEngineRemoveView(engine.get(), &remove_second_view_info),
            kSuccess);
  captures.remove_second_view.Wait();
  captures.add_second_view.Wait();
  captures.add_duplicate_view.Wait();
  captures.add_first_view.Wait();
  ASSERT_EQ(message_count, 3);
}

//------------------------------------------------------------------------------
/// Test the engine can present to multiple views.
///
TEST_F(EmbedderTest, CanRenderMultipleViews) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetCompositor();
  builder.SetDartEntrypoint("render_all_views");

  builder.SetRenderTargetType(
      EmbedderTestBackingStoreProducer::RenderTargetType::kSoftwareBuffer);

  fml::AutoResetWaitableEvent latch0, latch123;
  context.GetCompositor().SetPresentCallback(
      [&](FlutterViewId view_id, const FlutterLayer** layers,
          size_t layers_count) {
        switch (view_id) {
          case 0:
            latch0.Signal();
            break;
          case 123:
            latch123.Signal();
            break;
          default:
            FML_UNREACHABLE();
        }
      },
      /* one_shot= */ false);

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  // Give the implicit view a non-zero size so that it renders something.
  FlutterWindowMetricsEvent metrics0 = {};
  metrics0.struct_size = sizeof(FlutterWindowMetricsEvent);
  metrics0.width = 800;
  metrics0.height = 600;
  metrics0.pixel_ratio = 1.0;
  metrics0.view_id = 0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &metrics0),
            kSuccess);

  // Add view 123.
  FlutterWindowMetricsEvent metrics123 = {};
  metrics123.struct_size = sizeof(FlutterWindowMetricsEvent);
  metrics123.width = 800;
  metrics123.height = 600;
  metrics123.pixel_ratio = 1.0;
  metrics123.view_id = 123;

  FlutterAddViewInfo add_view_info = {};
  add_view_info.struct_size = sizeof(FlutterAddViewInfo);
  add_view_info.view_id = 123;
  add_view_info.view_metrics = &metrics123;
  add_view_info.add_view_callback = [](const FlutterAddViewResult* result) {
    ASSERT_TRUE(result->added);
  };

  ASSERT_EQ(FlutterEngineAddView(engine.get(), &add_view_info), kSuccess);

  latch0.Wait();
  latch123.Wait();
}

bool operator==(const FlutterViewFocusChangeRequest& lhs,
                const FlutterViewFocusChangeRequest& rhs) {
  return lhs.view_id == rhs.view_id && lhs.state == rhs.state &&
         lhs.direction == rhs.direction;
}

TEST_F(EmbedderTest, SendsViewFocusChangeRequest) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  auto platform_task_runner = CreateNewThread("test_platform_thread");
  UniqueEngine engine;
  static std::mutex engine_mutex;
  EmbedderTestTaskRunner test_platform_task_runner(
      platform_task_runner, [&](FlutterTask task) {
        std::scoped_lock lock(engine_mutex);
        if (!engine.is_valid()) {
          return;
        }
        FlutterEngineRunTask(engine.get(), &task);
      });
  fml::CountDownLatch latch(3);
  std::vector<FlutterViewFocusChangeRequest> received_requests;
  platform_task_runner->PostTask([&]() {
    EmbedderConfigBuilder builder(context);
    builder.SetSurface(DlISize(1, 1));
    builder.SetDartEntrypoint("testSendViewFocusChangeRequest");
    const auto platform_task_runner_description =
        test_platform_task_runner.GetFlutterTaskRunnerDescription();
    builder.SetPlatformTaskRunner(&platform_task_runner_description);
    builder.SetViewFocusChangeRequestCallback(
        [&](const FlutterViewFocusChangeRequest* request) {
          EXPECT_TRUE(platform_task_runner->RunsTasksOnCurrentThread());
          received_requests.push_back(*request);
          latch.CountDown();
        });
    engine = builder.LaunchEngine();
    ASSERT_TRUE(engine.is_valid());
  });
  latch.Wait();

  std::vector<FlutterViewFocusChangeRequest> expected_requests{
      {.view_id = 1, .state = kUnfocused, .direction = kUndefined},
      {.view_id = 2, .state = kFocused, .direction = kForward},
      {.view_id = 3, .state = kFocused, .direction = kBackward},
  };

  ASSERT_EQ(received_requests.size(), expected_requests.size());
  for (size_t i = 0; i < received_requests.size(); ++i) {
    ASSERT_TRUE(received_requests[i] == expected_requests[i]);
  }

  fml::AutoResetWaitableEvent kill_latch;
  platform_task_runner->PostTask(fml::MakeCopyable([&]() mutable {
    std::scoped_lock lock(engine_mutex);
    engine.reset();

    // There may still be pending tasks on the platform thread that were queued
    // by the test_task_runner.  Signal the latch after these tasks have been
    // consumed.
    platform_task_runner->PostTask([&kill_latch] { kill_latch.Signal(); });
  }));
  kill_latch.Wait();
}

TEST_F(EmbedderTest, CanSendViewFocusEvent) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("testSendViewFocusEvent");

  fml::AutoResetWaitableEvent latch;
  std::string last_event;

  context.AddFfiNativeCallback(
      "SignalNativeTest", CREATE_FFI_LAMBDA([&latch]() { latch.Signal(); }));
  context.AddFfiNativeCallback(
      "NotifyStringValue", CREATE_FFI_LAMBDA([&](Dart_Handle value) {
        const auto message_from_dart =
            tonic::DartConverter<std::string>::FromDart(value);
        last_event = message_from_dart;
        latch.Signal();
      }));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());
  // Wait until the focus change handler is attached.
  latch.Wait();
  latch.Reset();

  FlutterViewFocusEvent event1{
      .struct_size = sizeof(FlutterViewFocusEvent),
      .view_id = 1,
      .state = kFocused,
      .direction = kUndefined,
  };
  FlutterEngineResult result =
      FlutterEngineSendViewFocusEvent(engine.get(), &event1);
  ASSERT_EQ(result, kSuccess);
  latch.Wait();
  ASSERT_EQ(last_event,
            "1 ViewFocusState.focused ViewFocusDirection.undefined");

  FlutterViewFocusEvent event2{
      .struct_size = sizeof(FlutterViewFocusEvent),
      .view_id = 2,
      .state = kUnfocused,
      .direction = kBackward,
  };
  latch.Reset();
  result = FlutterEngineSendViewFocusEvent(engine.get(), &event2);
  ASSERT_EQ(result, kSuccess);
  latch.Wait();
  ASSERT_EQ(last_event,
            "2 ViewFocusState.unfocused ViewFocusDirection.backward");
}

//------------------------------------------------------------------------------
/// Test that the backing store is created with the correct view ID, is used
/// for the correct view, and is cached according to their views.
///
/// The test involves two frames:
/// 1. The first frame renders the implicit view and the second view.
/// 2. The second frame renders the implicit view and the third view.
///
/// The test verifies that:
/// - Each backing store is created with a valid view ID.
/// - Each backing store is presented for the view that it was created for.
/// - Both frames render the expected sets of views.
/// - By the end of frame 1, only 2 backing stores were created.
/// - By the end of frame 2, only 3 backing stores were created. This ensures
/// that the backing store for the 2nd view is not reused for the 3rd view.
TEST_F(EmbedderTest, BackingStoresCorrespondToTheirViews) {
  constexpr FlutterViewId kSecondViewId = 123;
  constexpr FlutterViewId kThirdViewId = 456;
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  EmbedderConfigBuilder builder(context);
  builder.SetDartEntrypoint("render_all_views");
  builder.SetSurface(DlISize(800, 600));
  builder.SetCompositor();

  EmbedderTestBackingStoreProducerSoftware producer(
      context.GetCompositor().GetGrContext(),
      EmbedderTestBackingStoreProducer::RenderTargetType::kSoftwareBuffer);

  // The variables needed by the callbacks of the compositor.
  struct CompositorUserData {
    EmbedderTestBackingStoreProducer* producer;
    // Each latch is signaled when its corresponding view is presented.
    fml::AutoResetWaitableEvent latch_implicit;
    fml::AutoResetWaitableEvent latch_second;
    fml::AutoResetWaitableEvent latch_third;
    // Whether the respective view should be rendered in the frame.
    bool second_expected;
    bool third_expected;
    // The total number of backing stores created to verify caching.
    int backing_stores_created;
  };
  CompositorUserData compositor_user_data{
      .producer = &producer,
      .backing_stores_created = 0,
  };

  builder.GetCompositor() = FlutterCompositor{
      .struct_size = sizeof(FlutterCompositor),
      .user_data = reinterpret_cast<void*>(&compositor_user_data),
      .create_backing_store_callback =
          [](const FlutterBackingStoreConfig* config,
             FlutterBackingStore* backing_store_out, void* user_data) {
            // Verify that the backing store comes with the correct view ID.
            EXPECT_TRUE(config->view_id == 0 ||
                        config->view_id == kSecondViewId ||
                        config->view_id == kThirdViewId);
            auto compositor_user_data =
                reinterpret_cast<CompositorUserData*>(user_data);
            compositor_user_data->backing_stores_created += 1;
            bool result = compositor_user_data->producer->Create(
                config, backing_store_out);
            // The created backing store has a user_data that records the view
            // that the store is created for.
            backing_store_out->user_data =
                reinterpret_cast<void*>(config->view_id);
            return result;
          },
      .collect_backing_store_callback = [](const FlutterBackingStore* renderer,
                                           void* user_data) { return true; },
      .present_layers_callback = nullptr,
      .avoid_backing_store_cache = false,
      .present_view_callback =
          [](const FlutterPresentViewInfo* info) {
            EXPECT_EQ(info->layers_count, 1u);
            // Verify that the given layer's backing store has the same view ID
            // as the target view.
            int64_t store_view_id = reinterpret_cast<int64_t>(
                info->layers[0]->backing_store->user_data);
            EXPECT_EQ(store_view_id, info->view_id);
            auto compositor_user_data =
                reinterpret_cast<CompositorUserData*>(info->user_data);
            // Verify that the respective views are rendered.
            switch (info->view_id) {
              case 0:
                compositor_user_data->latch_implicit.Signal();
                break;
              case kSecondViewId:
                EXPECT_TRUE(compositor_user_data->second_expected);
                compositor_user_data->latch_second.Signal();
                break;
              case kThirdViewId:
                EXPECT_TRUE(compositor_user_data->third_expected);
                compositor_user_data->latch_third.Signal();
                break;
              default:
                FML_UNREACHABLE();
            }
            return true;
          },
  };

  compositor_user_data.second_expected = true;
  compositor_user_data.third_expected = false;

  /*=== First frame ===*/

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  // Give the implicit view a non-zero size so that it renders something.
  FlutterWindowMetricsEvent metrics_implicit = {
      .struct_size = sizeof(FlutterWindowMetricsEvent),
      .width = 800,
      .height = 600,
      .pixel_ratio = 1.0,
      .view_id = 0,
  };
  ASSERT_EQ(
      FlutterEngineSendWindowMetricsEvent(engine.get(), &metrics_implicit),
      kSuccess);

  // Add the second view.
  FlutterWindowMetricsEvent metrics_add = {
      .struct_size = sizeof(FlutterWindowMetricsEvent),
      .width = 800,
      .height = 600,
      .pixel_ratio = 1.0,
      .view_id = kSecondViewId,
  };

  FlutterAddViewInfo add_view_info = {};
  add_view_info.struct_size = sizeof(FlutterAddViewInfo);
  add_view_info.view_id = kSecondViewId;
  add_view_info.view_metrics = &metrics_add;
  add_view_info.add_view_callback = [](const FlutterAddViewResult* result) {
    ASSERT_TRUE(result->added);
  };

  ASSERT_EQ(FlutterEngineAddView(engine.get(), &add_view_info), kSuccess);

  compositor_user_data.latch_implicit.Wait();
  compositor_user_data.latch_second.Wait();

  /*=== Second frame ===*/

  compositor_user_data.second_expected = false;
  compositor_user_data.third_expected = true;
  EXPECT_EQ(compositor_user_data.backing_stores_created, 2);

  // Remove the second view
  FlutterRemoveViewInfo remove_view_info = {};
  remove_view_info.struct_size = sizeof(FlutterRemoveViewInfo);
  remove_view_info.view_id = kSecondViewId;
  remove_view_info.remove_view_callback =
      [](const FlutterRemoveViewResult* result) {
        ASSERT_TRUE(result->removed);
      };
  ASSERT_EQ(FlutterEngineRemoveView(engine.get(), &remove_view_info), kSuccess);

  // Add the third view.
  add_view_info.view_id = kThirdViewId;
  metrics_add.view_id = kThirdViewId;
  ASSERT_EQ(FlutterEngineAddView(engine.get(), &add_view_info), kSuccess);
  // Adding the view should have scheduled a frame.

  compositor_user_data.latch_implicit.Wait();
  compositor_user_data.latch_third.Wait();
  EXPECT_EQ(compositor_user_data.backing_stores_created, 3);
}

TEST_F(EmbedderTest, CanUpdateLocales) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("can_receive_locale_updates");
  fml::AutoResetWaitableEvent latch;
  context.AddFfiNativeCallback(
      "SignalNativeTest", CREATE_FFI_LAMBDA([&latch]() { latch.Signal(); }));

  fml::AutoResetWaitableEvent check_latch;
  context.AddFfiNativeCallback("SignalNativeCount",
                               CREATE_FFI_LAMBDA([&check_latch](int count) {
                                 ASSERT_EQ(count, 2);
                                 check_latch.Signal();
                               }));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  // Wait for the application to attach the listener.
  latch.Wait();

  FlutterLocale locale1 = {};
  locale1.struct_size = sizeof(locale1);
  locale1.language_code = "";  // invalid
  locale1.country_code = "US";
  locale1.script_code = "";
  locale1.variant_code = nullptr;

  FlutterLocale locale2 = {};
  locale2.struct_size = sizeof(locale2);
  locale2.language_code = "zh";
  locale2.country_code = "CN";
  locale2.script_code = "Hans";
  locale2.variant_code = nullptr;

  std::vector<const FlutterLocale*> locales;
  locales.push_back(&locale1);
  locales.push_back(&locale2);

  ASSERT_EQ(
      FlutterEngineUpdateLocales(engine.get(), locales.data(), locales.size()),
      kInvalidArguments);

  // Fix the invalid code.
  locale1.language_code = "en";

  ASSERT_EQ(
      FlutterEngineUpdateLocales(engine.get(), locales.data(), locales.size()),
      kSuccess);

  check_latch.Wait();
}

TEST_F(EmbedderTest, LocalizationCallbacksCalled) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  fml::AutoResetWaitableEvent latch;
  context.AddIsolateCreateCallback([&latch]() { latch.Signal(); });
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());
  // Wait for the root isolate to launch.
  latch.Wait();

  flutter::Shell& shell = ToEmbedderEngine(engine.get())->GetShell();
  std::vector<std::string> supported_locales;
  supported_locales.push_back("es");
  supported_locales.push_back("MX");
  supported_locales.push_back("");
  auto result = shell.GetPlatformView()->ComputePlatformResolvedLocales(
      supported_locales);

  ASSERT_EQ((*result).size(), supported_locales.size());  // 3
  ASSERT_EQ((*result)[0], supported_locales[0]);
  ASSERT_EQ((*result)[1], supported_locales[1]);
  ASSERT_EQ((*result)[2], supported_locales[2]);

  engine.reset();
}

TEST_F(EmbedderTest, CanQueryDartAOTMode) {
  ASSERT_EQ(FlutterEngineRunsAOTCompiledDartCode(),
            flutter::DartVM::IsRunningPrecompiledCode());
}

TEST_F(EmbedderTest, VerifyB143464703WithSoftwareBackend) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1024, 600));
  builder.SetCompositor();
  builder.SetDartEntrypoint("verify_b143464703");

  builder.SetRenderTargetType(
      EmbedderTestBackingStoreProducer::RenderTargetType::kSoftwareBuffer);

  // setup the screenshot promise.
  auto rendered_scene = context.GetNextSceneImage();

  fml::CountDownLatch latch(1);
  context.GetCompositor().SetNextPresentCallback(
      [&](FlutterViewId view_id, const FlutterLayer** layers,
          size_t layers_count) {
        ASSERT_EQ(layers_count, 2u);

        // Layer 0 (Root)
        {
          FlutterBackingStore backing_store = *layers[0]->backing_store;
          backing_store.type = kFlutterBackingStoreTypeSoftware;
          backing_store.did_update = true;

          FlutterRect paint_region_rects[] = {
              FlutterRectMakeLTRB(0, 0, 1024, 600),
          };
          FlutterRegion paint_region = {
              .struct_size = sizeof(FlutterRegion),
              .rects_count = 1,
              .rects = paint_region_rects,
          };
          FlutterBackingStorePresentInfo present_info = {
              .struct_size = sizeof(FlutterBackingStorePresentInfo),
              .paint_region = &paint_region,
          };

          FlutterLayer layer = {};
          layer.struct_size = sizeof(layer);
          layer.type = kFlutterLayerContentTypeBackingStore;
          layer.backing_store = &backing_store;
          layer.size = FlutterSizeMake(1024.0, 600.0);
          layer.offset = FlutterPointMake(0.0, 0.0);
          layer.backing_store_present_info = &present_info;

          ASSERT_EQ(*layers[0], layer);
        }

        // Layer 1
        {
          FlutterPlatformView platform_view = *layers[1]->platform_view;
          platform_view.struct_size = sizeof(platform_view);
          platform_view.identifier = 42;

          FlutterLayer layer = {};
          layer.struct_size = sizeof(layer);
          layer.type = kFlutterLayerContentTypePlatformView;
          layer.platform_view = &platform_view;
          layer.size = FlutterSizeMake(1024.0, 540.0);
          layer.offset = FlutterPointMake(135.0, 60.0);

          ASSERT_EQ(*layers[1], layer);
        }

        latch.CountDown();
      });

  context.GetCompositor().SetPlatformViewRendererCallback(
      [](const FlutterLayer& layer,
         GrDirectContext* context) -> sk_sp<SkImage> {
        auto surface = CreateRenderSurface(
            layer, nullptr /* null because software compositor */);
        auto canvas = surface->getCanvas();
        FML_CHECK(canvas != nullptr);

        switch (layer.platform_view->identifier) {
          case 42: {
            SkPaint paint;
            // See dart test for total order.
            paint.setColor(SK_ColorGREEN);
            paint.setAlpha(127);
            const auto& rect =
                SkRect::MakeWH(layer.size.width, layer.size.height);
            canvas->drawRect(rect, paint);
          } break;
          default:
            // Asked to render an unknown platform view.
            FML_CHECK(false)
                << "Test was asked to composite an unknown platform view.";
        }

        return surface->makeImageSnapshot();
      });

  auto engine = builder.LaunchEngine();

  // Send a window metrics events so frames may be scheduled.
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 1024;
  event.height = 600;
  event.pixel_ratio = 1.0;
  event.physical_view_inset_top = 0.0;
  event.physical_view_inset_right = 0.0;
  event.physical_view_inset_bottom = 0.0;
  event.physical_view_inset_left = 0.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);
  ASSERT_TRUE(engine.is_valid());

  // wait for scene to be rendered.
  latch.Wait();

  // TODO(https://github.com/flutter/flutter/issues/53784): enable this on all
  // platforms.
#if !defined(FML_OS_LINUX)
  GTEST_SKIP() << "Skipping golden tests on non-Linux OSes";
#endif  // FML_OS_LINUX
  ASSERT_TRUE(
      ImageMatchesFixture("verifyb143464703_soft_noxform.png", rendered_scene));
}

TEST_F(EmbedderTest, CanSendLowMemoryNotification) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));

  auto engine = builder.LaunchEngine();

  ASSERT_TRUE(engine.is_valid());

  // TODO(chinmaygarde): The shell ought to have a mechanism for notification
  // dispatch that engine subsystems can register handlers to. This would allow
  // the raster cache and the secondary context caches to respond to
  // notifications. Once that is in place, this test can be updated to actually
  // ensure that the dispatched message is visible to engine subsystems.
  ASSERT_EQ(FlutterEngineNotifyLowMemoryWarning(engine.get()), kSuccess);
}

TEST_F(EmbedderTest, CanPostTaskToAllNativeThreads) {
  UniqueEngine engine;
  size_t worker_count = 0;
  fml::AutoResetWaitableEvent sync_latch;

  // One of the threads that the callback will be posted to is the platform
  // thread. So we cannot wait for assertions to complete on the platform
  // thread. Create a new thread to manage the engine instance and wait for
  // assertions on the test thread.
  auto platform_task_runner = CreateNewThread("platform_thread");

  platform_task_runner->PostTask([&]() {
    auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
    EmbedderConfigBuilder builder(context);
    builder.SetSurface(DlISize(1, 1));

    engine = builder.LaunchEngine();

    ASSERT_TRUE(engine.is_valid());

    worker_count = ToEmbedderEngine(engine.get())
                       ->GetShell()
                       .GetDartVM()
                       ->GetConcurrentMessageLoop()
                       ->GetWorkerCount();

    sync_latch.Signal();
  });

  sync_latch.Wait();

  const auto engine_threads_count = worker_count + 4u;

  struct Captures {
    // Waits the adequate number of callbacks to fire.
    fml::CountDownLatch latch;

    // This class will be accessed from multiple threads concurrently to track
    // thread specific information that is later checked. All updates to fields
    // in this struct must be made with this mutex acquired.

    std::mutex captures_mutex;
    // Ensures that the expect number of distinct threads were serviced.
    std::set<std::thread::id> thread_ids;

    size_t platform_threads_count = 0;
    size_t render_threads_count = 0;
    size_t ui_threads_count = 0;
    size_t worker_threads_count = 0;

    explicit Captures(size_t count) : latch(count) {}
  };

  Captures captures(engine_threads_count);

  platform_task_runner->PostTask([&]() {
    ASSERT_EQ(FlutterEnginePostCallbackOnAllNativeThreads(
                  engine.get(),
                  [](FlutterNativeThreadType type, void* baton) {
                    auto captures = reinterpret_cast<Captures*>(baton);
                    {
                      std::scoped_lock lock(captures->captures_mutex);
                      switch (type) {
                        case kFlutterNativeThreadTypeRender:
                          captures->render_threads_count++;
                          break;
                        case kFlutterNativeThreadTypeWorker:
                          captures->worker_threads_count++;
                          break;
                        case kFlutterNativeThreadTypeUI:
                          captures->ui_threads_count++;
                          break;
                        case kFlutterNativeThreadTypePlatform:
                          captures->platform_threads_count++;
                          break;
                      }
                      captures->thread_ids.insert(std::this_thread::get_id());
                    }
                    captures->latch.CountDown();
                  },
                  &captures),
              kSuccess);
  });

  captures.latch.Wait();
  ASSERT_EQ(captures.thread_ids.size(), engine_threads_count);
  ASSERT_EQ(captures.platform_threads_count, 1u);
  ASSERT_EQ(captures.render_threads_count, 1u);
  ASSERT_EQ(captures.ui_threads_count, 1u);
  ASSERT_EQ(captures.worker_threads_count, worker_count + 1u /* for IO */);
  EXPECT_GE(captures.worker_threads_count - 1, 2u);
  EXPECT_LE(captures.worker_threads_count - 1, 4u);

  platform_task_runner->PostTask([&]() {
    engine.reset();
    sync_latch.Signal();
  });
  sync_latch.Wait();

  // The engine should have already been destroyed on the platform task runner.
  ASSERT_FALSE(engine.is_valid());
}

TEST_F(EmbedderTest, InvalidAOTDataSourcesMustReturnError) {
  if (!DartVM::IsRunningPrecompiledCode()) {
    GTEST_SKIP();
    return;
  }
  FlutterEngineAOTDataSource data_in = {};
  FlutterEngineAOTData data_out = nullptr;

  // Null source specified.
  ASSERT_EQ(FlutterEngineCreateAOTData(nullptr, &data_out), kInvalidArguments);
  ASSERT_EQ(data_out, nullptr);

  // Null data_out specified.
  ASSERT_EQ(FlutterEngineCreateAOTData(&data_in, nullptr), kInvalidArguments);

  // Invalid FlutterEngineAOTDataSourceType type specified.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  data_in.type = static_cast<FlutterEngineAOTDataSourceType>(-1);
  ASSERT_EQ(FlutterEngineCreateAOTData(&data_in, &data_out), kInvalidArguments);
  ASSERT_EQ(data_out, nullptr);

  // Invalid ELF path specified.
  data_in.type = kFlutterEngineAOTDataSourceTypeElfPath;
  data_in.elf_path = nullptr;
  ASSERT_EQ(FlutterEngineCreateAOTData(&data_in, &data_out), kInvalidArguments);
  ASSERT_EQ(data_in.type, kFlutterEngineAOTDataSourceTypeElfPath);
  ASSERT_EQ(data_in.elf_path, nullptr);
  ASSERT_EQ(data_out, nullptr);

  // Invalid ELF path specified.
  data_in.elf_path = "";
  ASSERT_EQ(FlutterEngineCreateAOTData(&data_in, &data_out), kInvalidArguments);
  ASSERT_EQ(data_in.type, kFlutterEngineAOTDataSourceTypeElfPath);
  ASSERT_EQ(data_in.elf_path, "");
  ASSERT_EQ(data_out, nullptr);

  // Could not find VM snapshot data.
  data_in.elf_path = "/bin/true";
  ASSERT_EQ(FlutterEngineCreateAOTData(&data_in, &data_out), kInvalidArguments);
  ASSERT_EQ(data_in.type, kFlutterEngineAOTDataSourceTypeElfPath);
  ASSERT_EQ(data_in.elf_path, "/bin/true");
  ASSERT_EQ(data_out, nullptr);
}

TEST_F(EmbedderTest, MustNotRunWithMultipleAOTSources) {
  if (!DartVM::IsRunningPrecompiledCode()) {
    GTEST_SKIP();
    return;
  }
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  EmbedderConfigBuilder builder(
      context,
      EmbedderConfigBuilder::InitializationPreference::kMultiAOTInitialize);

  builder.SetSurface(DlISize(1, 1));

  auto engine = builder.LaunchEngine();
  ASSERT_FALSE(engine.is_valid());
}

TEST_F(EmbedderTest, CanCreateAndCollectAValidElfSource) {
  if (!DartVM::IsRunningPrecompiledCode()) {
    GTEST_SKIP();
    return;
  }
  FlutterEngineAOTDataSource data_in = {};
  FlutterEngineAOTData data_out = nullptr;

  // Collecting a null object should be allowed
  ASSERT_EQ(FlutterEngineCollectAOTData(data_out), kSuccess);

  const auto elf_path =
      fml::paths::JoinPaths({GetFixturesPath(), kDefaultAOTAppELFFileName});

  data_in.type = kFlutterEngineAOTDataSourceTypeElfPath;
  data_in.elf_path = elf_path.c_str();

  ASSERT_EQ(FlutterEngineCreateAOTData(&data_in, &data_out), kSuccess);
  ASSERT_EQ(data_in.type, kFlutterEngineAOTDataSourceTypeElfPath);
  ASSERT_EQ(data_in.elf_path, elf_path.c_str());
  ASSERT_NE(data_out, nullptr);

  ASSERT_EQ(FlutterEngineCollectAOTData(data_out), kSuccess);
}

TEST_F(EmbedderTest, CanLaunchAndShutdownWithAValidElfSource) {
  if (!DartVM::IsRunningPrecompiledCode()) {
    GTEST_SKIP();
    return;
  }
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  fml::AutoResetWaitableEvent latch;
  context.AddIsolateCreateCallback([&latch]() { latch.Signal(); });

  EmbedderConfigBuilder builder(
      context,
      EmbedderConfigBuilder::InitializationPreference::kAOTDataInitialize);

  builder.SetSurface(DlISize(1, 1));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  // Wait for the root isolate to launch.
  latch.Wait();
  engine.reset();
}

#if defined(__clang_analyzer__)
#define TEST_VM_SNAPSHOT_DATA "vm_data"
#define TEST_VM_SNAPSHOT_INSTRUCTIONS "vm_instructions"
#define TEST_ISOLATE_SNAPSHOT_DATA "isolate_data"
#define TEST_ISOLATE_SNAPSHOT_INSTRUCTIONS "isolate_instructions"
#endif

//------------------------------------------------------------------------------
/// PopulateJITSnapshotMappingCallbacks should successfully change the callbacks
/// of the snapshots in the engine's settings when JIT snapshots are explicitly
/// defined.
///
TEST_F(EmbedderTest, CanSuccessfullyPopulateSpecificJITSnapshotCallbacks) {
// TODO(#107263): Inconsistent snapshot paths in the Linux Fuchsia FEMU test.
#if defined(OS_FUCHSIA)
  GTEST_SKIP() << "Inconsistent paths in Fuchsia.";
#else

  // This test is only relevant in JIT mode.
  if (DartVM::IsRunningPrecompiledCode()) {
    GTEST_SKIP();
    return;
  }

  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));

  // Construct the location of valid JIT snapshots.
  const std::string src_path = GetSourcePath();
  const std::string vm_snapshot_data =
      fml::paths::JoinPaths({src_path, TEST_VM_SNAPSHOT_DATA});
  const std::string vm_snapshot_instructions =
      fml::paths::JoinPaths({src_path, TEST_VM_SNAPSHOT_INSTRUCTIONS});
  const std::string isolate_snapshot_data =
      fml::paths::JoinPaths({src_path, TEST_ISOLATE_SNAPSHOT_DATA});
  const std::string isolate_snapshot_instructions =
      fml::paths::JoinPaths({src_path, TEST_ISOLATE_SNAPSHOT_INSTRUCTIONS});

  // Explicitly define the locations of the JIT snapshots
  builder.GetProjectArgs().vm_snapshot_data =
      reinterpret_cast<const uint8_t*>(vm_snapshot_data.c_str());
  builder.GetProjectArgs().vm_snapshot_instructions =
      reinterpret_cast<const uint8_t*>(vm_snapshot_instructions.c_str());
  builder.GetProjectArgs().isolate_snapshot_data =
      reinterpret_cast<const uint8_t*>(isolate_snapshot_data.c_str());
  builder.GetProjectArgs().isolate_snapshot_instructions =
      reinterpret_cast<const uint8_t*>(isolate_snapshot_instructions.c_str());

  auto engine = builder.LaunchEngine();

  flutter::Shell& shell = ToEmbedderEngine(engine.get())->GetShell();
  const Settings settings = shell.GetSettings();

  ASSERT_NE(settings.vm_snapshot_data(), nullptr);
  ASSERT_NE(settings.vm_snapshot_instr(), nullptr);
  ASSERT_NE(settings.isolate_snapshot_data(), nullptr);
  ASSERT_NE(settings.isolate_snapshot_instr(), nullptr);
  ASSERT_NE(settings.dart_library_sources_kernel(), nullptr);
#endif  // OS_FUCHSIA
}

//------------------------------------------------------------------------------
/// PopulateJITSnapshotMappingCallbacks should still be able to successfully
/// change the callbacks of the snapshots in the engine's settings when JIT
/// snapshots are explicitly defined. However, if those snapshot locations are
/// invalid, the callbacks should return a nullptr.
///
TEST_F(EmbedderTest, JITSnapshotCallbacksFailWithInvalidLocation) {
// TODO(#107263): Inconsistent snapshot paths in the Linux Fuchsia FEMU test.
#if defined(OS_FUCHSIA)
  GTEST_SKIP() << "Inconsistent paths in Fuchsia.";
#else

  // This test is only relevant in JIT mode.
  if (DartVM::IsRunningPrecompiledCode()) {
    GTEST_SKIP();
    return;
  }

  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));

  // Explicitly define the locations of the invalid JIT snapshots
  builder.GetProjectArgs().vm_snapshot_data =
      reinterpret_cast<const uint8_t*>("invalid_vm_data");
  builder.GetProjectArgs().vm_snapshot_instructions =
      reinterpret_cast<const uint8_t*>("invalid_vm_instructions");
  builder.GetProjectArgs().isolate_snapshot_data =
      reinterpret_cast<const uint8_t*>("invalid_snapshot_data");
  builder.GetProjectArgs().isolate_snapshot_instructions =
      reinterpret_cast<const uint8_t*>("invalid_snapshot_instructions");

  auto engine = builder.LaunchEngine();

  flutter::Shell& shell = ToEmbedderEngine(engine.get())->GetShell();
  const Settings settings = shell.GetSettings();

  ASSERT_EQ(settings.vm_snapshot_data(), nullptr);
  ASSERT_EQ(settings.vm_snapshot_instr(), nullptr);
  ASSERT_EQ(settings.isolate_snapshot_data(), nullptr);
  ASSERT_EQ(settings.isolate_snapshot_instr(), nullptr);
#endif  // OS_FUCHSIA
}

//------------------------------------------------------------------------------
/// The embedder must be able to run explicitly specified snapshots in JIT mode
/// (i.e. when those are present in known locations).
///
TEST_F(EmbedderTest, CanLaunchEngineWithSpecifiedJITSnapshots) {
  // This test is only relevant in JIT mode.
  if (DartVM::IsRunningPrecompiledCode()) {
    GTEST_SKIP();
    return;
  }

  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));

  // Construct the location of valid JIT snapshots.
  const std::string src_path = GetSourcePath();
  const std::string vm_snapshot_data =
      fml::paths::JoinPaths({src_path, TEST_VM_SNAPSHOT_DATA});
  const std::string vm_snapshot_instructions =
      fml::paths::JoinPaths({src_path, TEST_VM_SNAPSHOT_INSTRUCTIONS});
  const std::string isolate_snapshot_data =
      fml::paths::JoinPaths({src_path, TEST_ISOLATE_SNAPSHOT_DATA});
  const std::string isolate_snapshot_instructions =
      fml::paths::JoinPaths({src_path, TEST_ISOLATE_SNAPSHOT_INSTRUCTIONS});

  // Explicitly define the locations of the JIT snapshots
  builder.GetProjectArgs().vm_snapshot_data =
      reinterpret_cast<const uint8_t*>(vm_snapshot_data.c_str());
  builder.GetProjectArgs().vm_snapshot_instructions =
      reinterpret_cast<const uint8_t*>(vm_snapshot_instructions.c_str());
  builder.GetProjectArgs().isolate_snapshot_data =
      reinterpret_cast<const uint8_t*>(isolate_snapshot_data.c_str());
  builder.GetProjectArgs().isolate_snapshot_instructions =
      reinterpret_cast<const uint8_t*>(isolate_snapshot_instructions.c_str());

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());
}

//------------------------------------------------------------------------------
/// The embedder must be able to run in JIT mode when only some snapshots are
/// specified.
///
TEST_F(EmbedderTest, CanLaunchEngineWithSomeSpecifiedJITSnapshots) {
  // This test is only relevant in JIT mode.
  if (DartVM::IsRunningPrecompiledCode()) {
    GTEST_SKIP();
    return;
  }

  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));

  // Construct the location of valid JIT snapshots.
  const std::string src_path = GetSourcePath();
  const std::string vm_snapshot_data =
      fml::paths::JoinPaths({src_path, TEST_VM_SNAPSHOT_DATA});
  const std::string vm_snapshot_instructions =
      fml::paths::JoinPaths({src_path, TEST_VM_SNAPSHOT_INSTRUCTIONS});

  // Explicitly define the locations of the JIT snapshots
  builder.GetProjectArgs().vm_snapshot_data =
      reinterpret_cast<const uint8_t*>(vm_snapshot_data.c_str());
  builder.GetProjectArgs().vm_snapshot_instructions =
      reinterpret_cast<const uint8_t*>(vm_snapshot_instructions.c_str());

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());
}

//------------------------------------------------------------------------------
/// The embedder must be able to run in JIT mode even when the specfied
/// snapshots are invalid. It should be able to resolve them as it would when
/// the snapshots are not specified.
///
TEST_F(EmbedderTest, CanLaunchEngineWithInvalidJITSnapshots) {
  // This test is only relevant in JIT mode.
  if (DartVM::IsRunningPrecompiledCode()) {
    GTEST_SKIP();
    return;
  }

  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));

  // Explicitly define the locations of the JIT snapshots
  builder.GetProjectArgs().isolate_snapshot_data =
      reinterpret_cast<const uint8_t*>("invalid_snapshot_data");
  builder.GetProjectArgs().isolate_snapshot_instructions =
      reinterpret_cast<const uint8_t*>("invalid_snapshot_instructions");

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());
  ASSERT_EQ(FlutterEngineRunInitialized(engine.get()), kInvalidArguments);
}

//------------------------------------------------------------------------------
/// The embedder must be able to launch even when the snapshots are not
/// explicitly defined in JIT mode. It must be able to resolve those snapshots.
///
TEST_F(EmbedderTest, CanLaunchEngineWithUnspecifiedJITSnapshots) {
  // This test is only relevant in JIT mode.
  if (DartVM::IsRunningPrecompiledCode()) {
    GTEST_SKIP();
    return;
  }

  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));

  ASSERT_EQ(builder.GetProjectArgs().vm_snapshot_data, nullptr);
  ASSERT_EQ(builder.GetProjectArgs().vm_snapshot_instructions, nullptr);
  ASSERT_EQ(builder.GetProjectArgs().isolate_snapshot_data, nullptr);
  ASSERT_EQ(builder.GetProjectArgs().isolate_snapshot_instructions, nullptr);

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());
}

TEST_F(EmbedderTest, InvalidFlutterWindowMetricsEvent) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto engine = builder.LaunchEngine();

  ASSERT_TRUE(engine.is_valid());

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 800;
  event.height = 600;
  event.pixel_ratio = 0.0;
  event.physical_view_inset_top = 0.0;
  event.physical_view_inset_right = 0.0;
  event.physical_view_inset_bottom = 0.0;
  event.physical_view_inset_left = 0.0;

  // Pixel ratio must be positive.
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kInvalidArguments);

  event.pixel_ratio = 1.0;
  event.physical_view_inset_top = -1.0;
  event.physical_view_inset_right = -1.0;
  event.physical_view_inset_bottom = -1.0;
  event.physical_view_inset_left = -1.0;

  // Physical view insets must be non-negative.
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kInvalidArguments);

  event.physical_view_inset_top = 700;
  event.physical_view_inset_right = 900;
  event.physical_view_inset_bottom = 700;
  event.physical_view_inset_left = 900;

  // Top/bottom insets cannot be greater than height.
  // Left/right insets cannot be greater than width.
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kInvalidArguments);

  // Reset insets to zero to test padding validation.
  event.physical_view_inset_top = 0.0;
  event.physical_view_inset_right = 0.0;
  event.physical_view_inset_bottom = 0.0;
  event.physical_view_inset_left = 0.0;

  // Negative padding should be rejected.
  event.physical_padding_top = -1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kInvalidArguments);

  // Excessive padding exceeding window dimensions should be rejected.
  event.physical_padding_top = 700.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kInvalidArguments);

  // Valid padding should succeed.
  event.physical_padding_top = 72.0;
  event.physical_padding_bottom = 48.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);
}

TEST_F(EmbedderTest, WindowMetricsEventWithConstraints) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto engine = builder.LaunchEngine();

  ASSERT_TRUE(engine.is_valid());

  // Test with has_constraints = true and valid constraints
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 800;
  event.height = 600;
  event.pixel_ratio = 1.0;
  event.has_constraints = true;
  event.min_width_constraint = 400;
  event.max_width_constraint = 1200;
  event.min_height_constraint = 300;
  event.max_height_constraint = 900;

  // Should succeed with valid constraints
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  // Test with has_constraints = false
  // Constraints should be ignored and set to current width/height
  FlutterWindowMetricsEvent event_no_constraints = {};
  event_no_constraints.struct_size = sizeof(event_no_constraints);
  event_no_constraints.width = 1024;
  event_no_constraints.height = 768;
  event_no_constraints.pixel_ratio = 1.0;
  event_no_constraints.has_constraints = false;
  // These constraint values should be ignored
  event_no_constraints.min_width_constraint = 0;
  event_no_constraints.max_width_constraint = 0;
  event_no_constraints.min_height_constraint = 0;
  event_no_constraints.max_height_constraint = 0;

  // Should succeed even with invalid constraint values because has_constraints
  // is false
  ASSERT_EQ(
      FlutterEngineSendWindowMetricsEvent(engine.get(), &event_no_constraints),
      kSuccess);

  // Test with has_constraints = true but width violates min constraint
  FlutterWindowMetricsEvent event_invalid_min = {};
  event_invalid_min.struct_size = sizeof(event_invalid_min);
  event_invalid_min.width = 300;  // Less than min_width_constraint
  event_invalid_min.height = 600;
  event_invalid_min.pixel_ratio = 1.0;
  event_invalid_min.has_constraints = true;
  event_invalid_min.min_width_constraint = 400;
  event_invalid_min.max_width_constraint = 1200;
  event_invalid_min.min_height_constraint = 300;
  event_invalid_min.max_height_constraint = 900;

  // Should fail because width < min_width_constraint
  ASSERT_EQ(
      FlutterEngineSendWindowMetricsEvent(engine.get(), &event_invalid_min),
      kInvalidArguments);

  // Test with has_constraints = true but width violates max constraint
  FlutterWindowMetricsEvent event_invalid_max = {};
  event_invalid_max.struct_size = sizeof(event_invalid_max);
  event_invalid_max.width = 1300;  // Greater than max_width_constraint
  event_invalid_max.height = 600;
  event_invalid_max.pixel_ratio = 1.0;
  event_invalid_max.has_constraints = true;
  event_invalid_max.min_width_constraint = 400;
  event_invalid_max.max_width_constraint = 1200;
  event_invalid_max.min_height_constraint = 300;
  event_invalid_max.max_height_constraint = 900;

  // Should fail because width > max_width_constraint
  ASSERT_EQ(
      FlutterEngineSendWindowMetricsEvent(engine.get(), &event_invalid_max),
      kInvalidArguments);

  // Test with has_constraints = true but height violates constraints
  FlutterWindowMetricsEvent event_invalid_height = {};
  event_invalid_height.struct_size = sizeof(event_invalid_height);
  event_invalid_height.width = 800;
  event_invalid_height.height = 200;  // Less than min_height_constraint
  event_invalid_height.pixel_ratio = 1.0;
  event_invalid_height.has_constraints = true;
  event_invalid_height.min_width_constraint = 400;
  event_invalid_height.max_width_constraint = 1200;
  event_invalid_height.min_height_constraint = 300;
  event_invalid_height.max_height_constraint = 900;

  // Should fail because height < min_height_constraint
  ASSERT_EQ(
      FlutterEngineSendWindowMetricsEvent(engine.get(), &event_invalid_height),
      kInvalidArguments);
}

TEST_F(EmbedderTest, WindowMetricsEventDisplayFeaturesValidation) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  // Test with display_features_count > 0 but null buffers.
  FlutterWindowMetricsEvent event_null_buffers = {};
  event_null_buffers.struct_size = sizeof(event_null_buffers);
  event_null_buffers.width = 800;
  event_null_buffers.height = 600;
  event_null_buffers.pixel_ratio = 1.0;
  event_null_buffers.display_features_count = 1;
  event_null_buffers.display_features_bounds = nullptr;
  event_null_buffers.display_features_type = nullptr;
  event_null_buffers.display_features_state = nullptr;

  ASSERT_EQ(
      FlutterEngineSendWindowMetricsEvent(engine.get(), &event_null_buffers),
      kInvalidArguments);

  // Test with valid display feature buffers.
  constexpr double kBounds[] = {0.0, 0.0, 100.0, 20.0};
  constexpr int kType[] = {kFlutterDisplayFeatureTypeFold};
  constexpr int kState[] = {kFlutterDisplayFeatureStatePostureFlat};

  FlutterWindowMetricsEvent event_valid = {};
  event_valid.struct_size = sizeof(event_valid);
  event_valid.width = 800;
  event_valid.height = 600;
  event_valid.pixel_ratio = 1.0;
  event_valid.display_features_count = 1;
  event_valid.display_features_bounds = kBounds;
  event_valid.display_features_type = kType;
  event_valid.display_features_state = kState;

  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event_valid),
            kSuccess);
}

TEST_F(EmbedderTest, WindowMetricsEventAllParityFields) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  constexpr double kDisplayFeaturesBounds[] = {0.0, 1000.0, 1080.0, 1050.0};
  constexpr int kDisplayFeaturesType[] = {kFlutterDisplayFeatureTypeFold};
  constexpr int kDisplayFeaturesState[] = {
      kFlutterDisplayFeatureStatePostureHalfOpened};

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 1080;
  event.height = 2400;
  event.pixel_ratio = 2.75;
  event.left = 0;
  event.top = 0;
  event.physical_view_inset_top = 10.0;
  event.physical_view_inset_right = 20.0;
  event.physical_view_inset_bottom = 30.0;
  event.physical_view_inset_left = 40.0;
  event.display_id = 0;
  event.view_id = 0;
  event.has_constraints = false;

  // Parity contract fields from T-0.4
  event.display_features_count = 1;
  event.display_features_bounds = kDisplayFeaturesBounds;
  event.display_features_type = kDisplayFeaturesType;
  event.display_features_state = kDisplayFeaturesState;
  event.physical_padding_top = 84.0;
  event.physical_padding_right = 12.0;
  event.physical_padding_bottom = 48.0;
  event.physical_padding_left = 16.0;
  event.physical_system_gesture_inset_top = 50.0;
  event.physical_system_gesture_inset_right = 60.0;
  event.physical_system_gesture_inset_bottom = 70.0;
  event.physical_system_gesture_inset_left = 80.0;
  event.physical_touch_slop = 24.0;
  event.physical_display_corner_radius_top_left = 15.0;
  event.physical_display_corner_radius_top_right = 16.0;
  event.physical_display_corner_radius_bottom_right = 17.0;
  event.physical_display_corner_radius_bottom_left = 18.0;

  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);
}

TEST_F(EmbedderTest, WindowMetricsEventWithLegacyStructSize) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  FlutterWindowMetricsEvent event = {};
  // Simulate an older caller whose struct ended at max_height_constraint.
  event.struct_size =
      offsetof(FlutterWindowMetricsEvent, max_height_constraint) +
      sizeof(size_t);
  event.width = 800;
  event.height = 600;
  event.pixel_ratio = 1.0;

  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);
}

static void expectSoftwareRenderingOutputMatches(
    EmbedderTest& test,
    std::string entrypoint,
    FlutterSoftwarePixelFormat pixfmt,
    const std::vector<uint8_t>& bytes) {
  auto& context = test.GetEmbedderContext<EmbedderTestContextSoftware>();

  EmbedderConfigBuilder builder(context);
  fml::AutoResetWaitableEvent latch;
  bool matches = false;

  builder.SetSurface(DlISize(1, 1));
  builder.SetCompositor();
  builder.SetDartEntrypoint(std::move(entrypoint));
  builder.SetRenderTargetType(
      EmbedderTestBackingStoreProducer::RenderTargetType::kSoftwareBuffer2,
      pixfmt);

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  context.GetCompositor().SetNextPresentCallback(
      [&context, &matches, &bytes, &latch](FlutterViewId view_id,
                                           const FlutterLayer** layers,
                                           size_t layers_count) {
        ASSERT_EQ(layers[0]->type, kFlutterLayerContentTypeBackingStore);
        ASSERT_EQ(layers[0]->backing_store->type,
                  kFlutterBackingStoreTypeSoftware2);
        sk_sp<SkSurface> surface =
            context.GetCompositor().GetSurface(layers[0]->backing_store);
        matches = SurfacePixelDataMatchesBytes(surface.get(), bytes);
        latch.Signal();
      });

  // Send a window metrics events so frames may be scheduled.
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 1;
  event.height = 1;
  event.pixel_ratio = 1.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  latch.Wait();
  ASSERT_TRUE(matches);

  engine.reset();
}

template <typename T>
static void expectSoftwareRenderingOutputMatches(
    EmbedderTest& test,
    std::string entrypoint,
    FlutterSoftwarePixelFormat pixfmt,
    T pixelvalue) {
  uint8_t* bytes = reinterpret_cast<uint8_t*>(&pixelvalue);
  return expectSoftwareRenderingOutputMatches(
      test, std::move(entrypoint), pixfmt,
      std::vector<uint8_t>(bytes, bytes + sizeof(T)));
}

#define SW_PIXFMT_TEST_F(test_name, dart_entrypoint, pixfmt, matcher)     \
  TEST_F(EmbedderTest, SoftwareRenderingPixelFormats##test_name) {        \
    expectSoftwareRenderingOutputMatches(*this, #dart_entrypoint, pixfmt, \
                                         matcher);                        \
  }

// Don't test the pixel formats that contain padding (so an X) and the
// kFlutterSoftwarePixelFormatNative32 pixel format here, so we don't add any
// flakiness.
SW_PIXFMT_TEST_F(RedRGBA565xF800,
                 draw_solid_red,
                 kFlutterSoftwarePixelFormatRGB565,
                 (uint16_t)0xF800);
SW_PIXFMT_TEST_F(RedRGBA4444xF00F,
                 draw_solid_red,
                 kFlutterSoftwarePixelFormatRGBA4444,
                 (uint16_t)0xF00F);
SW_PIXFMT_TEST_F(RedRGBA8888xFFx00x00xFF,
                 draw_solid_red,
                 kFlutterSoftwarePixelFormatRGBA8888,
                 (std::vector<uint8_t>{0xFF, 0x00, 0x00, 0xFF}));
SW_PIXFMT_TEST_F(RedBGRA8888x00x00xFFxFF,
                 draw_solid_red,
                 kFlutterSoftwarePixelFormatBGRA8888,
                 (std::vector<uint8_t>{0x00, 0x00, 0xFF, 0xFF}));
SW_PIXFMT_TEST_F(RedGray8x36,
                 draw_solid_red,
                 kFlutterSoftwarePixelFormatGray8,
                 (uint8_t)0x36);

SW_PIXFMT_TEST_F(GreenRGB565x07E0,
                 draw_solid_green,
                 kFlutterSoftwarePixelFormatRGB565,
                 (uint16_t)0x07E0);
SW_PIXFMT_TEST_F(GreenRGBA4444x0F0F,
                 draw_solid_green,
                 kFlutterSoftwarePixelFormatRGBA4444,
                 (uint16_t)0x0F0F);
SW_PIXFMT_TEST_F(GreenRGBA8888x00xFFx00xFF,
                 draw_solid_green,
                 kFlutterSoftwarePixelFormatRGBA8888,
                 (std::vector<uint8_t>{0x00, 0xFF, 0x00, 0xFF}));
SW_PIXFMT_TEST_F(GreenBGRA8888x00xFFx00xFF,
                 draw_solid_green,
                 kFlutterSoftwarePixelFormatBGRA8888,
                 (std::vector<uint8_t>{0x00, 0xFF, 0x00, 0xFF}));
SW_PIXFMT_TEST_F(GreenGray8xB6,
                 draw_solid_green,
                 kFlutterSoftwarePixelFormatGray8,
                 (uint8_t)0xB6);

SW_PIXFMT_TEST_F(BlueRGB565x001F,
                 draw_solid_blue,
                 kFlutterSoftwarePixelFormatRGB565,
                 (uint16_t)0x001F);
SW_PIXFMT_TEST_F(BlueRGBA4444x00FF,
                 draw_solid_blue,
                 kFlutterSoftwarePixelFormatRGBA4444,
                 (uint16_t)0x00FF);
SW_PIXFMT_TEST_F(BlueRGBA8888x00x00xFFxFF,
                 draw_solid_blue,
                 kFlutterSoftwarePixelFormatRGBA8888,
                 (std::vector<uint8_t>{0x00, 0x00, 0xFF, 0xFF}));
SW_PIXFMT_TEST_F(BlueBGRA8888xFFx00x00xFF,
                 draw_solid_blue,
                 kFlutterSoftwarePixelFormatBGRA8888,
                 (std::vector<uint8_t>{0xFF, 0x00, 0x00, 0xFF}));
SW_PIXFMT_TEST_F(BlueGray8x12,
                 draw_solid_blue,
                 kFlutterSoftwarePixelFormatGray8,
                 (uint8_t)0x12);

//------------------------------------------------------------------------------
// Key Data
//------------------------------------------------------------------------------

typedef struct {
  std::shared_ptr<fml::AutoResetWaitableEvent> latch;
  bool returned;
} KeyEventUserData;

// Convert `kind` in integer form to its enum form.
//
// It performs a revesed mapping from `_serializeKeyEventType`
// in shell/platform/embedder/fixtures/main.dart.
FlutterKeyEventType UnserializeKeyEventType(uint64_t kind) {
  switch (kind) {
    case 1:
      return kFlutterKeyEventTypeUp;
    case 2:
      return kFlutterKeyEventTypeDown;
    case 3:
      return kFlutterKeyEventTypeRepeat;
    default:
      FML_UNREACHABLE();
      return kFlutterKeyEventTypeUp;
  }
}

// Convert `source` in integer form to its enum form.
//
// It performs a revesed mapping from `_serializeKeyEventDeviceType`
// in shell/platform/embedder/fixtures/main.dart.
FlutterKeyEventDeviceType UnserializeKeyEventDeviceType(uint64_t source) {
  switch (source) {
    case 1:
      return kFlutterKeyEventDeviceTypeKeyboard;
    case 2:
      return kFlutterKeyEventDeviceTypeDirectionalPad;
    case 3:
      return kFlutterKeyEventDeviceTypeGamepad;
    case 4:
      return kFlutterKeyEventDeviceTypeJoystick;
    case 5:
      return kFlutterKeyEventDeviceTypeHdmi;
    default:
      FML_UNREACHABLE();
      return kFlutterKeyEventDeviceTypeKeyboard;
  }
}

// Checks the equality of two `FlutterKeyEvent` by each of their members except
// for `character`. The `character` must be checked separately.
void ExpectKeyEventEq(const FlutterKeyEvent& subject,
                      const FlutterKeyEvent& baseline) {
  EXPECT_EQ(subject.timestamp, baseline.timestamp);
  EXPECT_EQ(subject.type, baseline.type);
  EXPECT_EQ(subject.physical, baseline.physical);
  EXPECT_EQ(subject.logical, baseline.logical);
  EXPECT_EQ(subject.synthesized, baseline.synthesized);
  EXPECT_EQ(subject.device_type, baseline.device_type);
}

TEST_F(EmbedderTest, KeyDataIsCorrectlySerialized) {
  auto message_latch = std::make_shared<fml::AutoResetWaitableEvent>();
  uint64_t echoed_char;
  FlutterKeyEvent echoed_event;
  echoed_event.struct_size = sizeof(FlutterKeyEvent);

  auto native_echo_event = [&](uint64_t change, uint64_t timestamp,
                               uint64_t physical, uint64_t logical,
                               uint64_t char_code, bool synthesized,
                               uint64_t device_type) {
    echoed_event.type = UnserializeKeyEventType(change);
    echoed_event.timestamp = static_cast<double>(timestamp);
    echoed_event.physical = physical;
    echoed_event.logical = logical;
    echoed_char = char_code;
    echoed_event.synthesized = synthesized;
    echoed_event.device_type = UnserializeKeyEventDeviceType(device_type);

    message_latch->Signal();
  };

  auto platform_task_runner = CreateNewThread("platform_thread");

  UniqueEngine engine;
  fml::AutoResetWaitableEvent ready;
  platform_task_runner->PostTask([&]() {
    auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
    EmbedderConfigBuilder builder(context);
    builder.SetSurface(DlISize(1, 1));
    builder.SetDartEntrypoint("key_data_echo");
    builder.SetPlatformMessageCallback(
        [&](const FlutterPlatformMessage* message) {
          FlutterEngineSendPlatformMessageResponse(
              engine.get(), message->response_handle, nullptr, 0);
        });
    context.AddFfiNativeCallback(
        "SignalNativeTest", CREATE_FFI_LAMBDA([&ready]() { ready.Signal(); }));

    context.AddFfiNativeCallback("EchoKeyEvent",
                                 CREATE_FFI_LAMBDA(native_echo_event));

    engine = builder.LaunchEngine();
    ASSERT_TRUE(engine.is_valid());
  });

  ready.Wait();

  // A normal down event
  const FlutterKeyEvent down_event_upper_a{
      .struct_size = sizeof(FlutterKeyEvent),
      .timestamp = 1,
      .type = kFlutterKeyEventTypeDown,
      .physical = 0x00070004,
      .logical = 0x00000000061,
      .character = "A",
      .synthesized = false,
      .device_type = kFlutterKeyEventDeviceTypeKeyboard,
  };
  platform_task_runner->PostTask([&]() {
    FlutterEngineSendKeyEvent(engine.get(), &down_event_upper_a, nullptr,
                              nullptr);
  });
  message_latch->Wait();

  ExpectKeyEventEq(echoed_event, down_event_upper_a);
  EXPECT_EQ(echoed_char, 0x41llu);

  // A repeat event with multi-byte character
  const FlutterKeyEvent repeat_event_wide_char{
      .struct_size = sizeof(FlutterKeyEvent),
      .timestamp = 1000,
      .type = kFlutterKeyEventTypeRepeat,
      .physical = 0x00070005,
      .logical = 0x00000000062,
      .character = "∆",
      .synthesized = false,
      .device_type = kFlutterKeyEventDeviceTypeKeyboard,
  };
  platform_task_runner->PostTask([&]() {
    FlutterEngineSendKeyEvent(engine.get(), &repeat_event_wide_char, nullptr,
                              nullptr);
  });
  message_latch->Wait();

  ExpectKeyEventEq(echoed_event, repeat_event_wide_char);
  EXPECT_EQ(echoed_char, 0x2206llu);

  // An up event with no character, synthesized
  const FlutterKeyEvent up_event{
      .struct_size = sizeof(FlutterKeyEvent),
      .timestamp = 1000000,
      .type = kFlutterKeyEventTypeUp,
      .physical = 0x00070006,
      .logical = 0x00000000063,
      .character = nullptr,
      .synthesized = true,
      .device_type = kFlutterKeyEventDeviceTypeKeyboard,
  };
  platform_task_runner->PostTask([&]() {
    FlutterEngineSendKeyEvent(engine.get(), &up_event, nullptr, nullptr);
  });
  message_latch->Wait();

  ExpectKeyEventEq(echoed_event, up_event);
  EXPECT_EQ(echoed_char, 0llu);

  fml::AutoResetWaitableEvent shutdown_latch;
  platform_task_runner->PostTask([&]() {
    engine.reset();
    shutdown_latch.Signal();
  });
  shutdown_latch.Wait();
}

TEST_F(EmbedderTest, KeyDataAreBuffered) {
  auto message_latch = std::make_shared<fml::AutoResetWaitableEvent>();
  std::vector<FlutterKeyEvent> echoed_events;

  auto native_echo_event = [&](uint64_t change, uint64_t timestamp,
                               uint64_t physical, uint64_t logical,
                               uint64_t char_code, bool synthesized,
                               uint64_t device_type) {
    echoed_events.push_back(FlutterKeyEvent{
        .timestamp = static_cast<double>(timestamp),
        .type = UnserializeKeyEventType(change),
        .physical = physical,
        .logical = logical,
        .synthesized = synthesized,
        .device_type = UnserializeKeyEventDeviceType(device_type),
    });

    message_latch->Signal();
  };

  auto platform_task_runner = CreateNewThread("platform_thread");

  UniqueEngine engine;
  fml::AutoResetWaitableEvent ready;
  platform_task_runner->PostTask([&]() {
    auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
    EmbedderConfigBuilder builder(context);
    builder.SetSurface(DlISize(1, 1));
    builder.SetDartEntrypoint("key_data_late_echo");
    builder.SetPlatformMessageCallback(
        [&](const FlutterPlatformMessage* message) {
          FlutterEngineSendPlatformMessageResponse(
              engine.get(), message->response_handle, nullptr, 0);
        });
    context.AddFfiNativeCallback(
        "SignalNativeTest", CREATE_FFI_LAMBDA([&ready]() { ready.Signal(); }));

    context.AddFfiNativeCallback("EchoKeyEvent",
                                 CREATE_FFI_LAMBDA(native_echo_event));

    engine = builder.LaunchEngine();
    ASSERT_TRUE(engine.is_valid());
  });
  ready.Wait();

  FlutterKeyEvent sample_event{
      .struct_size = sizeof(FlutterKeyEvent),
      .type = kFlutterKeyEventTypeDown,
      .physical = 0x00070004,
      .logical = 0x00000000061,
      .character = "A",
      .synthesized = false,
      .device_type = kFlutterKeyEventDeviceTypeKeyboard,
  };

  // Send an event.
  sample_event.timestamp = 1.0;
  platform_task_runner->PostTask([&]() {
    FlutterEngineSendKeyEvent(engine.get(), &sample_event, nullptr, nullptr);
    message_latch->Signal();
  });
  message_latch->Wait();

  // Should not receive echos because the callback is not set yet.
  EXPECT_EQ(echoed_events.size(), 0u);

  // Send an empty message to 'test/starts_echo' to start echoing.
  FlutterPlatformMessageResponseHandle* response_handle = nullptr;
  FlutterPlatformMessageCreateResponseHandle(
      engine.get(), [](const uint8_t* data, size_t size, void* user_data) {},
      nullptr, &response_handle);

  FlutterPlatformMessage message{
      .struct_size = sizeof(FlutterPlatformMessage),
      .channel = "test/starts_echo",
      .message = nullptr,
      .message_size = 0,
      .response_handle = response_handle,
  };

  platform_task_runner->PostTask([&]() {
    FlutterEngineResult result =
        FlutterEngineSendPlatformMessage(engine.get(), &message);
    ASSERT_EQ(result, kSuccess);

    FlutterPlatformMessageReleaseResponseHandle(engine.get(), response_handle);
  });

  // message_latch->Wait();
  message_latch->Wait();
  // All previous events should be received now.
  EXPECT_EQ(echoed_events.size(), 1u);

  // Send a second event.
  sample_event.timestamp = 10.0;
  platform_task_runner->PostTask([&]() {
    FlutterEngineSendKeyEvent(engine.get(), &sample_event, nullptr, nullptr);
  });
  message_latch->Wait();

  // The event should be echoed, too.
  EXPECT_EQ(echoed_events.size(), 2u);

  fml::AutoResetWaitableEvent shutdown_latch;
  platform_task_runner->PostTask([&]() {
    engine.reset();
    shutdown_latch.Signal();
  });
  shutdown_latch.Wait();
}

TEST_F(EmbedderTest, KeyDataResponseIsCorrectlyInvoked) {
  UniqueEngine engine;
  fml::AutoResetWaitableEvent sync_latch;
  fml::AutoResetWaitableEvent ready;

  // One of the threads that the key data callback will be posted to is the
  // platform thread. So we cannot wait for assertions to complete on the
  // platform thread. Create a new thread to manage the engine instance and wait
  // for assertions on the test thread.
  auto platform_task_runner = CreateNewThread("platform_thread");

  platform_task_runner->PostTask([&]() {
    auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
    EmbedderConfigBuilder builder(context);
    builder.SetSurface(DlISize(1, 1));
    builder.SetDartEntrypoint("key_data_echo");
    context.AddFfiNativeCallback(
        "SignalNativeTest", CREATE_FFI_LAMBDA([&ready]() { ready.Signal(); }));
    context.AddFfiNativeCallback(
        "EchoKeyEvent",
        CREATE_FFI_LAMBDA([](uint64_t change, uint64_t timestamp,
                             uint64_t physical, uint64_t logical,
                             uint64_t char_code, bool synthesized,
                             uint64_t device_type) {}));

    engine = builder.LaunchEngine();
    ASSERT_TRUE(engine.is_valid());

    sync_latch.Signal();
  });
  sync_latch.Wait();
  ready.Wait();

  // Dispatch a single event
  FlutterKeyEvent event{
      .struct_size = sizeof(FlutterKeyEvent),
      .timestamp = 1000,
      .type = kFlutterKeyEventTypeDown,
      .physical = 0x00070005,
      .logical = 0x00000000062,
      .character = nullptr,
      .device_type = kFlutterKeyEventDeviceTypeKeyboard,
  };

  KeyEventUserData user_data1{
      .latch = std::make_shared<fml::AutoResetWaitableEvent>(),
  };
  // Entrypoint `key_data_echo` returns `event.synthesized` as `handled`.
  event.synthesized = true;
  platform_task_runner->PostTask([&]() {
    // Test when the response callback is empty.
    // It should not cause a crash.
    FlutterEngineSendKeyEvent(engine.get(), &event, nullptr, nullptr);

    // Test when the response callback is non-empty.
    // It should be invoked (so that the latch can be unlocked.)
    FlutterEngineSendKeyEvent(
        engine.get(), &event,
        [](bool handled, void* untyped_user_data) {
          KeyEventUserData* user_data =
              reinterpret_cast<KeyEventUserData*>(untyped_user_data);
          EXPECT_EQ(handled, true);
          user_data->latch->Signal();
        },
        &user_data1);
  });
  user_data1.latch->Wait();
  fml::AutoResetWaitableEvent shutdown_latch;
  platform_task_runner->PostTask([&]() {
    engine.reset();
    shutdown_latch.Signal();
  });
  shutdown_latch.Wait();
}

TEST_F(EmbedderTest, BackToBackKeyEventResponsesCorrectlyInvoked) {
  UniqueEngine engine;
  fml::AutoResetWaitableEvent sync_latch;
  fml::AutoResetWaitableEvent ready;

  // One of the threads that the callback will be posted to is the platform
  // thread. So we cannot wait for assertions to complete on the platform
  // thread. Create a new thread to manage the engine instance and wait for
  // assertions on the test thread.
  auto platform_task_runner = CreateNewThread("platform_thread");

  platform_task_runner->PostTask([&]() {
    auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
    EmbedderConfigBuilder builder(context);
    builder.SetSurface(DlISize(1, 1));
    builder.SetDartEntrypoint("key_data_echo");
    context.AddFfiNativeCallback(
        "SignalNativeTest", CREATE_FFI_LAMBDA([&ready]() { ready.Signal(); }));

    context.AddFfiNativeCallback(
        "EchoKeyEvent",
        CREATE_FFI_LAMBDA([](uint64_t change, uint64_t timestamp,
                             uint64_t physical, uint64_t logical,
                             uint64_t char_code, bool synthesized,
                             uint64_t device_type) {}));

    engine = builder.LaunchEngine();
    ASSERT_TRUE(engine.is_valid());

    sync_latch.Signal();
  });
  sync_latch.Wait();
  ready.Wait();

  // Dispatch a single event
  FlutterKeyEvent event{
      .struct_size = sizeof(FlutterKeyEvent),
      .timestamp = 1000,
      .type = kFlutterKeyEventTypeDown,
      .physical = 0x00070005,
      .logical = 0x00000000062,
      .character = nullptr,
      .synthesized = false,
      .device_type = kFlutterKeyEventDeviceTypeKeyboard,
  };

  // Dispatch two events back to back, using the same callback on different
  // user_data
  KeyEventUserData user_data2{
      .latch = std::make_shared<fml::AutoResetWaitableEvent>(),
      .returned = false,
  };
  KeyEventUserData user_data3{
      .latch = std::make_shared<fml::AutoResetWaitableEvent>(),
      .returned = false,
  };
  auto callback23 = [](bool handled, void* untyped_user_data) {
    KeyEventUserData* user_data =
        reinterpret_cast<KeyEventUserData*>(untyped_user_data);
    EXPECT_EQ(handled, false);
    user_data->returned = true;
    user_data->latch->Signal();
  };
  platform_task_runner->PostTask([&]() {
    FlutterEngineSendKeyEvent(engine.get(), &event, callback23, &user_data2);
    FlutterEngineSendKeyEvent(engine.get(), &event, callback23, &user_data3);
  });
  user_data2.latch->Wait();
  user_data3.latch->Wait();

  EXPECT_TRUE(user_data2.returned);
  EXPECT_TRUE(user_data3.returned);

  fml::AutoResetWaitableEvent shutdown_latch;
  platform_task_runner->PostTask([&]() {
    engine.reset();
    shutdown_latch.Signal();
  });
  shutdown_latch.Wait();
}

//------------------------------------------------------------------------------
// Vsync waiter
//------------------------------------------------------------------------------

// This test schedules a frame for the future and asserts that vsync waiter
// posts the event at the right frame start time (which is in the future).
TEST_F(EmbedderTest, VsyncCallbackPostedIntoFuture) {
  UniqueEngine engine;
  fml::AutoResetWaitableEvent present_latch;
  fml::AutoResetWaitableEvent vsync_latch;

  // One of the threads that the callback (FlutterEngineOnVsync) will be posted
  // to is the platform thread. So we cannot wait for assertions to complete on
  // the platform thread. Create a new thread to manage the engine instance and
  // wait for assertions on the test thread.
  auto platform_task_runner = CreateNewThread("platform_thread");

  platform_task_runner->PostTask([&]() {
    auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
    context.SetVsyncCallback([&](intptr_t baton) {
      platform_task_runner->PostTask([baton = baton, &engine, &vsync_latch]() {
        FlutterEngineOnVsync(engine.get(), baton, NanosFromEpoch(16),
                             NanosFromEpoch(32));
        vsync_latch.Signal();
      });
    });
    context.AddFfiNativeCallback("SignalNativeTest", CREATE_FFI_LAMBDA([&]() {
                                   present_latch.Signal();
                                 }));

    EmbedderConfigBuilder builder(context);
    builder.SetSurface(DlISize(1, 1));
    builder.SetupVsyncCallback();
    builder.SetDartEntrypoint("empty_scene");
    engine = builder.LaunchEngine();
    ASSERT_TRUE(engine.is_valid());

    // Send a window metrics events so frames may be scheduled.
    FlutterWindowMetricsEvent event = {};
    event.struct_size = sizeof(event);
    event.width = 800;
    event.height = 600;
    event.pixel_ratio = 1.0;

    ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
              kSuccess);
  });

  vsync_latch.Wait();
  present_latch.Wait();

  fml::AutoResetWaitableEvent shutdown_latch;
  platform_task_runner->PostTask([&]() {
    engine.reset();
    shutdown_latch.Signal();
  });
  shutdown_latch.Wait();
}

TEST_F(EmbedderTest, CanScheduleFrame) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("can_schedule_frame");
  fml::AutoResetWaitableEvent latch;
  context.AddFfiNativeCallback(
      "SignalNativeTest", CREATE_FFI_LAMBDA([&latch]() { latch.Signal(); }));

  fml::AutoResetWaitableEvent check_latch;
  context.AddFfiNativeCallback(
      "SignalNativeCount",
      CREATE_FFI_LAMBDA([&check_latch](int count) { check_latch.Signal(); }));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  // Wait for the application to attach the listener.
  latch.Wait();

  ASSERT_EQ(FlutterEngineScheduleFrame(engine.get()), kSuccess);

  check_latch.Wait();
}

TEST_F(EmbedderTest, CanSetNextFrameCallback) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("draw_solid_red");

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  // Register the callback that is executed once the next frame is drawn.
  fml::AutoResetWaitableEvent callback_latch;
  VoidCallback callback = [](void* user_data) {
    fml::AutoResetWaitableEvent* callback_latch =
        static_cast<fml::AutoResetWaitableEvent*>(user_data);

    callback_latch->Signal();
  };

  auto result = FlutterEngineSetNextFrameCallback(engine.get(), callback,
                                                  &callback_latch);
  ASSERT_EQ(result, kSuccess);

  // Send a window metrics events so frames may be scheduled.
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(event);
  event.width = 800;
  event.height = 600;
  event.pixel_ratio = 1.0;
  event.physical_view_inset_top = 0.0;
  event.physical_view_inset_right = 0.0;
  event.physical_view_inset_bottom = 0.0;
  event.physical_view_inset_left = 0.0;
  ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
            kSuccess);

  callback_latch.Wait();
}

#if defined(FML_OS_MACOSX)

static void MockThreadConfigSetter(const fml::Thread::ThreadConfig& config) {
  pthread_t tid = pthread_self();
  struct sched_param param;
  int policy = SCHED_OTHER;
  switch (config.priority) {
    case fml::Thread::ThreadPriority::kDisplay:
      param.sched_priority = 10;
      break;
    default:
      param.sched_priority = 1;
  }
  pthread_setschedparam(tid, policy, &param);
}

TEST_F(EmbedderTest, EmbedderThreadHostUseCustomThreadConfig) {
  auto thread_host =
      flutter::EmbedderThreadHost::CreateEmbedderOrEngineManagedThreadHost(
          nullptr, MockThreadConfigSetter);

  fml::AutoResetWaitableEvent ui_latch;
  int ui_policy;
  struct sched_param ui_param;

  thread_host->GetTaskRunners().GetUITaskRunner()->PostTask([&] {
    pthread_t current_thread = pthread_self();
    pthread_getschedparam(current_thread, &ui_policy, &ui_param);
    ASSERT_EQ(ui_param.sched_priority, 10);
    ui_latch.Signal();
  });

  fml::AutoResetWaitableEvent io_latch;
  int io_policy;
  struct sched_param io_param;
  thread_host->GetTaskRunners().GetIOTaskRunner()->PostTask([&] {
    pthread_t current_thread = pthread_self();
    pthread_getschedparam(current_thread, &io_policy, &io_param);
    ASSERT_EQ(io_param.sched_priority, 1);
    io_latch.Signal();
  });

  ui_latch.Wait();
  io_latch.Wait();
}
#endif

/// Send a pointer event to Dart and wait until the Dart code signals
/// it received the event.
TEST_F(EmbedderTest, CanSendPointer) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("pointer_data_packet");

  fml::AutoResetWaitableEvent ready_latch, count_latch, message_latch;
  context.AddFfiNativeCallback(
      "SignalNativeTest",
      CREATE_FFI_LAMBDA([&ready_latch]() { ready_latch.Signal(); }));
  context.AddFfiNativeCallback("SignalNativeCount",
                               CREATE_FFI_LAMBDA([&count_latch](int count) {
                                 ASSERT_EQ(count, 1);
                                 count_latch.Signal();
                               }));
  context.AddFfiNativeCallback(
      "SignalNativeMessage",
      CREATE_FFI_LAMBDA([&message_latch](Dart_Handle message_handle) {
        auto message =
            tonic::DartConverter<std::string>::FromDart(message_handle);
        ASSERT_EQ("PointerData(viewId: 0, x: 123.0, y: 456.0)", message);
        message_latch.Signal();
      }));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  ready_latch.Wait();

  FlutterPointerEvent pointer_event = {};
  pointer_event.struct_size = sizeof(FlutterPointerEvent);
  pointer_event.phase = FlutterPointerPhase::kAdd;
  pointer_event.x = 123;
  pointer_event.y = 456;
  pointer_event.timestamp = static_cast<size_t>(1234567890);
  pointer_event.view_id = 0;

  FlutterEngineResult result =
      FlutterEngineSendPointerEvent(engine.get(), &pointer_event, 1);
  ASSERT_EQ(result, kSuccess);

  count_latch.Wait();
  message_latch.Wait();
}

/// Send a stylus pointer event to Dart and verify the buttons mask.
TEST_F(EmbedderTest, CanSendStylusPointerButtons) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("pointer_data_packet_stylus_buttons");

  fml::AutoResetWaitableEvent ready_latch, count_latch, message_latch;
  context.AddFfiNativeCallback(
      "SignalNativeTest",
      CREATE_FFI_LAMBDA([&ready_latch]() { ready_latch.Signal(); }));
  context.AddFfiNativeCallback("SignalNativeCount",
                               CREATE_FFI_LAMBDA([&count_latch](int count) {
                                 EXPECT_EQ(count, 1);
                                 count_latch.Signal();
                               }));
  context.AddFfiNativeCallback(
      "SignalNativeMessage",
      CREATE_FFI_LAMBDA([&message_latch](Dart_Handle message_handle) {
        auto message =
            tonic::DartConverter<std::string>::FromDart(message_handle);
        EXPECT_EQ("buttons: 3", message);
        message_latch.Signal();
      }));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  ready_latch.Wait();

  FlutterPointerEvent pointer_event = {};
  pointer_event.struct_size = sizeof(FlutterPointerEvent);
  pointer_event.phase = FlutterPointerPhase::kAdd;
  pointer_event.device_kind = kFlutterPointerDeviceKindStylus;
  pointer_event.buttons =
      kFlutterPointerButtonStylusContact | kFlutterPointerButtonStylusPrimary;
  pointer_event.x = 123;
  pointer_event.y = 456;
  pointer_event.timestamp = static_cast<size_t>(1234567890);
  pointer_event.view_id = 0;

  FlutterEngineResult result =
      FlutterEngineSendPointerEvent(engine.get(), &pointer_event, 1);
  ASSERT_EQ(result, kSuccess);

  count_latch.Wait();
  message_latch.Wait();
}

/// Send a pointer event to Dart and wait until the Dart code echos with the
/// view ID.
TEST_F(EmbedderTest, CanSendPointerEventWithViewId) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("pointer_data_packet_view_id");

  fml::AutoResetWaitableEvent ready_latch, add_view_latch, message_latch;
  context.AddFfiNativeCallback(
      "SignalNativeTest",
      CREATE_FFI_LAMBDA([&ready_latch]() { ready_latch.Signal(); }));
  context.AddFfiNativeCallback(
      "SignalNativeMessage",
      CREATE_FFI_LAMBDA([&message_latch](Dart_Handle message_handle) {
        auto message =
            tonic::DartConverter<std::string>::FromDart(message_handle);
        ASSERT_EQ("ViewID: 2", message);
        message_latch.Signal();
      }));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());
  ready_latch.Wait();

  // Add view 2
  FlutterWindowMetricsEvent metrics = {};
  metrics.struct_size = sizeof(FlutterWindowMetricsEvent);
  metrics.width = 800;
  metrics.height = 600;
  metrics.pixel_ratio = 1.0;
  metrics.view_id = 2;

  FlutterAddViewInfo info = {};
  info.struct_size = sizeof(FlutterAddViewInfo);
  info.view_id = 2;
  info.view_metrics = &metrics;
  info.add_view_callback = [](const FlutterAddViewResult* result) {
    EXPECT_TRUE(result->added);
    fml::AutoResetWaitableEvent* add_view_latch =
        reinterpret_cast<fml::AutoResetWaitableEvent*>(result->user_data);
    add_view_latch->Signal();
  };
  info.user_data = &add_view_latch;
  ASSERT_EQ(FlutterEngineAddView(engine.get(), &info), kSuccess);
  add_view_latch.Wait();

  // Send a pointer event for view 2
  FlutterPointerEvent pointer_event = {};
  pointer_event.struct_size = sizeof(FlutterPointerEvent);
  pointer_event.phase = FlutterPointerPhase::kAdd;
  pointer_event.x = 123;
  pointer_event.y = 456;
  pointer_event.timestamp = static_cast<size_t>(1234567890);
  pointer_event.view_id = 2;

  FlutterEngineResult result =
      FlutterEngineSendPointerEvent(engine.get(), &pointer_event, 1);
  ASSERT_EQ(result, kSuccess);

  message_latch.Wait();
}

/// Send a pointer event with geometry fields (tilt, orientation, radii,
/// distance, size, embedder_id) to Dart.
TEST_F(EmbedderTest, CanSendPointerDataGeometry) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("pointer_data_packet_geometry");

  fml::AutoResetWaitableEvent ready_latch, message_latch;
  context.AddFfiNativeCallback(
      "SignalNativeTest",
      CREATE_FFI_LAMBDA([&ready_latch]() { ready_latch.Signal(); }));
  context.AddFfiNativeCallback(
      "SignalNativeMessage",
      CREATE_FFI_LAMBDA([&message_latch](Dart_Handle message_handle) {
        auto message =
            tonic::DartConverter<std::string>::FromDart(message_handle);
        EXPECT_EQ(
            "embedderId: 42, tilt: 0.5, orientation: 1.2, radiusMajor: 15.0, "
            "radiusMinor: 10.0, radiusMin: 5.0, radiusMax: 20.0, distance: "
            "2.5, "
            "distanceMax: 10.0, size: 0.8",
            message);
        message_latch.Signal();
      }));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());
  ready_latch.Wait();

  FlutterPointerEvent pointer_event = {};
  pointer_event.struct_size = sizeof(FlutterPointerEvent);
  pointer_event.phase = FlutterPointerPhase::kAdd;
  pointer_event.device_kind = kFlutterPointerDeviceKindStylus;
  pointer_event.x = 100;
  pointer_event.y = 200;
  pointer_event.timestamp = static_cast<size_t>(1234567890);
  pointer_event.view_id = 0;
  pointer_event.embedder_id = 42;
  pointer_event.tilt = 0.5;
  pointer_event.orientation = 1.2;
  pointer_event.radius_major = 15.0;
  pointer_event.radius_minor = 10.0;
  pointer_event.radius_min = 5.0;
  pointer_event.radius_max = 20.0;
  pointer_event.distance = 2.5;
  pointer_event.distance_max = 10.0;
  pointer_event.size = 0.8;

  FlutterEngineResult result =
      FlutterEngineSendPointerEvent(engine.get(), &pointer_event, 1);
  ASSERT_EQ(result, kSuccess);

  message_latch.Wait();
}

/// Send a pointer event with a legacy struct size (truncated before new fields)
/// to verify backwards compatibility.
TEST_F(EmbedderTest, CanSendPointerEventWithLegacyStructSize) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("pointer_data_packet_geometry");

  fml::AutoResetWaitableEvent ready_latch, message_latch;
  context.AddFfiNativeCallback(
      "SignalNativeTest",
      CREATE_FFI_LAMBDA([&ready_latch]() { ready_latch.Signal(); }));
  context.AddFfiNativeCallback(
      "SignalNativeMessage",
      CREATE_FFI_LAMBDA([&message_latch](Dart_Handle message_handle) {
        auto message =
            tonic::DartConverter<std::string>::FromDart(message_handle);
        EXPECT_EQ(
            "embedderId: 0, tilt: 0.0, orientation: 0.0, radiusMajor: 0.0, "
            "radiusMinor: 0.0, radiusMin: 0.0, radiusMax: 0.0, distance: 0.0, "
            "distanceMax: 0.0, size: 0.0",
            message);
        message_latch.Signal();
      }));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());
  ready_latch.Wait();

  FlutterPointerEvent pointer_event = {};
  // Simulate an older embedder compiled when struct ended at pressure_max.
  pointer_event.struct_size =
      offsetof(FlutterPointerEvent, pressure_max) + sizeof(double);
  pointer_event.phase = FlutterPointerPhase::kAdd;
  pointer_event.device_kind = kFlutterPointerDeviceKindTouch;
  pointer_event.x = 50;
  pointer_event.y = 75;
  pointer_event.timestamp = static_cast<size_t>(1234567890);
  pointer_event.view_id = 0;

  FlutterEngineResult result =
      FlutterEngineSendPointerEvent(engine.get(), &pointer_event, 1);
  ASSERT_EQ(result, kSuccess);

  message_latch.Wait();
}

TEST_F(EmbedderTest, WindowMetricsEventDefaultsToImplicitView) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("window_metrics_event_view_id");

  fml::AutoResetWaitableEvent ready_latch, message_latch;
  context.AddFfiNativeCallback(
      "SignalNativeTest",
      CREATE_FFI_LAMBDA([&ready_latch]() { ready_latch.Signal(); }));
  context.AddFfiNativeCallback(
      "SignalNativeMessage",
      CREATE_FFI_LAMBDA([&message_latch](Dart_Handle message_handle) {
        auto message =
            tonic::DartConverter<std::string>::FromDart(message_handle);
        ASSERT_EQ("Changed: [0]", message);
        message_latch.Signal();
      }));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  ready_latch.Wait();

  FlutterWindowMetricsEvent event = {};
  // Simulate an event that comes from an old version of embedder.h that doesn't
  // have the view_id field.
  event.struct_size = offsetof(FlutterWindowMetricsEvent, view_id);
  event.width = 200;
  event.height = 300;
  event.pixel_ratio = 1.5;
  // Skip assigning event.view_id here to test the default behavior.

  FlutterEngineResult result =
      FlutterEngineSendWindowMetricsEvent(engine.get(), &event);
  ASSERT_EQ(result, kSuccess);

  message_latch.Wait();
}

TEST_F(EmbedderTest, IgnoresWindowMetricsEventForUnknownView) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("window_metrics_event_view_id");

  fml::AutoResetWaitableEvent ready_latch, message_latch;
  context.AddFfiNativeCallback(
      "SignalNativeTest",
      CREATE_FFI_LAMBDA([&ready_latch]() { ready_latch.Signal(); }));

  context.AddFfiNativeCallback(
      "SignalNativeMessage",
      CREATE_FFI_LAMBDA([&message_latch](Dart_Handle message_handle) {
        auto message =
            tonic::DartConverter<std::string>::FromDart(message_handle);
        // Message latch should only be signaled once as the bad
        // view metric should be dropped by the engine.
        ASSERT_FALSE(message_latch.IsSignaledForTest());
        ASSERT_EQ("Changed: [0]", message);
        message_latch.Signal();
      }));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  ready_latch.Wait();

  // Send a window metric for a nonexistent view, which should be dropped by the
  // engine.
  FlutterWindowMetricsEvent bad_event = {};
  bad_event.struct_size = sizeof(FlutterWindowMetricsEvent);
  bad_event.width = 200;
  bad_event.height = 300;
  bad_event.pixel_ratio = 1.5;
  bad_event.view_id = 100;

  FlutterEngineResult result =
      FlutterEngineSendWindowMetricsEvent(engine.get(), &bad_event);
  ASSERT_EQ(result, kSuccess);

  // Send a window metric for a valid view. The engine notifies the Dart app.
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(FlutterWindowMetricsEvent);
  event.width = 200;
  event.height = 300;
  event.pixel_ratio = 1.5;
  event.view_id = 0;

  result = FlutterEngineSendWindowMetricsEvent(engine.get(), &event);
  ASSERT_EQ(result, kSuccess);

  message_latch.Wait();
}

TEST_F(EmbedderTest, RegisterChannelListener) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  fml::AutoResetWaitableEvent latch;
  fml::AutoResetWaitableEvent latch2;
  bool listening = false;
  context.AddFfiNativeCallback("SignalNativeTest",
                               CREATE_FFI_LAMBDA([&]() { latch.Signal(); }));
  context.SetChannelUpdateCallback([&](const FlutterChannelUpdate* update) {
    EXPECT_STREQ(update->channel, "test/listen");
    EXPECT_TRUE(update->listening);
    listening = true;
    latch2.Signal();
  });

  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("channel_listener_response");

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  latch.Wait();
  // Drain tasks posted to platform thread task runner.
  fml::MessageLoop::GetCurrent().RunExpiredTasksNow();
  latch2.Wait();

  ASSERT_TRUE(listening);
}

TEST_F(EmbedderTest, PlatformThreadIsolatesWithCustomPlatformTaskRunner) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  static fml::AutoResetWaitableEvent latch;

  static std::thread::id ffi_call_thread_id;
  static void (*ffi_signal_native_test)() = []() -> void {
    ffi_call_thread_id = std::this_thread::get_id();
    latch.Signal();
  };

  Dart_FfiNativeResolver ffi_resolver = [](const char* name,
                                           uintptr_t args_n) -> void* {
    if (std::string_view(name) == "FFISignalNativeTest") {
      return reinterpret_cast<void*>(ffi_signal_native_test);
    }
    return nullptr;
  };

  // The test's Dart code will call this native function which overrides the
  // FFI resolver.  After that, the Dart code will invoke the FFI function
  // using runOnPlatformThread.
  context.AddFfiNativeCallback("SignalNativeTest", CREATE_FFI_LAMBDA([&]() {
                                 Dart_SetFfiNativeResolver(Dart_RootLibrary(),
                                                           ffi_resolver);
                               }));

  auto platform_task_runner = CreateNewThread("test_platform_thread");

  UniqueEngine engine;

  EmbedderTestTaskRunner test_task_runner(
      platform_task_runner, [&](FlutterTask task) {
        if (!engine.is_valid()) {
          return;
        }
        FlutterEngineRunTask(engine.get(), &task);
      });

  std::thread::id platform_thread_id;
  platform_task_runner->PostTask([&]() {
    platform_thread_id = std::this_thread::get_id();

    EmbedderConfigBuilder builder(context);
    const auto task_runner_description =
        test_task_runner.GetFlutterTaskRunnerDescription();
    builder.SetSurface(DlISize(1, 1));
    builder.SetPlatformTaskRunner(&task_runner_description);
    builder.SetDartEntrypoint("invokePlatformThreadIsolate");
    builder.AddCommandLineArgument("--enable-platform-isolates");
    engine = builder.LaunchEngine();
    ASSERT_TRUE(engine.is_valid());
  });

  latch.Wait();

  fml::AutoResetWaitableEvent kill_latch;
  platform_task_runner->PostTask(fml::MakeCopyable([&]() mutable {
    engine.reset();

    platform_task_runner->PostTask([&kill_latch] { kill_latch.Signal(); });
  }));
  kill_latch.Wait();

  // Check that the FFI call was executed on the platform thread.
  ASSERT_EQ(platform_thread_id, ffi_call_thread_id);
}

TEST_F(EmbedderTest, CustomAssetResolverLoadsAssetAndInvokesReleaseCallback) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));

  static const std::string kAssetContent =
      "Custom asset payload from embedder!";
  static const std::string kAssetName = "custom_test_asset.bin";

  struct ResolverContext {
    bool release_called = false;
    int asset_requests = 0;
  };
  ResolverContext resolver_ctx;

  auto get_asset_cb = [](const char* name, FlutterMapping* mapping_out,
                         void* user_data) -> bool {
    auto* ctx = static_cast<ResolverContext*>(user_data);
    ctx->asset_requests++;
    if (std::string(name) == kAssetName) {
      mapping_out->struct_size = sizeof(FlutterMapping);
      mapping_out->mapping =
          reinterpret_cast<const uint8_t*>(kAssetContent.data());
      mapping_out->size = kAssetContent.size();
      mapping_out->user_data = user_data;
      mapping_out->release_callback = [](void* udata) {
        auto* c = static_cast<ResolverContext*>(udata);
        c->release_called = true;
      };
      return true;
    }
    return false;
  };

  FlutterAssetResolver asset_resolver = {
      .struct_size = sizeof(FlutterAssetResolver),
      .user_data = &resolver_ctx,
      .type = kFlutterAssetResolverTypeCustom,
      .get_asset_callback = get_asset_cb,
  };

  const FlutterAssetResolver* asset_resolvers[] = {&asset_resolver};
  builder.GetProjectArgs().asset_resolvers_count = 1;
  builder.GetProjectArgs().asset_resolvers = asset_resolvers;

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  auto* embedder_engine = reinterpret_cast<EmbedderEngine*>(engine.get());
  fml::AutoResetWaitableEvent latch;
  embedder_engine->GetTaskRunners().GetUITaskRunner()->PostTask([&]() {
    auto asset_manager =
        embedder_engine->GetShell().GetEngine()->GetAssetManager();
    EXPECT_NE(asset_manager, nullptr);

    // 1. Verify unknown asset returns nullptr
    auto unknown_mapping =
        asset_manager->GetAsMapping("non_existent_asset.txt");
    EXPECT_EQ(unknown_mapping, nullptr);

    // 2. Verify known asset is loaded successfully
    auto mapping = asset_manager->GetAsMapping(kAssetName);
    EXPECT_NE(mapping, nullptr);
    if (mapping) {
      EXPECT_EQ(mapping->GetSize(), kAssetContent.size());
      EXPECT_EQ(memcmp(mapping->GetMapping(), kAssetContent.data(),
                       kAssetContent.size()),
                0);

      // Release callback should not have been called yet
      EXPECT_FALSE(resolver_ctx.release_called);

      // 3. Reset mapping and verify release callback was invoked
      mapping.reset();
      EXPECT_TRUE(resolver_ctx.release_called);
    }
    latch.Signal();
  });
  latch.Wait();

  engine.reset();
}

TEST_F(EmbedderTest, InvalidAssetResolverArguments) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  FlutterAssetResolver config = {};
  config.struct_size = sizeof(FlutterAssetResolver);
  config.type = kFlutterAssetResolverTypeCustom;
  config.get_asset_callback = [](const char*, FlutterMapping*, void*) {
    return false;
  };

  // Invalid engine handle.
  EXPECT_EQ(FlutterEngineUpdateAssetResolver(nullptr, &config),
            kInvalidArguments);

  // Null config.
  EXPECT_EQ(FlutterEngineUpdateAssetResolver(engine.get(), nullptr),
            kInvalidArguments);

  // Invalid struct size.
  FlutterAssetResolver invalid_size_config = config;
  invalid_size_config.struct_size = sizeof(FlutterAssetResolver) - 1;
  EXPECT_EQ(
      FlutterEngineUpdateAssetResolver(engine.get(), &invalid_size_config),
      kInvalidArguments);

  // Null get_asset callback.
  FlutterAssetResolver null_cb_config = config;
  null_cb_config.get_asset_callback = nullptr;
  EXPECT_EQ(FlutterEngineUpdateAssetResolver(engine.get(), &null_cb_config),
            kInvalidArguments);

  // Startup validation: null array with non-zero count.
  EmbedderConfigBuilder invalid_builder(context);
  invalid_builder.SetSurface(DlISize(1, 1));
  invalid_builder.GetProjectArgs().asset_resolvers = nullptr;
  invalid_builder.GetProjectArgs().asset_resolvers_count = 1;
  auto invalid_engine = invalid_builder.LaunchEngine();
  EXPECT_FALSE(invalid_engine.is_valid());

  // Startup validation: null resolver in array.
  EmbedderConfigBuilder null_elem_builder(context);
  null_elem_builder.SetSurface(DlISize(1, 1));
  const FlutterAssetResolver* null_resolvers[] = {nullptr};
  null_elem_builder.GetProjectArgs().asset_resolvers = null_resolvers;
  null_elem_builder.GetProjectArgs().asset_resolvers_count = 1;
  auto null_elem_engine = null_elem_builder.LaunchEngine();
  EXPECT_FALSE(null_elem_engine.is_valid());

  engine.reset();
}

TEST_F(EmbedderTest, CanUpdateAssetResolver) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  FlutterAssetResolver config = {};
  config.struct_size = sizeof(FlutterAssetResolver);
  config.type = kFlutterAssetResolverTypeCustom;
  config.get_asset_callback = [](const char* name, FlutterMapping* mapping,
                                 void* user_data) -> bool {
    static const char* kData = "dynamically updated asset payload";
    mapping->struct_size = sizeof(FlutterMapping);
    mapping->mapping = reinterpret_cast<const uint8_t*>(kData);
    mapping->size = std::strlen(kData);
    return true;
  };

  EXPECT_EQ(FlutterEngineUpdateAssetResolver(engine.get(), &config), kSuccess);

  engine.reset();
}

TEST_F(EmbedderTest, DartDeferredLibraryCallbacks) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));

  static intptr_t s_requested_unit_id = -1;
  static void* s_received_user_data = nullptr;
  s_requested_unit_id = -1;
  s_received_user_data = nullptr;

  builder.GetProjectArgs().dart_deferred_library_request_callback =
      [](intptr_t loading_unit_id, void* user_data) {
        s_requested_unit_id = loading_unit_id;
        s_received_user_data = user_data;
      };

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  // Parameter validation: FlutterEngineLoadDartDeferredLibrary
  FlutterLoadDeferredLibraryInfo load_info = {};
  load_info.struct_size = sizeof(FlutterLoadDeferredLibraryInfo);
  load_info.loading_unit_id = 1;
  const uint8_t dummy_data[] = {0x00, 0x01};
  load_info.isolate_snapshot_data = dummy_data;
  load_info.isolate_snapshot_data_size = sizeof(dummy_data);
  load_info.isolate_snapshot_instructions = dummy_data;
  load_info.isolate_snapshot_instructions_size = sizeof(dummy_data);

  EXPECT_EQ(FlutterEngineLoadDartDeferredLibrary(nullptr, &load_info),
            kInvalidArguments);
  EXPECT_EQ(FlutterEngineLoadDartDeferredLibrary(engine.get(), nullptr),
            kInvalidArguments);

  FlutterLoadDeferredLibraryInfo invalid_size_load = load_info;
  invalid_size_load.struct_size = sizeof(FlutterLoadDeferredLibraryInfo) - 1;
  EXPECT_EQ(
      FlutterEngineLoadDartDeferredLibrary(engine.get(), &invalid_size_load),
      kInvalidArguments);

  FlutterLoadDeferredLibraryInfo null_data_load = load_info;
  null_data_load.isolate_snapshot_data = nullptr;
  EXPECT_EQ(FlutterEngineLoadDartDeferredLibrary(engine.get(), &null_data_load),
            kInvalidArguments);

  FlutterLoadDeferredLibraryInfo null_instr_load = load_info;
  null_instr_load.isolate_snapshot_instructions = nullptr;
  EXPECT_EQ(
      FlutterEngineLoadDartDeferredLibrary(engine.get(), &null_instr_load),
      kInvalidArguments);

  // Parameter validation: FlutterEngineLoadDartDeferredLibraryError
  FlutterLoadDeferredLibraryErrorInfo error_info = {};
  error_info.struct_size = sizeof(FlutterLoadDeferredLibraryErrorInfo);
  error_info.loading_unit_id = 1;
  error_info.error_message = "Test error";
  error_info.transient = false;

  EXPECT_EQ(FlutterEngineLoadDartDeferredLibraryError(nullptr, &error_info),
            kInvalidArguments);
  EXPECT_EQ(FlutterEngineLoadDartDeferredLibraryError(engine.get(), nullptr),
            kInvalidArguments);

  FlutterLoadDeferredLibraryErrorInfo invalid_size_error = error_info;
  invalid_size_error.struct_size =
      sizeof(FlutterLoadDeferredLibraryErrorInfo) - 1;
  EXPECT_EQ(FlutterEngineLoadDartDeferredLibraryError(engine.get(),
                                                      &invalid_size_error),
            kInvalidArguments);

  FlutterLoadDeferredLibraryErrorInfo null_msg_error = error_info;
  null_msg_error.error_message = nullptr;
  EXPECT_EQ(
      FlutterEngineLoadDartDeferredLibraryError(engine.get(), &null_msg_error),
      kInvalidArguments);

  // Proc table verification
  FlutterEngineProcTable ptable = {};
  ptable.struct_size = sizeof(FlutterEngineProcTable);
  EXPECT_EQ(FlutterEngineGetProcAddresses(&ptable), kSuccess);
  EXPECT_NE(ptable.LoadDartDeferredLibrary, nullptr);
  EXPECT_NE(ptable.LoadDartDeferredLibraryError, nullptr);

  // Request dispatching verification
  auto embedder_engine =
      reinterpret_cast<flutter::EmbedderEngine*>(engine.get());
  embedder_engine->GetShell().GetPlatformView()->RequestDartDeferredLibrary(42);
  EXPECT_EQ(s_requested_unit_id, 42);
  EXPECT_EQ(s_received_user_data, &context);

  // Successful error notification dispatching
  EXPECT_EQ(
      FlutterEngineLoadDartDeferredLibraryError(engine.get(), &error_info),
      kSuccess);

  // Successful deferred library loading dispatching
  EXPECT_EQ(FlutterEngineLoadDartDeferredLibrary(engine.get(), &load_info),
            kSuccess);

  engine.reset();
}

/// Verify that Dart calling PlatformDispatcher.instance.setApplicationLocale
/// dispatches to FlutterProjectArgs::set_application_locale_callback.
TEST_F(EmbedderTest, CanReceiveApplicationLocale) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("set_application_locale_main");

  fml::AutoResetWaitableEvent ready_latch, locale_latch;
  context.AddFfiNativeCallback(
      "SignalNativeTest",
      CREATE_FFI_LAMBDA([&ready_latch]() { ready_latch.Signal(); }));

  static std::string s_received_locale;
  static void* s_received_user_data = nullptr;
  static fml::AutoResetWaitableEvent* s_locale_latch = nullptr;
  s_received_locale.clear();
  s_received_user_data = nullptr;
  s_locale_latch = &locale_latch;

  builder.GetProjectArgs().set_application_locale_callback =
      [](const char* locale, void* user_data) {
        if (locale != nullptr) {
          s_received_locale = locale;
        }
        s_received_user_data = user_data;
        if (s_locale_latch != nullptr) {
          s_locale_latch->Signal();
        }
      };

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  ready_latch.Wait();
  locale_latch.Wait();

  EXPECT_EQ(s_received_locale, "pt-BR");
  EXPECT_EQ(s_received_user_data, &context);

  engine.reset();
}

/// Verify that FlutterProjectArgs::get_scaled_font_size_callback is dispatched
/// by PlatformView::GetScaledFontSize.
TEST_F(EmbedderTest, CanGetScaledFontSize) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));

  static double s_received_font_size = 0.0;
  static int s_received_configuration_id = -1;
  static void* s_received_user_data = nullptr;
  s_received_font_size = 0.0;
  s_received_configuration_id = -1;
  s_received_user_data = nullptr;

  builder.GetProjectArgs().get_scaled_font_size_callback =
      [](double font_size, int configuration_id, void* user_data) -> double {
    s_received_font_size = font_size;
    s_received_configuration_id = configuration_id;
    s_received_user_data = user_data;
    return font_size * 2.5;
  };

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  auto embedder_engine =
      reinterpret_cast<flutter::EmbedderEngine*>(engine.get());
  double scaled =
      embedder_engine->GetShell().GetPlatformView()->GetScaledFontSize(16.0,
                                                                       101);

  EXPECT_DOUBLE_EQ(scaled, 40.0);
  EXPECT_DOUBLE_EQ(s_received_font_size, 16.0);
  EXPECT_EQ(s_received_configuration_id, 101);
  EXPECT_EQ(s_received_user_data, &context);

  engine.reset();
}

TEST_F(EmbedderTest, GetScaledFontSizeDefaultsToUnscaled) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  auto embedder_engine =
      reinterpret_cast<flutter::EmbedderEngine*>(engine.get());
  double scaled =
      embedder_engine->GetShell().GetPlatformView()->GetScaledFontSize(18.5,
                                                                       101);

  EXPECT_DOUBLE_EQ(scaled, 18.5);

  engine.reset();
}

TEST_F(EmbedderTest, CanSpecifyCustomIOTaskRunner) {
  std::mutex engine_mutex;
  UniqueEngine engine;
  auto io_task_runner = CreateNewThread("test_io_thread");
  std::atomic<bool> io_task_ran = false;

  EmbedderTestTaskRunner test_io_task_runner(
      io_task_runner, [&](FlutterTask task) {
        std::scoped_lock engine_lock(engine_mutex);
        if (engine.is_valid()) {
          ASSERT_EQ(FlutterEngineRunTask(engine.get(), &task), kSuccess);
          io_task_ran = true;
        }
      });

  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  const auto io_desc = test_io_task_runner.GetFlutterTaskRunnerDescription();
  builder.SetSurface(DlISize(1, 1));
  builder.SetIOTaskRunner(&io_desc);

  {
    std::scoped_lock lock(engine_mutex);
    engine = builder.InitializeEngine();
  }
  ASSERT_TRUE(engine.is_valid());
  ASSERT_EQ(FlutterEngineRunInitialized(engine.get()), kSuccess);

  EXPECT_TRUE(io_task_ran.load());

  ASSERT_EQ(FlutterEngineDeinitialize(engine.get()), kSuccess);
  {
    std::scoped_lock engine_lock(engine_mutex);
    engine.reset();
  }
}

TEST_F(EmbedderTest, CanSpecifyCustomTaskRunnerThreadPriorities) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  auto platform_task_runner = CreateNewThread("test_platform_thread");
  UniqueEngine engine;

  fml::AutoResetWaitableEvent priority_latch;
  std::atomic<FlutterThreadPriority> recorded_priority =
      FlutterThreadPriority::kNormal;

  EmbedderTestTaskRunner test_platform_task_runner(
      platform_task_runner, [&](FlutterTask task) {
        if (!engine.is_valid()) {
          return;
        }
        FlutterEngineRunTask(engine.get(), &task);
      });
  test_platform_task_runner.SetThreadPriority(FlutterThreadPriority::kRaster);
  static fml::AutoResetWaitableEvent* s_priority_latch = &priority_latch;
  static std::atomic<FlutterThreadPriority>* s_recorded_priority =
      &recorded_priority;
  test_platform_task_runner.SetThreadPrioritySetter(
      [](FlutterThreadPriority priority) {
        s_recorded_priority->store(priority);
        if (s_priority_latch != nullptr) {
          s_priority_latch->Signal();
        }
      });

  platform_task_runner->PostTask([&]() {
    EmbedderConfigBuilder builder(context);
    const auto platform_desc =
        test_platform_task_runner.GetFlutterTaskRunnerDescription();
    builder.SetSurface(DlISize(1, 1));
    builder.SetPlatformTaskRunner(&platform_desc);
    engine = builder.LaunchEngine();
    ASSERT_TRUE(engine.is_valid());
  });

  priority_latch.Wait();
  EXPECT_EQ(recorded_priority.load(), FlutterThreadPriority::kRaster);

  fml::AutoResetWaitableEvent kill_latch;
  platform_task_runner->PostTask([&] {
    engine.reset();
    kill_latch.Signal();
  });
  kill_latch.Wait();
}

TEST_F(EmbedderTest, CanSpecifyCustomTaskRunnerThreadPriorityWithUserData) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  auto platform_task_runner = CreateNewThread("test_platform_thread");
  UniqueEngine engine;

  fml::AutoResetWaitableEvent priority_latch;
  std::atomic<FlutterThreadPriority> recorded_priority =
      FlutterThreadPriority::kNormal;
  void* recorded_user_data = nullptr;

  EmbedderTestTaskRunner test_platform_task_runner(
      platform_task_runner, [&](FlutterTask task) {
        if (!engine.is_valid()) {
          return;
        }
        FlutterEngineRunTask(engine.get(), &task);
      });
  test_platform_task_runner.SetThreadPriority(FlutterThreadPriority::kDisplay);
  static fml::AutoResetWaitableEvent* s_priority_latch2 = &priority_latch;
  static std::atomic<FlutterThreadPriority>* s_recorded_priority2 =
      &recorded_priority;
  static void** s_recorded_user_data2 = &recorded_user_data;
  test_platform_task_runner.SetThreadPrioritySetterWithUserData(
      [](FlutterThreadPriority priority, void* user_data) {
        s_recorded_priority2->store(priority);
        *s_recorded_user_data2 = user_data;
        if (s_priority_latch2 != nullptr) {
          s_priority_latch2->Signal();
        }
      });

  platform_task_runner->PostTask([&]() {
    EmbedderConfigBuilder builder(context);
    const auto platform_desc =
        test_platform_task_runner.GetFlutterTaskRunnerDescription();
    builder.SetSurface(DlISize(1, 1));
    builder.SetPlatformTaskRunner(&platform_desc);
    engine = builder.LaunchEngine();
    ASSERT_TRUE(engine.is_valid());
  });

  priority_latch.Wait();
  EXPECT_EQ(recorded_priority.load(), FlutterThreadPriority::kDisplay);
  EXPECT_EQ(recorded_user_data, &test_platform_task_runner);

  fml::AutoResetWaitableEvent kill_latch;
  platform_task_runner->PostTask([&] {
    engine.reset();
    kill_latch.Signal();
  });
  kill_latch.Wait();
}

TEST_F(EmbedderTest, CanSetEngineThreadPrioritiesWithGlobalSetter) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));

  static std::vector<FlutterThreadPriority> s_priorities;
  static std::mutex s_priorities_mutex;
  s_priorities.clear();

  builder.SetThreadPrioritySetter([](FlutterThreadPriority priority) {
    std::scoped_lock lock(s_priorities_mutex);
    s_priorities.push_back(priority);
  });

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  {
    std::scoped_lock lock(s_priorities_mutex);
    EXPECT_FALSE(s_priorities.empty());
  }

  engine.reset();
}

TEST_F(EmbedderTest, CanSetEngineThreadPrioritiesWithUserDataSetter) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));

  int dummy_context = 42;
  static std::vector<std::pair<FlutterThreadPriority, void*>> s_calls;
  static std::mutex s_calls_mutex;
  s_calls.clear();

  builder.SetThreadPrioritySetterWithUserData(
      [](FlutterThreadPriority priority, void* user_data) {
        std::scoped_lock lock(s_calls_mutex);
        s_calls.emplace_back(priority, user_data);
      },
      &dummy_context);

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  {
    std::scoped_lock lock(s_calls_mutex);
    EXPECT_FALSE(s_calls.empty());
    for (const auto& call : s_calls) {
      EXPECT_EQ(call.second, &dummy_context);
    }
  }

  engine.reset();
}

TEST_F(EmbedderTest, EngineManagedIOThreadPriorityConfig) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));

  static std::vector<FlutterThreadPriority> s_observed_priorities;
  static std::mutex s_priorities_mutex;
  s_observed_priorities.clear();

  builder.SetIOThreadPriority(FlutterThreadPriority::kNormal);
  builder.SetThreadPrioritySetter([](FlutterThreadPriority priority) {
    std::scoped_lock lock(s_priorities_mutex);
    s_observed_priorities.push_back(priority);
  });

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  {
    std::scoped_lock lock(s_priorities_mutex);
    bool found_normal = false;
    for (auto p : s_observed_priorities) {
      if (p == FlutterThreadPriority::kNormal) {
        found_normal = true;
        break;
      }
    }
    EXPECT_TRUE(found_normal);
  }

  engine.reset();
}

TEST_F(EmbedderTest, EmbedderTaskRunnerSetThreadPriorityAtRuntime) {
  fml::AutoResetWaitableEvent latch;
  std::atomic<FlutterThreadPriority> updated_priority =
      FlutterThreadPriority::kNormal;

  EmbedderTaskRunner::DispatchTable table = {
      .post_task_callback = [](EmbedderTaskRunner* runner, uint64_t baton,
                               fml::TimePoint time) {},
      .runs_task_on_current_thread_callback = []() { return true; },
      .destruction_callback = []() {},
      .thread_priority_setter =
          [&](FlutterThreadPriority priority) {
            updated_priority.store(priority);
            latch.Signal();
          },
  };

  auto runner = fml::MakeRefCounted<EmbedderTaskRunner>(
      table, 123u, FlutterThreadPriority::kBackground);
  EXPECT_EQ(runner->GetThreadPriority(), FlutterThreadPriority::kBackground);

  runner->SetThreadPriority(FlutterThreadPriority::kRaster);
  latch.Wait();

  EXPECT_EQ(runner->GetThreadPriority(), FlutterThreadPriority::kRaster);
  EXPECT_EQ(updated_priority.load(), FlutterThreadPriority::kRaster);
}

TEST_F(EmbedderTest, CustomTaskRunnersSafeAccessTruncatedStruct) {
  struct LegacyFlutterCustomTaskRunners {
    size_t struct_size;
    const FlutterTaskRunnerDescription* platform_task_runner;
    const FlutterTaskRunnerDescription* render_task_runner;
    void (*thread_priority_setter)(FlutterThreadPriority);
    const FlutterTaskRunnerDescription* ui_task_runner;
  };

  LegacyFlutterCustomTaskRunners legacy_runners = {};
  legacy_runners.struct_size = sizeof(LegacyFlutterCustomTaskRunners);

  auto host = EmbedderThreadHost::CreateEmbedderOrEngineManagedThreadHost(
      reinterpret_cast<const FlutterCustomTaskRunners*>(&legacy_runners));
  ASSERT_NE(host, nullptr);
  EXPECT_TRUE(host->IsValid());
}

TEST_F(EmbedderTest, PlatformMessageRoutingDefaultPlatformThread) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  auto platform_thread = std::make_unique<fml::Thread>("test_platform_thread");
  auto ui_thread = std::make_unique<fml::Thread>("test_ui_thread");

  std::mutex ui_task_runner_mutex;
  bool ui_task_runner_destroyed = false;
  auto ui_task_runner = ui_thread->GetTaskRunner();
  auto platform_task_runner = platform_thread->GetTaskRunner();
  UniqueEngine engine;

  EmbedderTestTaskRunner test_ui_task_runner =
      EmbedderTestTaskRunnerBuilder()
          .SetRealTaskRunner(ui_task_runner)
          .SetTaskExpiryCallback([&](FlutterTask task) {
            std::scoped_lock lock(ui_task_runner_mutex);
            if (ui_task_runner_destroyed) {
              return;
            }
            FlutterEngineRunTask(engine.get(), &task);
          })
          .SetDestructionCallback([&]() {
            std::scoped_lock lock(ui_task_runner_mutex);
            ui_task_runner_destroyed = true;
          })
          .Build();

  EmbedderTestTaskRunner test_platform_task_runner =
      EmbedderTestTaskRunnerBuilder()
          .SetRealTaskRunner(platform_task_runner)
          .SetTaskExpiryCallback([&](FlutterTask task) {
            if (!engine.is_valid()) {
              return;
            }
            FlutterEngineRunTask(engine.get(), &task);
          })
          .Build();

  fml::AutoResetWaitableEvent signal_latch_ui;
  fml::AutoResetWaitableEvent signal_latch_platform;

  context.AddFfiNativeCallback(
      "SignalNativeTest", CREATE_FFI_LAMBDA([&]() {
        ASSERT_TRUE(ui_task_runner->RunsTasksOnCurrentThread());
        signal_latch_ui.Signal();
      }));

  platform_task_runner->PostTask([&]() {
    EmbedderConfigBuilder builder(context);
    const auto ui_task_runner_description =
        test_ui_task_runner.GetFlutterTaskRunnerDescription();
    const auto platform_task_runner_description =
        test_platform_task_runner.GetFlutterTaskRunnerDescription();
    builder.SetSurface(DlISize(1, 1));
    builder.SetUITaskRunner(&ui_task_runner_description);
    builder.SetPlatformTaskRunner(&platform_task_runner_description);
    builder.SetDartEntrypoint("canSpecifyCustomUITaskRunner");
    builder.SetPlatformMessageRouting(
        kFlutterPlatformMessageRoutingPlatformThread);
    builder.SetPlatformMessageCallback(
        [&](const FlutterPlatformMessage* message) {
          ASSERT_TRUE(platform_task_runner->RunsTasksOnCurrentThread());
          ASSERT_FALSE(ui_task_runner->RunsTasksOnCurrentThread());
          signal_latch_platform.Signal();
        });
    engine = builder.InitializeEngine();
    ASSERT_EQ(FlutterEngineRunInitialized(engine.get()), kSuccess);
    ASSERT_TRUE(engine.is_valid());
  });
  signal_latch_ui.Wait();
  signal_latch_platform.Wait();

  fml::AutoResetWaitableEvent kill_latch;
  platform_task_runner->PostTask([&] {
    engine.reset();
    platform_task_runner->PostTask([&kill_latch] { kill_latch.Signal(); });
  });
  kill_latch.Wait();

  ui_thread.reset();
  platform_thread.reset();
}

TEST_F(EmbedderTest, PlatformMessageRoutingCallingThread) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  auto platform_thread = std::make_unique<fml::Thread>("test_platform_thread");
  auto ui_thread = std::make_unique<fml::Thread>("test_ui_thread");

  std::mutex ui_task_runner_mutex;
  bool ui_task_runner_destroyed = false;
  auto ui_task_runner = ui_thread->GetTaskRunner();
  auto platform_task_runner = platform_thread->GetTaskRunner();
  UniqueEngine engine;

  EmbedderTestTaskRunner test_ui_task_runner =
      EmbedderTestTaskRunnerBuilder()
          .SetRealTaskRunner(ui_task_runner)
          .SetTaskExpiryCallback([&](FlutterTask task) {
            std::scoped_lock lock(ui_task_runner_mutex);
            if (ui_task_runner_destroyed) {
              return;
            }
            FlutterEngineRunTask(engine.get(), &task);
          })
          .SetDestructionCallback([&]() {
            std::scoped_lock lock(ui_task_runner_mutex);
            ui_task_runner_destroyed = true;
          })
          .Build();

  EmbedderTestTaskRunner test_platform_task_runner =
      EmbedderTestTaskRunnerBuilder()
          .SetRealTaskRunner(platform_task_runner)
          .SetTaskExpiryCallback([&](FlutterTask task) {
            if (!engine.is_valid()) {
              return;
            }
            FlutterEngineRunTask(engine.get(), &task);
          })
          .Build();

  fml::AutoResetWaitableEvent signal_latch_ui;
  fml::AutoResetWaitableEvent signal_latch_calling_thread;

  context.AddFfiNativeCallback(
      "SignalNativeTest", CREATE_FFI_LAMBDA([&]() {
        ASSERT_TRUE(ui_task_runner->RunsTasksOnCurrentThread());
        signal_latch_ui.Signal();
      }));

  platform_task_runner->PostTask([&]() {
    EmbedderConfigBuilder builder(context);
    const auto ui_task_runner_description =
        test_ui_task_runner.GetFlutterTaskRunnerDescription();
    const auto platform_task_runner_description =
        test_platform_task_runner.GetFlutterTaskRunnerDescription();
    builder.SetSurface(DlISize(1, 1));
    builder.SetUITaskRunner(&ui_task_runner_description);
    builder.SetPlatformTaskRunner(&platform_task_runner_description);
    builder.SetDartEntrypoint("canSpecifyCustomUITaskRunner");
    builder.SetPlatformMessageRouting(
        kFlutterPlatformMessageRoutingCallingThread);
    builder.SetPlatformMessageCallback(
        [&](const FlutterPlatformMessage* message) {
          ASSERT_TRUE(ui_task_runner->RunsTasksOnCurrentThread());
          ASSERT_FALSE(platform_task_runner->RunsTasksOnCurrentThread());
          signal_latch_calling_thread.Signal();
        });
    engine = builder.InitializeEngine();
    ASSERT_EQ(FlutterEngineRunInitialized(engine.get()), kSuccess);
    ASSERT_TRUE(engine.is_valid());
  });
  signal_latch_ui.Wait();
  signal_latch_calling_thread.Wait();

  fml::AutoResetWaitableEvent kill_latch;
  platform_task_runner->PostTask([&] {
    engine.reset();
    platform_task_runner->PostTask([&kill_latch] { kill_latch.Signal(); });
  });
  kill_latch.Wait();

  ui_thread.reset();
  platform_thread.reset();
}

TEST_F(EmbedderTest, PlatformMessageRoutingSafeAccessTruncatedStruct) {
  FlutterProjectArgs args{};
  const FlutterProjectArgs* args_ptr = &args;
  args.struct_size = offsetof(FlutterProjectArgs, platform_message_routing);
  auto routing = SAFE_ACCESS(args_ptr, platform_message_routing,
                             kFlutterPlatformMessageRoutingPlatformThread);
  ASSERT_EQ(routing, kFlutterPlatformMessageRoutingPlatformThread);

  args.struct_size = sizeof(FlutterProjectArgs);
  args.platform_message_routing = kFlutterPlatformMessageRoutingCallingThread;
  routing = SAFE_ACCESS(args_ptr, platform_message_routing,
                        kFlutterPlatformMessageRoutingPlatformThread);
  ASSERT_EQ(routing, kFlutterPlatformMessageRoutingCallingThread);
}

TEST_F(EmbedderTest, CallbackInfoArgumentValidation) {
  FlutterCallbackInformation info = {};
  info.struct_size = sizeof(FlutterCallbackInformation);

  // Null info_out pointer.
  ASSERT_EQ(FlutterEngineGetCallbackInformation(1, nullptr), kInvalidArguments);

  // Invalid struct_size (0).
  info.struct_size = 0;
  ASSERT_EQ(FlutterEngineGetCallbackInformation(1, &info), kInvalidArguments);

  // Invalid struct_size (truncated / wrong size).
  info.struct_size = sizeof(FlutterCallbackInformation) - 1;
  ASSERT_EQ(FlutterEngineGetCallbackInformation(1, &info), kInvalidArguments);

  // Valid struct size, but non-existent callback handle.
  info.struct_size = sizeof(FlutterCallbackInformation);
  ASSERT_EQ(FlutterEngineGetCallbackInformation(0x1234567890ABCDEFLL, &info),
            kInternalInconsistency);
}

TEST_F(EmbedderTest, CallbackInfoLookupClassMethod) {
  int64_t handle = DartCallbackCache::GetCallbackHandle(
      "testClassCallback", "TestTargetClass", "package:test/class_method.dart");
  ASSERT_NE(handle, 0);

  FlutterCallbackInformation info = {};
  info.struct_size = sizeof(FlutterCallbackInformation);

  ASSERT_EQ(FlutterEngineGetCallbackInformation(handle, &info), kSuccess);
  ASSERT_STREQ(info.callback_name, "testClassCallback");
  ASSERT_STREQ(info.class_name, "TestTargetClass");
  ASSERT_STREQ(info.library_path, "package:test/class_method.dart");
}

TEST_F(EmbedderTest, CallbackInfoLookupTopLevelFunction) {
  int64_t handle = DartCallbackCache::GetCallbackHandle(
      "topLevelCallback", "", "package:test/top_level.dart");
  ASSERT_NE(handle, 0);

  FlutterCallbackInformation info = {};
  info.struct_size = sizeof(FlutterCallbackInformation);

  ASSERT_EQ(FlutterEngineGetCallbackInformation(handle, &info), kSuccess);
  ASSERT_STREQ(info.callback_name, "topLevelCallback");
  ASSERT_STREQ(info.class_name, "");
  ASSERT_STREQ(info.library_path, "package:test/top_level.dart");
}

TEST_F(EmbedderTest, CallbackInfoProcTable) {
  FlutterEngineProcTable table = {};
  table.struct_size = sizeof(FlutterEngineProcTable);

  ASSERT_EQ(FlutterEngineGetProcAddresses(&table), kSuccess);
  ASSERT_NE(table.GetCallbackInformation, nullptr);
  ASSERT_EQ(table.GetCallbackInformation, &FlutterEngineGetCallbackInformation);

  int64_t handle = DartCallbackCache::GetCallbackHandle(
      "procTableCallback", "ProcTableClass", "package:test/proc_table.dart");
  ASSERT_NE(handle, 0);

  FlutterCallbackInformation info = {};
  info.struct_size = sizeof(FlutterCallbackInformation);

  ASSERT_EQ(table.GetCallbackInformation(handle, &info), kSuccess);
  ASSERT_STREQ(info.callback_name, "procTableCallback");
  ASSERT_STREQ(info.class_name, "ProcTableClass");
  ASSERT_STREQ(info.library_path, "package:test/proc_table.dart");
}

TEST_F(EmbedderTest, DynamicThreadMergingNullCallbackError) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(800, 600));
  builder.SetCompositor();
  builder.SetDartEntrypoint("render_implicit_view");
  builder.SetRenderTargetType(
      EmbedderTestBackingStoreProducer::RenderTargetType::kSoftwareBuffer);

  // Enabling dynamic thread merging without post_preroll_callback must fail
  // (Invariant I-1).
  builder.GetCompositor().supports_dynamic_thread_merging = true;
  builder.GetCompositor().post_preroll_callback = nullptr;

  auto engine = builder.InitializeEngine();
  EXPECT_FALSE(engine.is_valid());
}

TEST_F(EmbedderTest, DynamicThreadMergingNullMergerSafe) {
  EXPECT_FALSE(FlutterRasterThreadMergerIsMerged(nullptr));
  EXPECT_FALSE(FlutterRasterThreadMergerIsOnPlatformThread(nullptr));
  // These should be safe no-ops and not crash.
  FlutterRasterThreadMergerMergeWithLease(nullptr, 2);
  FlutterRasterThreadMergerExtendLeaseTo(nullptr, 5);
}

TEST_F(EmbedderTest, DynamicThreadMergingProcTable) {
  FlutterEngineProcTable table = {};
  table.struct_size = sizeof(FlutterEngineProcTable);
  ASSERT_EQ(FlutterEngineGetProcAddresses(&table), kSuccess);
  ASSERT_NE(table.RasterThreadMergerIsMerged, nullptr);
  ASSERT_NE(table.RasterThreadMergerIsOnPlatformThread, nullptr);
  ASSERT_NE(table.RasterThreadMergerMergeWithLease, nullptr);
  ASSERT_NE(table.RasterThreadMergerExtendLeaseTo, nullptr);
}

struct DynamicThreadMergingState {
  std::atomic<size_t> begin_frame_count{0};
  std::atomic<size_t> post_preroll_count{0};
  std::atomic<size_t> end_frame_count{0};
  std::atomic<bool> begin_frame_saw_merger{false};
  std::atomic<bool> post_preroll_saw_merger{false};
  std::atomic<bool> end_frame_saw_merger{false};
  std::atomic<bool> saw_initial_unmerged{false};
  std::atomic<bool> saw_merged_on_retry{false};
  fml::AutoResetWaitableEvent* end_frame_latch = nullptr;
};

static DynamicThreadMergingState* g_dynamic_thread_merging_state = nullptr;

TEST_F(EmbedderTest, DynamicThreadMergingLifecycle) {
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
  fml::Thread thread;
  UniqueEngine engine;

  DynamicThreadMergingState state;
  g_dynamic_thread_merging_state = &state;

  fml::AutoResetWaitableEvent present_latch;
  fml::AutoResetWaitableEvent end_frame_latch;
  state.end_frame_latch = &end_frame_latch;

  thread.GetTaskRunner()->PostTask([&]() {
    EmbedderConfigBuilder builder(context);
    builder.SetSurface(DlISize(800, 600));
    builder.SetCompositor();
    builder.SetDartEntrypoint("render_implicit_view");
    builder.SetRenderTargetType(
        EmbedderTestBackingStoreProducer::RenderTargetType::kSoftwareBuffer);

    builder.GetCompositor().supports_dynamic_thread_merging = true;
    builder.GetCompositor().begin_frame_callback =
        [](const FlutterFrameThreadingInfo* info) {
          if (!g_dynamic_thread_merging_state || !info) {
            return;
          }
          EXPECT_EQ(info->struct_size, sizeof(FlutterFrameThreadingInfo));
          g_dynamic_thread_merging_state->begin_frame_count++;
          if (info->thread_merger != nullptr) {
            g_dynamic_thread_merging_state->begin_frame_saw_merger = true;
          }
        };
    builder.GetCompositor().post_preroll_callback =
        [](const FlutterFrameThreadingInfo* info) -> FlutterPostPrerollResult {
      if (!g_dynamic_thread_merging_state || !info) {
        return kFlutterPostPrerollResultSuccess;
      }
      EXPECT_EQ(info->struct_size, sizeof(FlutterFrameThreadingInfo));
      size_t count = ++g_dynamic_thread_merging_state->post_preroll_count;
      if (info->thread_merger != nullptr) {
        g_dynamic_thread_merging_state->post_preroll_saw_merger = true;
        if (!FlutterRasterThreadMergerIsMerged(info->thread_merger)) {
          g_dynamic_thread_merging_state->saw_initial_unmerged = true;
        }
      }

      if (count == 1) {
        // Frame 1: Merge threads with a lease and retry the frame.
        if (info->thread_merger != nullptr) {
          FlutterRasterThreadMergerMergeWithLease(info->thread_merger, 2);
        }
        return kFlutterPostPrerollResultSkipAndRetryFrame;
      } else if (count == 2) {
        // Frame 2 (retry): Verify threads are now merged, extend lease, and
        // resubmit.
        if (info->thread_merger != nullptr) {
          if (FlutterRasterThreadMergerIsMerged(info->thread_merger) &&
              FlutterRasterThreadMergerIsOnPlatformThread(
                  info->thread_merger)) {
            g_dynamic_thread_merging_state->saw_merged_on_retry = true;
          }
          FlutterRasterThreadMergerExtendLeaseTo(info->thread_merger, 5);
        }
        return kFlutterPostPrerollResultResubmitFrame;
      }

      // Frame 3 (resubmit): Proceed to rasterize and present.
      return kFlutterPostPrerollResultSuccess;
    };
    builder.GetCompositor().end_frame_callback =
        [](const FlutterFrameThreadingInfo* info) {
          if (!g_dynamic_thread_merging_state || !info) {
            return;
          }
          EXPECT_EQ(info->struct_size, sizeof(FlutterFrameThreadingInfo));
          size_t end_count = ++g_dynamic_thread_merging_state->end_frame_count;
          if (info->thread_merger != nullptr) {
            g_dynamic_thread_merging_state->end_frame_saw_merger = true;
          }
          if (end_count >= 3 &&
              g_dynamic_thread_merging_state->end_frame_latch != nullptr) {
            g_dynamic_thread_merging_state->end_frame_latch->Signal();
          }
        };

    context.GetCompositor().SetNextPresentCallback(
        [&](FlutterViewId view_id, const FlutterLayer** layers,
            size_t layers_count) {
          ASSERT_EQ(view_id, kFlutterImplicitViewId);
          present_latch.Signal();
        });

    engine = builder.LaunchEngine();
    ASSERT_TRUE(engine.is_valid());

    FlutterWindowMetricsEvent event = {};
    event.struct_size = sizeof(event);
    event.width = 300;
    event.height = 200;
    event.pixel_ratio = 1.0;
    ASSERT_EQ(FlutterEngineSendWindowMetricsEvent(engine.get(), &event),
              kSuccess);
  });

  present_latch.Wait();
  end_frame_latch.Wait();

  EXPECT_GE(state.begin_frame_count.load(), 3u);
  EXPECT_GE(state.post_preroll_count.load(), 3u);
  EXPECT_GE(state.end_frame_count.load(), 3u);
  EXPECT_TRUE(state.begin_frame_saw_merger.load());
  EXPECT_TRUE(state.post_preroll_saw_merger.load());
  EXPECT_TRUE(state.end_frame_saw_merger.load());
  EXPECT_TRUE(state.saw_initial_unmerged.load());
  EXPECT_TRUE(state.saw_merged_on_retry.load());

  fml::AutoResetWaitableEvent kill_latch;
  thread.GetTaskRunner()->PostTask([&] {
    engine.reset();
    kill_latch.Signal();
  });
  kill_latch.Wait();

  g_dynamic_thread_merging_state = nullptr;
}

}  // namespace testing
}  // namespace flutter

// NOLINTEND(clang-analyzer-core.StackAddressEscape)
