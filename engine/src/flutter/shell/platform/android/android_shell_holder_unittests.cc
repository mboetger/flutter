// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <android/looper.h>

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "flutter/flow/frame_timings.h"
#include "flutter/fml/time/time_delta.h"
#include "flutter/fml/time/time_point.h"
#include "flutter/runtime/runtime_controller.h"
#include "flutter/shell/common/engine.h"
#include "flutter/shell/common/shell.h"
#include "flutter/shell/common/vsync_waiter.h"
#include "flutter/shell/platform/android/android_shell_holder.h"
#include "flutter/shell/platform/embedder/test_utils/proc_table_replacement.h"
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
              (const std::string& channel,
               UniquePlatformMessagePayload payload,
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
  MOCK_METHOD(void,
              MaybeResizeSurfaceView,
              (int32_t width, int32_t height),
              (const, override));
};

// How long a test waits for a vsync. At 60 Hz this is 120 frames, so a busy
// emulator does not flake.
constexpr fml::TimeDelta kVsyncTimeout = fml::TimeDelta::FromSeconds(2);

// How long the test thread keeps dispatching after the event it waited for,
// to catch duplicate deliveries. At 60 Hz this is 6 frames.
constexpr fml::TimeDelta kSettleTime = fml::TimeDelta::FromMilliseconds(100);

// Upper bound for one ALooper_pollOnce call while pumping, in milliseconds.
// Short, so the pump notices completion within a fraction of a frame.
constexpr int kPollIntervalMs = 5;

// Frame interval the vsync waiter reports before Java sets a refresh rate.
// vsync_waiter_android.cc starts at 60 Hz and converts 1e9 ns / 60 to whole
// nanoseconds by truncation.
constexpr fml::TimeDelta kDefaultFrameInterval =
    fml::TimeDelta::FromNanoseconds(16'666'666);

// Creates a holder without calling NotifyCreated: a fake window makes the
// Impeller GLES reactor abort on devices.
std::unique_ptr<AndroidShellHolder> CreateHolderWithoutSurface(
    Settings::MergedPlatformUIThread merged_platform_ui_thread) {
  Settings settings;
  settings.enable_software_rendering = false;
  settings.merged_platform_ui_thread = merged_platform_ui_thread;
  return std::make_unique<AndroidShellHolder>(
      settings, std::make_shared<MockPlatformViewAndroidJNI>(),
      AndroidRenderingAPI::kImpellerOpenGLES);
}

// Dispatches the ALooper of the current thread, which runs both fml tasks and
// Choreographer frame callbacks, until |done| returns true or |timeout|
// passes. With merged threads the UI task runner is the test thread, so
// waiting on a latch instead would stall the vsync it waits for.
bool PumpLooperUntil(const std::function<bool()>& done,
                     fml::TimeDelta timeout) {
  const fml::TimePoint deadline = fml::TimePoint::Now() + timeout;
  while (!done()) {
    if (fml::TimePoint::Now() >= deadline) {
      return false;
    }
    if (ALooper_pollOnce(kPollIntervalMs, nullptr, nullptr, nullptr) ==
        ALOOPER_POLL_ERROR) {
      ADD_FAILURE() << "ALooper_pollOnce failed on the test thread.";
      return false;
    }
  }
  return true;
}

void PumpLooperFor(fml::TimeDelta duration) {
  PumpLooperUntil([] { return false; }, duration);
}

// Records the frames a vsync waiter delivers. Frame callbacks run on the UI
// thread, which is not the test thread when threads are not merged. Callbacks
// share ownership of the recorder, so a late frame never reaches a destroyed
// one.
class VsyncFrameRecorder
    : public std::enable_shared_from_this<VsyncFrameRecorder> {
 public:
  struct Frame {
    fml::TimePoint start;
    fml::TimePoint target;
    fml::TimePoint delivered;
    bool on_ui_thread = false;
  };

  explicit VsyncFrameRecorder(fml::RefPtr<fml::TaskRunner> ui_task_runner)
      : ui_task_runner_(std::move(ui_task_runner)) {}

  VsyncWaiter::Callback Callback() {
    return [self = shared_from_this()](
               std::unique_ptr<FrameTimingsRecorder> recorder) {
      Frame frame{
          .start = recorder->GetVsyncStartTime(),
          .target = recorder->GetVsyncTargetTime(),
          .delivered = fml::TimePoint::Now(),
          .on_ui_thread = self->ui_task_runner_->RunsTasksOnCurrentThread(),
      };
      std::scoped_lock lock(self->mutex_);
      self->frames_.push_back(frame);
    };
  }

  std::vector<Frame> frames() const {
    std::scoped_lock lock(mutex_);
    return frames_;
  }

 private:
  const fml::RefPtr<fml::TaskRunner> ui_task_runner_;
  mutable std::mutex mutex_;
  std::vector<Frame> frames_;
};

// Requests one frame from the vsync waiter the platform view creates for the
// shell, from the UI thread like the animator does, and checks how it is
// delivered.
void ExpectOneFrameOnUIThread(
    Settings::MergedPlatformUIThread merged_platform_ui_thread) {
  std::unique_ptr<AndroidShellHolder> holder =
      CreateHolderWithoutSurface(merged_platform_ui_thread);
  ASSERT_TRUE(holder->IsValid());
  fml::RefPtr<fml::TaskRunner> ui_task_runner =
      holder->GetShellForTesting()->GetTaskRunners().GetUITaskRunner();
  PlatformView& platform_view =
      *holder->GetShellForTesting()->GetPlatformView();
  std::shared_ptr<VsyncWaiter> vsync_waiter = platform_view.CreateVSyncWaiter();
  auto recorder = std::make_shared<VsyncFrameRecorder>(ui_task_runner);

  fml::TaskRunner::RunNowOrPostTask(ui_task_runner, [vsync_waiter, recorder] {
    vsync_waiter->AsyncWaitForVsync(recorder->Callback());
  });
  ASSERT_TRUE(PumpLooperUntil([&] { return !recorder->frames().empty(); },
                              kVsyncTimeout));
  PumpLooperFor(kSettleTime);

  std::vector<VsyncFrameRecorder::Frame> frames = recorder->frames();
  ASSERT_EQ(frames.size(), 1u);
  EXPECT_TRUE(frames[0].on_ui_thread);
  EXPECT_LE(frames[0].start, frames[0].delivered);
  EXPECT_EQ(frames[0].target - frames[0].start, kDefaultFrameInterval);
}
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
  holder->NotifySurfaceCreated(window);
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
  holder->NotifySurfaceCreated(window);

  // Exercise PlatformMessageHandlerAndroid directly through a mock C-API proc
  // table to verify that it takes the payload via
  // `PlatformMessageTakePayload`, forwards the channel + payload to JNI, and
  // routes the empty response back through `SendPlatformMessageResponse`.
  PlatformMessageHandlerAndroid handler(jni);
  FlutterEngineProcTable& api = holder->GetEmbedderApiForTesting();
  handler.Attach(
      reinterpret_cast<FLUTTER_API_SYMBOL(FlutterEngine)>(holder.get()), &api);

  const auto* fake_handle =
      reinterpret_cast<const FlutterPlatformMessageResponseHandle*>(0x1234);
  bool take_called = false;
  bool destructor_called = false;
  api.PlatformMessageTakePayload =
      MOCK_ENGINE_PROC(PlatformMessageTakePayload,
                       ([&](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                            const FlutterPlatformMessageResponseHandle* handle,
                            FlutterPlatformMessagePayload* payload_out) {
                         EXPECT_EQ(handle, fake_handle);
                         take_called = true;
                         static const uint8_t kBytes[4] = {10, 20, 30, 40};
                         payload_out->data = kBytes;
                         payload_out->data_size = sizeof(kBytes);
                         payload_out->user_data = &destructor_called;
                         payload_out->destruction_callback = [](void* ud) {
                           *static_cast<bool*>(ud) = true;
                         };
                         return kSuccess;
                       }));

  bool response_sent = false;
  api.SendPlatformMessageResponse =
      MOCK_ENGINE_PROC(SendPlatformMessageResponse,
                       ([&](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                            const FlutterPlatformMessageResponseHandle* handle,
                            const uint8_t* data, size_t data_length) {
                         EXPECT_EQ(handle, fake_handle);
                         EXPECT_EQ(data, nullptr);
                         EXPECT_EQ(data_length, 0u);
                         response_sent = true;
                         return kSuccess;
                       }));

  int captured_response_id = 0;
  EXPECT_CALL(
      *jni, FlutterViewHandlePlatformMessage("foo", ::testing::_, ::testing::_))
      .WillOnce([&](const std::string& channel,
                    UniquePlatformMessagePayload payload, int response_id) {
        ASSERT_NE(payload, nullptr);
        EXPECT_EQ(payload->data_size, 4u);
        captured_response_id = response_id;
      });

  FlutterPlatformMessage incoming = {};
  incoming.struct_size = sizeof(FlutterPlatformMessage);
  incoming.channel = "foo";
  incoming.response_handle = fake_handle;
  handler.HandlePlatformMessage(incoming);
  EXPECT_TRUE(take_called);
  EXPECT_TRUE(destructor_called);
  ASSERT_NE(captured_response_id, 0);

  handler.InvokePlatformMessageEmptyResponseCallback(captured_response_id);
  EXPECT_TRUE(response_sent);
}

TEST(AndroidShellHolder,
     PlatformMessageHandlerReleasesPendingHandlesOnShutdown) {
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  PlatformMessageHandlerAndroid handler(jni);

  FlutterEngineProcTable api = {};
  api.struct_size = sizeof(FlutterEngineProcTable);
  const auto fake_engine =
      reinterpret_cast<FLUTTER_API_SYMBOL(FlutterEngine)>(0xCAFE);
  const auto* fake_handle =
      reinterpret_cast<const FlutterPlatformMessageResponseHandle*>(0xBEEF);

  api.PlatformMessageTakePayload =
      MOCK_ENGINE_PROC(PlatformMessageTakePayload,
                       ([](FLUTTER_API_SYMBOL(FlutterEngine),
                           const FlutterPlatformMessageResponseHandle*,
                           FlutterPlatformMessagePayload* payload_out) {
                         payload_out->data = nullptr;
                         payload_out->data_size = 0;
                         return kSuccess;
                       }));
  int released_count = 0;
  api.PlatformMessageReleaseResponseHandle =
      MOCK_ENGINE_PROC(PlatformMessageReleaseResponseHandle,
                       ([&](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                            FlutterPlatformMessageResponseHandle* handle) {
                         EXPECT_EQ(engine, fake_engine);
                         EXPECT_EQ(handle, fake_handle);
                         released_count++;
                         return kSuccess;
                       }));

  handler.Attach(fake_engine, &api);
  EXPECT_CALL(*jni, FlutterViewHandlePlatformMessage("bar", ::testing::_,
                                                     ::testing::_));

  FlutterPlatformMessage incoming = {};
  incoming.struct_size = sizeof(FlutterPlatformMessage);
  incoming.channel = "bar";
  incoming.response_handle = fake_handle;
  handler.HandlePlatformMessage(incoming);

  // Destroy/release without invoking a response callback: the pending handle
  // must be released via `PlatformMessageReleaseResponseHandle`.
  handler.ReleasePendingResponses();
  EXPECT_EQ(released_count, 1);
}

TEST(AndroidShellHolder, CreateWithMergedPlatformAndUIThread) {
  Settings settings;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->NotifySurfaceCreated(window);

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
  holder->NotifySurfaceCreated(window);

  EXPECT_NE(
      holder->GetShellForTesting()->GetTaskRunners().GetUITaskRunner(),
      holder->GetShellForTesting()->GetTaskRunners().GetPlatformTaskRunner());
}

TEST(AndroidShellHolder, StartsWithDetachedLifecycleState) {
  Settings settings;
  settings.enable_software_rendering = false;
  // With merged threads the UI thread is the test thread, so the runtime
  // controller can be read directly.
  ASSERT_EQ(settings.merged_platform_ui_thread,
            Settings::MergedPlatformUIThread::kEnabled);
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_TRUE(holder->IsValid());

  // Frames stay off until Java reports a lifecycle state over
  // flutter/lifecycle.
  EXPECT_EQ(holder->GetShellForTesting()
                ->GetEngine()
                ->GetRuntimeController()
                ->GetPlatformData()
                .lifecycle_state,
            "AppLifecycleState.detached");
}

TEST(AndroidShellHolder, SurfaceLifecycleRoutesThroughEmbedderApi) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_TRUE(holder->IsValid());

  static int notify_created_calls;
  static int set_next_frame_callback_calls;
  static int schedule_frame_calls;
  static int notify_destroyed_calls;
  notify_created_calls = 0;
  set_next_frame_callback_calls = 0;
  schedule_frame_calls = 0;
  notify_destroyed_calls = 0;

  FlutterEngineProcTable& api = holder->GetEmbedderApiForTesting();
  auto real_notify_created = api.NotifyCreated;
  auto real_set_next_frame_callback = api.SetNextFrameCallback;
  auto real_schedule_frame = api.ScheduleFrame;
  auto real_notify_destroyed = api.NotifyDestroyed;
  static FlutterEngineNotifyCreatedFnPtr saved_notify_created;
  static FlutterEngineSetNextFrameCallbackFnPtr saved_set_next_frame_callback;
  static FlutterEngineScheduleFrameFnPtr saved_schedule_frame;
  static FlutterEngineNotifyDestroyedFnPtr saved_notify_destroyed;
  saved_notify_created = real_notify_created;
  saved_set_next_frame_callback = real_set_next_frame_callback;
  saved_schedule_frame = real_schedule_frame;
  saved_notify_destroyed = real_notify_destroyed;

  api.NotifyCreated = [](FLUTTER_API_SYMBOL(FlutterEngine) engine) {
    notify_created_calls++;
    return saved_notify_created(engine);
  };
  api.SetNextFrameCallback = [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                                VoidCallback callback, void* user_data) {
    set_next_frame_callback_calls++;
    return saved_set_next_frame_callback(engine, callback, user_data);
  };
  api.ScheduleFrame = [](FLUTTER_API_SYMBOL(FlutterEngine) engine) {
    schedule_frame_calls++;
    return saved_schedule_frame(engine);
  };
  api.NotifyDestroyed = [](FLUTTER_API_SYMBOL(FlutterEngine) engine) {
    notify_destroyed_calls++;
    return saved_notify_destroyed(engine);
  };

  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      nullptr, /*is_fake_window=*/true);
  holder->NotifySurfaceCreated(window);
  EXPECT_EQ(notify_created_calls, 1);
  EXPECT_EQ(set_next_frame_callback_calls, 1);

  holder->NotifySurfaceWindowChanged(window);
  EXPECT_EQ(schedule_frame_calls, 1);

  holder->NotifySurfaceDestroyed();
  EXPECT_EQ(notify_destroyed_calls, 1);
}

TEST(AndroidShellHolder, TextureLifecycleRoutesThroughEmbedderApi) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_TRUE(holder->IsValid());

  static int mark_frame_available_calls;
  static int unregister_texture_calls;
  static int64_t last_marked_texture_id;
  static int64_t last_unregistered_texture_id;
  mark_frame_available_calls = 0;
  unregister_texture_calls = 0;
  last_marked_texture_id = 0;
  last_unregistered_texture_id = 0;

  FlutterEngineProcTable& api = holder->GetEmbedderApiForTesting();
  static FlutterEngineMarkExternalTextureFrameAvailableFnPtr
      saved_mark_frame_available;
  static FlutterEngineUnregisterExternalTextureFnPtr saved_unregister_texture;
  saved_mark_frame_available = api.MarkExternalTextureFrameAvailable;
  saved_unregister_texture = api.UnregisterExternalTexture;

  api.MarkExternalTextureFrameAvailable =
      [](FLUTTER_API_SYMBOL(FlutterEngine) engine, int64_t texture_identifier) {
        mark_frame_available_calls++;
        last_marked_texture_id = texture_identifier;
        return saved_mark_frame_available(engine, texture_identifier);
      };
  api.UnregisterExternalTexture = [](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                                     int64_t texture_identifier) {
    unregister_texture_calls++;
    last_unregistered_texture_id = texture_identifier;
    return saved_unregister_texture(engine, texture_identifier);
  };

  constexpr int64_t kTestTextureId = 42;
  holder->MarkTextureFrameAvailable(kTestTextureId);
  EXPECT_EQ(mark_frame_available_calls, 1);
  EXPECT_EQ(last_marked_texture_id, kTestTextureId);

  holder->UnregisterTexture(kTestTextureId);
  EXPECT_EQ(unregister_texture_calls, 1);
  EXPECT_EQ(last_unregistered_texture_id, kTestTextureId);
}

TEST(AndroidShellHolder, VsyncFrameOnUIThreadWithMergedThreads) {
  ExpectOneFrameOnUIThread(Settings::MergedPlatformUIThread::kEnabled);
}

TEST(AndroidShellHolder, VsyncFrameOnUIThreadWithUnmergedThreads) {
  ExpectOneFrameOnUIThread(Settings::MergedPlatformUIThread::kDisabled);
}

TEST(AndroidShellHolder, PendingVsyncIsDeliveredOnceAfterHolderIsDestroyed) {
  // With merged threads the UI task runner is the message loop of the test
  // thread, so it outlives the holder.
  std::unique_ptr<AndroidShellHolder> holder =
      CreateHolderWithoutSurface(Settings::MergedPlatformUIThread::kEnabled);
  ASSERT_TRUE(holder->IsValid());
  fml::RefPtr<fml::TaskRunner> ui_task_runner =
      holder->GetShellForTesting()->GetTaskRunners().GetUITaskRunner();
  ASSERT_TRUE(ui_task_runner->RunsTasksOnCurrentThread());
  PlatformView& platform_view =
      *holder->GetShellForTesting()->GetPlatformView();
  // The test keeps the waiter alive. In an app the shell owns it and it dies
  // with the holder.
  std::shared_ptr<VsyncWaiter> vsync_waiter = platform_view.CreateVSyncWaiter();
  auto recorder = std::make_shared<VsyncFrameRecorder>(ui_task_runner);

  vsync_waiter->AsyncWaitForVsync(recorder->Callback());
  holder.reset();

  ASSERT_TRUE(PumpLooperUntil([&] { return !recorder->frames().empty(); },
                              kVsyncTimeout));
  PumpLooperFor(kSettleTime);

  std::vector<VsyncFrameRecorder::Frame> frames = recorder->frames();
  ASSERT_EQ(frames.size(), 1u);
  EXPECT_TRUE(frames[0].on_ui_thread);
  EXPECT_LE(frames[0].start, frames[0].delivered);
  EXPECT_EQ(frames[0].target - frames[0].start, kDefaultFrameInterval);
}

TEST(AndroidShellHolder, VsyncBatonsReturnThroughFlutterEngineOnVsync) {
  struct OnVsyncCall {
    intptr_t baton;
    uint64_t frame_start_time_nanos;
    uint64_t frame_target_time_nanos;
    bool on_ui_thread;
  };
  // Declared before the holder, which can still return batons while it is
  // destroyed.
  std::vector<OnVsyncCall> calls;
  std::unique_ptr<AndroidShellHolder> holder =
      CreateHolderWithoutSurface(Settings::MergedPlatformUIThread::kEnabled);
  ASSERT_TRUE(holder->IsValid());
  fml::RefPtr<fml::TaskRunner> ui_task_runner =
      holder->GetShellForTesting()->GetTaskRunners().GetUITaskRunner();
  // With merged threads every call below happens on the test thread.
  ASSERT_TRUE(ui_task_runner->RunsTasksOnCurrentThread());

  FlutterEngineProcTable& embedder_api = holder->GetEmbedderApiForTesting();
  // Forwarding to the real function keeps the frame coming.
  FlutterEngineOnVsyncFnPtr real_on_vsync = embedder_api.OnVsync;
  embedder_api.OnVsync = MOCK_ENGINE_PROC(
      OnVsync,
      ([&calls, ui_task_runner, real_on_vsync](
           FLUTTER_API_SYMBOL(FlutterEngine) engine, intptr_t baton,
           uint64_t frame_start_time_nanos, uint64_t frame_target_time_nanos) {
        calls.push_back({
            .baton = baton,
            .frame_start_time_nanos = frame_start_time_nanos,
            .frame_target_time_nanos = frame_target_time_nanos,
            .on_ui_thread = ui_task_runner->RunsTasksOnCurrentThread(),
        });
        return real_on_vsync(engine, baton, frame_start_time_nanos,
                             frame_target_time_nanos);
      }));

  PlatformView& platform_view =
      *holder->GetShellForTesting()->GetPlatformView();
  std::shared_ptr<VsyncWaiter> vsync_waiter = platform_view.CreateVSyncWaiter();
  auto recorder = std::make_shared<VsyncFrameRecorder>(ui_task_runner);
  vsync_waiter->AsyncWaitForVsync(recorder->Callback());
  ASSERT_TRUE(PumpLooperUntil([&] { return !recorder->frames().empty(); },
                              kVsyncTimeout));
  PumpLooperFor(kSettleTime);

  ASSERT_EQ(calls.size(), 1u);
  EXPECT_NE(calls[0].baton, 0);
  EXPECT_TRUE(calls[0].on_ui_thread);
  EXPECT_EQ(calls[0].frame_target_time_nanos - calls[0].frame_start_time_nanos,
            static_cast<uint64_t>(kDefaultFrameInterval.ToNanoseconds()));
  std::vector<VsyncFrameRecorder::Frame> frames = recorder->frames();
  ASSERT_EQ(frames.size(), 1u);
  EXPECT_EQ(frames[0].start.ToEpochDelta().ToNanoseconds(),
            static_cast<int64_t>(calls[0].frame_start_time_nanos));
}

TEST(AndroidShellHolder, SemanticsUpdateRoutesThroughCallbackToDelegate) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_TRUE(holder->IsValid());

  flutter::SemanticsNodeUpdates nodes;
  flutter::SemanticsNode node;
  node.id = 5;
  node.label = "submit_btn";
  node.traversalParent = 2;
  node.role = flutter::SemanticsRole::kSlider;
  nodes.insert({5, node});

  flutter::CustomAccessibilityActionUpdates actions;
  flutter::CustomAccessibilityAction action;
  action.id = 9;
  action.overrideId = 10;
  action.label = "custom_lbl";
  action.hint = "custom_hint";
  actions.insert({9, action});

  std::vector<uint8_t> captured_node_buf;
  std::vector<std::string> captured_node_strings;
  EXPECT_CALL(*jni, FlutterViewUpdateSemantics(::testing::_, ::testing::_,
                                               ::testing::_))
      .WillOnce([&](std::vector<uint8_t> buffer,
                    std::vector<std::string> strings,
                    std::vector<std::vector<uint8_t>>) {
        captured_node_buf = std::move(buffer);
        captured_node_strings = std::move(strings);
      });
  EXPECT_CALL(*jni, FlutterViewUpdateCustomAccessibilityActions(::testing::_,
                                                                ::testing::_))
      .Times(1);

  holder->GetShellForTesting()->GetPlatformView()->UpdateSemantics(
      0, std::move(nodes), std::move(actions));

  ASSERT_EQ(captured_node_buf.size(),
            PlatformViewAndroidDelegate::kBytesPerNode);
  const auto* words =
      reinterpret_cast<const int32_t*>(captured_node_buf.data());
  EXPECT_EQ(words[0], 5);   // node.id
  EXPECT_EQ(words[11], 2);  // node.traversalParent
  EXPECT_EQ(words[15],
            static_cast<int32_t>(flutter::SemanticsRole::kSlider));  // role
  ASSERT_EQ(captured_node_strings.size(), 1u);
  EXPECT_EQ(captured_node_strings[0], "submit_btn");
}

TEST(AndroidShellHolder,
     LoadDartDeferredLibraryAndErrorRouteThroughEmbedderApi) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_TRUE(holder->IsValid());

  bool load_called = false;
  intptr_t captured_unit_id = 0;
  std::vector<uint8_t> captured_data;
  std::vector<uint8_t> captured_instr;
  VoidCallback saved_destruction_callback = nullptr;
  void* saved_user_data = nullptr;

  auto& api = holder->GetEmbedderApiForTesting();
  api.LoadDartDeferredLibrary = MOCK_ENGINE_PROC(
      LoadDartDeferredLibrary, ([&](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                                    const FlutterDartDeferredLibrary* library) {
        EXPECT_NE(engine, nullptr);
        EXPECT_NE(library, nullptr);
        load_called = true;
        captured_unit_id = library->loading_unit_id;
        captured_data.assign(
            library->snapshot_data,
            library->snapshot_data + library->snapshot_data_size);
        captured_instr.assign(library->snapshot_instructions,
                              library->snapshot_instructions +
                                  library->snapshot_instructions_size);
        saved_destruction_callback = library->destruction_callback;
        saved_user_data = library->user_data;
        return kSuccess;
      }));

  bool data_mapping_destroyed = false;
  bool instr_mapping_destroyed = false;
  const uint8_t kDataBytes[3] = {1, 2, 3};
  const uint8_t kInstrBytes[2] = {4, 5};
  auto data_mapping = std::make_unique<fml::NonOwnedMapping>(
      kDataBytes, sizeof(kDataBytes),
      [&data_mapping_destroyed](const uint8_t*, size_t) {
        data_mapping_destroyed = true;
      });
  auto instr_mapping = std::make_unique<fml::NonOwnedMapping>(
      kInstrBytes, sizeof(kInstrBytes),
      [&instr_mapping_destroyed](const uint8_t*, size_t) {
        instr_mapping_destroyed = true;
      });

  holder->LoadDartDeferredLibrary(42, std::move(data_mapping),
                                  std::move(instr_mapping));
  EXPECT_TRUE(load_called);
  EXPECT_EQ(captured_unit_id, 42);
  EXPECT_THAT(captured_data, ::testing::ElementsAre(1, 2, 3));
  EXPECT_THAT(captured_instr, ::testing::ElementsAre(4, 5));
  EXPECT_FALSE(data_mapping_destroyed);
  EXPECT_FALSE(instr_mapping_destroyed);

  ASSERT_NE(saved_destruction_callback, nullptr);
  saved_destruction_callback(saved_user_data);
  EXPECT_TRUE(data_mapping_destroyed);
  EXPECT_TRUE(instr_mapping_destroyed);

  bool error_called = false;
  intptr_t error_unit_id = 0;
  std::string error_msg;
  bool error_transient = false;
  api.NotifyDartDeferredLibraryLoadError = MOCK_ENGINE_PROC(
      NotifyDartDeferredLibraryLoadError,
      ([&](FLUTTER_API_SYMBOL(FlutterEngine) engine,
           const FlutterDartDeferredLibraryLoadError* error) {
        EXPECT_NE(engine, nullptr);
        EXPECT_NE(error, nullptr);
        error_called = true;
        error_unit_id = error->loading_unit_id;
        error_msg = error->error_message != nullptr ? error->error_message : "";
        error_transient = error->transient;
        return kSuccess;
      }));

  holder->LoadDartDeferredLibraryError(99, "missing split apk", true);
  EXPECT_TRUE(error_called);
  EXPECT_EQ(error_unit_id, 99);
  EXPECT_EQ(error_msg, "missing split apk");
  EXPECT_TRUE(error_transient);
}

TEST(AndroidShellHolder, NotifyLowMemoryWarningCallsEmbedderApi) {
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<MockPlatformViewAndroidJNI>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_TRUE(holder->IsValid());

  int calls = 0;
  holder->GetEmbedderApiForTesting().NotifyLowMemoryWarning = MOCK_ENGINE_PROC(
      NotifyLowMemoryWarning, ([&](FLUTTER_API_SYMBOL(FlutterEngine) engine) {
        EXPECT_NE(engine, nullptr);
        calls++;
        return kSuccess;
      }));

  holder->NotifyLowMemoryWarning();
  EXPECT_EQ(calls, 1);
}

}  // namespace testing
}  // namespace flutter
