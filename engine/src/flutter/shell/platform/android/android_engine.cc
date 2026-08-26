// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include "flutter/shell/platform/android/android_engine.h"

#include <pthread.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "flutter/common/settings.h"
#include "flutter/fml/logging.h"
#include "flutter/fml/message_loop.h"
#include "flutter/shell/platform/android/android_display.h"
#include "flutter/shell/platform/android/android_thread_config.h"

namespace flutter {

static FlutterPointerPhase ToFlutterPointerPhase(PointerData::Change change) {
  switch (change) {
    case PointerData::Change::kCancel:
      return FlutterPointerPhase::kCancel;
    case PointerData::Change::kAdd:
      return FlutterPointerPhase::kAdd;
    case PointerData::Change::kRemove:
      return FlutterPointerPhase::kRemove;
    case PointerData::Change::kHover:
      return FlutterPointerPhase::kHover;
    case PointerData::Change::kDown:
      return FlutterPointerPhase::kDown;
    case PointerData::Change::kMove:
      return FlutterPointerPhase::kMove;
    case PointerData::Change::kUp:
      return FlutterPointerPhase::kUp;
    case PointerData::Change::kPanZoomStart:
      return FlutterPointerPhase::kPanZoomStart;
    case PointerData::Change::kPanZoomUpdate:
      return FlutterPointerPhase::kPanZoomUpdate;
    case PointerData::Change::kPanZoomEnd:
      return FlutterPointerPhase::kPanZoomEnd;
  }
  return FlutterPointerPhase::kCancel;
}

static FlutterPointerDeviceKind ToFlutterPointerDeviceKind(
    PointerData::DeviceKind kind) {
  switch (kind) {
    case PointerData::DeviceKind::kTouch:
      return FlutterPointerDeviceKind::kFlutterPointerDeviceKindTouch;
    case PointerData::DeviceKind::kMouse:
      return FlutterPointerDeviceKind::kFlutterPointerDeviceKindMouse;
    case PointerData::DeviceKind::kStylus:
      return FlutterPointerDeviceKind::kFlutterPointerDeviceKindStylus;
    case PointerData::DeviceKind::kInvertedStylus:
      return FlutterPointerDeviceKind::kFlutterPointerDeviceKindInvertedStylus;
    case PointerData::DeviceKind::kTrackpad:
      return FlutterPointerDeviceKind::kFlutterPointerDeviceKindTrackpad;
  }
  return FlutterPointerDeviceKind::kFlutterPointerDeviceKindTouch;
}

static FlutterPointerSignalKind ToFlutterPointerSignalKind(
    PointerData::SignalKind signal_kind) {
  switch (signal_kind) {
    case PointerData::SignalKind::kNone:
      return FlutterPointerSignalKind::kFlutterPointerSignalKindNone;
    case PointerData::SignalKind::kScroll:
      return FlutterPointerSignalKind::kFlutterPointerSignalKindScroll;
    case PointerData::SignalKind::kScrollInertiaCancel:
      return FlutterPointerSignalKind::
          kFlutterPointerSignalKindScrollInertiaCancel;
    case PointerData::SignalKind::kScale:
      return FlutterPointerSignalKind::kFlutterPointerSignalKindScale;
  }
  return FlutterPointerSignalKind::kFlutterPointerSignalKindNone;
}

//------------------------------------------------------------------------------
/// @brief Bridges incoming embedder platform message responses back to the
///        Embedder C API.
///
class AndroidEmbedderPlatformMessageResponse : public PlatformMessageResponse {
 public:
  AndroidEmbedderPlatformMessageResponse(
      const FlutterEngineProcTable& embedder_api,
      std::shared_ptr<AndroidEngine::TaskRunnerContext> context,
      const FlutterPlatformMessageResponseHandle* response_handle)
      : embedder_api_(embedder_api),
        context_(std::move(context)),
        response_handle_(response_handle) {}

  ~AndroidEmbedderPlatformMessageResponse() override = default;

  void Complete(std::unique_ptr<fml::Mapping> data) override {
    if (!context_ || !response_handle_) {
      return;
    }
    auto engine = context_->engine.load();
    if (engine && embedder_api_.SendPlatformMessageResponse) {
      const uint8_t* bytes = data ? data->GetMapping() : nullptr;
      size_t size = data ? data->GetSize() : 0;
      embedder_api_.SendPlatformMessageResponse(engine, response_handle_, bytes,
                                                size);
    }
  }

  void CompleteEmpty() override {
    if (!context_ || !response_handle_) {
      return;
    }
    auto engine = context_->engine.load();
    if (engine && embedder_api_.SendPlatformMessageResponse) {
      embedder_api_.SendPlatformMessageResponse(engine, response_handle_,
                                                nullptr, 0);
    }
  }

 private:
  FlutterEngineProcTable embedder_api_;
  std::shared_ptr<AndroidEngine::TaskRunnerContext> context_;
  const FlutterPlatformMessageResponseHandle* response_handle_ = nullptr;

  FML_DISALLOW_COPY_AND_ASSIGN(AndroidEmbedderPlatformMessageResponse);
};

AndroidEngine::AndroidEngine(const flutter::Settings& settings,
                             std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
                             AndroidRenderingAPI android_rendering_api)
    : settings_(settings),
      jni_facade_(std::move(jni_facade)),
      android_rendering_api_(android_rendering_api) {
  SetupEmbedderProcTable();
  InitializeTaskRunners();
  is_valid_ = InitializeEngine();
}

AndroidEngine::AndroidEngine(
    const Settings& settings,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    const std::shared_ptr<ThreadHost>& thread_host,
    const TaskRunners& task_runners,
    FLUTTER_API_SYMBOL(FlutterEngine) engine,
    std::unique_ptr<APKAssetProvider> apk_asset_provider,
    AndroidRenderingAPI rendering_api,
    std::shared_ptr<AndroidContext> android_context)
    : settings_(settings),
      jni_facade_(jni_facade),
      android_rendering_api_(rendering_api),
      engine_(engine),
      thread_host_(thread_host),
      task_runners_(task_runners),
      apk_asset_provider_(std::move(apk_asset_provider)) {
  SetupEmbedderProcTable();
  task_runner_context_ = std::make_shared<TaskRunnerContext>();
  task_runner_context_->platform_runner = task_runners.GetPlatformTaskRunner();
  task_runner_context_->ui_runner = task_runners.GetUITaskRunner();
  task_runner_context_->raster_runner = task_runners.GetRasterTaskRunner();
  task_runner_context_->io_runner = task_runners.GetIOTaskRunner();
  task_runner_context_->embedder_api = embedder_api_;
  task_runner_context_->engine.store(engine_);

  platform_view_ = std::make_unique<PlatformViewAndroid>(
      *this, *task_runners_, jni_facade_, std::move(android_context));
  compositor_ =
      std::make_shared<AndroidCompositor>(platform_view_->GetAndroidContext(),
                                          jni_facade_, nullptr, *task_runners_);
  is_valid_ = engine_ != nullptr;
}

AndroidEngine::~AndroidEngine() {
  if (task_runner_context_) {
    task_runner_context_->engine.store(nullptr);
  }
  if (engine_) {
    embedder_api_.Deinitialize(engine_);
    engine_ = nullptr;
  }
}

void AndroidEngine::SetupEmbedderProcTable() {
  embedder_api_.struct_size = sizeof(FlutterEngineProcTable);
  FlutterEngineGetProcAddresses(&embedder_api_);
}

void AndroidEngine::InitializeTaskRunners() {
  static std::atomic<size_t> thread_host_count{1};
  auto thread_label = std::to_string(thread_host_count.fetch_add(1));

  auto mask = ThreadHost::Type::kRaster | ThreadHost::Type::kIo;
  if (settings_.merged_platform_ui_thread !=
      Settings::MergedPlatformUIThread::kEnabled) {
    mask |= ThreadHost::Type::kUi;
  }

  flutter::ThreadHost::ThreadHostConfig host_config(
      thread_label, mask, AndroidPlatformThreadConfigSetter);
  host_config.ui_config = fml::Thread::ThreadConfig(
      flutter::ThreadHost::ThreadHostConfig::MakeThreadName(
          flutter::ThreadHost::Type::kUi, thread_label),
      fml::Thread::ThreadPriority::kDisplay);
  host_config.raster_config = fml::Thread::ThreadConfig(
      flutter::ThreadHost::ThreadHostConfig::MakeThreadName(
          flutter::ThreadHost::Type::kRaster, thread_label),
      fml::Thread::ThreadPriority::kRaster);
  host_config.io_config = fml::Thread::ThreadConfig(
      flutter::ThreadHost::ThreadHostConfig::MakeThreadName(
          flutter::ThreadHost::Type::kIo, thread_label),
      fml::Thread::ThreadPriority::kNormal);

  thread_host_ = std::make_shared<ThreadHost>(host_config);

  fml::MessageLoop::EnsureInitializedForCurrentThread();
  fml::RefPtr<fml::TaskRunner> platform_runner =
      fml::MessageLoop::GetCurrent().GetTaskRunner();
  fml::RefPtr<fml::TaskRunner> raster_runner =
      thread_host_->raster_thread->GetTaskRunner();
  fml::RefPtr<fml::TaskRunner> ui_runner =
      (settings_.merged_platform_ui_thread ==
       Settings::MergedPlatformUIThread::kEnabled)
          ? platform_runner
          : thread_host_->ui_thread->GetTaskRunner();
  fml::RefPtr<fml::TaskRunner> io_runner =
      thread_host_->io_thread->GetTaskRunner();

  task_runners_.emplace(thread_label, platform_runner, raster_runner, ui_runner,
                        io_runner);

  task_runner_context_ = std::make_shared<TaskRunnerContext>();
  task_runner_context_->platform_runner = platform_runner;
  task_runner_context_->ui_runner = ui_runner;
  task_runner_context_->raster_runner = raster_runner;
  task_runner_context_->io_runner = io_runner;
  task_runner_context_->embedder_api = embedder_api_;
  task_runner_context_->engine.store(nullptr);

  platform_handler_ = {task_runner_context_, platform_runner};
  ui_handler_ = {task_runner_context_, ui_runner};
  raster_handler_ = {task_runner_context_, raster_runner};
  io_handler_ = {task_runner_context_, io_runner};

  auto make_description = [](TaskRunnerHandler* handler, size_t identifier) {
    FlutterTaskRunnerDescription desc = {};
    desc.struct_size = sizeof(FlutterTaskRunnerDescription);
    desc.user_data = handler;
    desc.runs_task_on_current_thread_callback = [](void* user_data) -> bool {
      auto* h = static_cast<TaskRunnerHandler*>(user_data);
      return h && h->runner && h->runner->RunsTasksOnCurrentThread();
    };
    desc.post_task_callback = [](FlutterTask task, uint64_t target_time_nanos,
                                 void* user_data) -> void {
      auto* h = static_cast<TaskRunnerHandler*>(user_data);
      if (!h || !h->runner) {
        return;
      }
      auto target_time = fml::TimePoint::FromEpochDelta(
          fml::TimeDelta::FromNanoseconds(target_time_nanos));
      std::shared_ptr<TaskRunnerContext> context = h->context;
      h->runner->PostTaskForTime(
          [context, task]() {
            if (context) {
              auto engine = context->engine.load();
              if (engine && context->embedder_api.RunTask) {
                context->embedder_api.RunTask(engine, &task);
              }
            }
          },
          target_time);
    };
    desc.identifier = identifier;
    return desc;
  };

  static std::atomic<size_t> next_identifier{1};
  size_t platform_id = next_identifier.fetch_add(1);
  size_t ui_id = (settings_.merged_platform_ui_thread ==
                  Settings::MergedPlatformUIThread::kEnabled)
                     ? platform_id
                     : next_identifier.fetch_add(1);
  size_t raster_id = next_identifier.fetch_add(1);
  size_t io_id = next_identifier.fetch_add(1);

  platform_task_runner_description_ =
      make_description(&platform_handler_, platform_id);
  ui_task_runner_description_ = make_description(&ui_handler_, ui_id);
  raster_task_runner_description_ =
      make_description(&raster_handler_, raster_id);
  io_task_runner_description_ = make_description(&io_handler_, io_id);

  custom_task_runners_.struct_size = sizeof(FlutterCustomTaskRunners);
  custom_task_runners_.platform_task_runner =
      &platform_task_runner_description_;
  custom_task_runners_.ui_task_runner = &ui_task_runner_description_;
  custom_task_runners_.render_task_runner = &raster_task_runner_description_;
  custom_task_runners_.thread_priority_setter = AndroidThreadPrioritySetter;
}

bool AndroidEngine::InitializeEngine() {
  platform_view_ = std::make_unique<PlatformViewAndroid>(
      *this, *task_runners_, jni_facade_, android_rendering_api_);
  compositor_ =
      std::make_shared<AndroidCompositor>(platform_view_->GetAndroidContext(),
                                          jni_facade_, nullptr, *task_runners_);

  FlutterRendererConfig renderer_config = {};
  if (android_rendering_api_ == AndroidRenderingAPI::kSoftware) {
    renderer_config.type = kSoftware;
    renderer_config.software.struct_size =
        sizeof(FlutterSoftwareRendererConfig);
    renderer_config.software.surface_present_callback =
        [](void*, const void*, size_t, size_t) -> bool { return true; };
  } else if (android_rendering_api_ == AndroidRenderingAPI::kImpellerVulkan) {
    renderer_config.type = kVulkan;
    renderer_config.vulkan.struct_size = sizeof(FlutterVulkanRendererConfig);
    renderer_config.vulkan.version = 0;
    renderer_config.vulkan.instance = nullptr;
    renderer_config.vulkan.physical_device = nullptr;
    renderer_config.vulkan.device = nullptr;
    renderer_config.vulkan.queue_family_index = 0;
    renderer_config.vulkan.queue = nullptr;
    renderer_config.vulkan.get_instance_proc_address_callback = nullptr;
    renderer_config.vulkan.get_next_image_callback = nullptr;
    renderer_config.vulkan.present_image_callback = nullptr;
    renderer_config.vulkan.setup_callback = [](void* user_data) -> bool {
      auto* thiz = static_cast<AndroidEngine*>(user_data);
      if (thiz && thiz->platform_view_) {
        thiz->platform_view_->SetupImpellerContext();
        return true;
      }
      return false;
    };
  } else {
    renderer_config.type = kOpenGL;
    renderer_config.open_gl.struct_size = sizeof(FlutterOpenGLRendererConfig);
    renderer_config.open_gl.make_current = [](void*) -> bool { return false; };
    renderer_config.open_gl.clear_current = [](void*) -> bool { return false; };
    renderer_config.open_gl.present = [](void*) -> bool { return false; };
    renderer_config.open_gl.fbo_callback = [](void*) -> uint32_t { return 0; };
    renderer_config.open_gl.setup_callback = [](void* user_data) -> bool {
      auto* thiz = static_cast<AndroidEngine*>(user_data);
      if (thiz && thiz->platform_view_) {
        thiz->platform_view_->SetupImpellerContext();
        return true;
      }
      return false;
    };
  }

  FlutterProjectArgs project_args = {};
  project_args.struct_size = sizeof(FlutterProjectArgs);
  project_args.assets_path = settings_.assets_path.c_str();
  project_args.icu_data_path = settings_.icu_data_path.c_str();
  project_args.custom_task_runners = &custom_task_runners_;

  FlutterCompositor compositor = compositor_->GetFlutterCompositor();
  project_args.compositor = &compositor;

  project_args.platform_message_callback =
      [](const FlutterPlatformMessage* engine_message, void* user_data) {
        auto* thiz = static_cast<AndroidEngine*>(user_data);
        if (!thiz || !thiz->platform_view_ || !engine_message) {
          return;
        }
        fml::MallocMapping mapping =
            (engine_message->message && engine_message->message_size > 0)
                ? fml::MallocMapping::Copy(engine_message->message,
                                           engine_message->message_size)
                : fml::MallocMapping();

        fml::RefPtr<flutter::PlatformMessageResponse> response;
        if (engine_message->response_handle) {
          response =
              fml::MakeRefCounted<AndroidEmbedderPlatformMessageResponse>(
                  thiz->embedder_api_, thiz->task_runner_context_,
                  engine_message->response_handle);
        }

        auto message = std::make_unique<flutter::PlatformMessage>(
            engine_message->channel ? engine_message->channel : "",
            std::move(mapping), std::move(response));
        thiz->platform_view_->HandlePlatformMessage(std::move(message));
      };

  project_args.request_dart_deferred_library_callback =
      [](intptr_t loading_unit_id, void* user_data) {
        auto* thiz = static_cast<AndroidEngine*>(user_data);
        if (thiz && thiz->platform_view_) {
          thiz->platform_view_->RequestDartDeferredLibrary(loading_unit_id);
        }
      };

  FlutterEngineResult result = embedder_api_.Initialize(
      FLUTTER_ENGINE_VERSION, &renderer_config, &project_args, this, &engine_);

  if (result == kSuccess && engine_ != nullptr) {
    if (task_runner_context_) {
      task_runner_context_->engine.store(engine_);
    }
    return true;
  }
  return false;
}

bool AndroidEngine::IsValid() const {
  return is_valid_ && engine_ != nullptr;
}

const flutter::Settings& AndroidEngine::GetSettings() const {
  return settings_;
}

fml::WeakPtr<PlatformViewAndroid> AndroidEngine::GetPlatformView() {
  if (platform_view_) {
    return platform_view_->GetWeakPtr();
  }
  return {};
}

std::shared_ptr<AndroidCompositor> AndroidEngine::GetAndroidCompositor() const {
  return compositor_;
}

bool AndroidEngine::IsSurfaceControlEnabled() {
  if (platform_view_) {
    return platform_view_->IsSurfaceControlEnabled();
  }
  return false;
}

void AndroidEngine::NotifyLowMemoryWarning() {
  if (!IsValid()) {
    return;
  }
  embedder_api_.NotifyLowMemoryWarning(engine_);
}

void AndroidEngine::UpdateDisplayMetrics() {
  if (!IsValid()) {
    return;
  }
  AndroidDisplay display(jni_facade_);
  FlutterEngineDisplay embedder_display = {};
  embedder_display.struct_size = sizeof(FlutterEngineDisplay);
  embedder_display.display_id = display.GetDisplayId();
  embedder_display.single_display = true;
  embedder_display.refresh_rate = display.GetRefreshRate();
  embedder_display.width = display.GetWidth();
  embedder_display.height = display.GetHeight();
  embedder_display.device_pixel_ratio = display.GetDevicePixelRatio();

  embedder_api_.NotifyDisplayUpdate(
      engine_, kFlutterEngineDisplaysUpdateTypeStartup, &embedder_display, 1);
}

void AndroidEngine::Launch(std::unique_ptr<APKAssetProvider> apk_asset_provider,
                           const std::string& entrypoint,
                           const std::string& libraryUrl,
                           const std::vector<std::string>& entrypoint_args,
                           int64_t engine_id) {
  if (!IsValid()) {
    return;
  }
  apk_asset_provider_ = std::move(apk_asset_provider);
  UpdateDisplayMetrics();
  embedder_api_.RunInitialized(engine_);
}

std::unique_ptr<AndroidEngine> AndroidEngine::Spawn(
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    const std::string& entrypoint,
    const std::string& libraryUrl,
    const std::string& initial_route,
    const std::vector<std::string>& entrypoint_args,
    int64_t engine_id) const {
  if (!IsValid() || !task_runners_.has_value()) {
    return nullptr;
  }

  std::vector<const char*> c_entrypoint_args;
  c_entrypoint_args.reserve(entrypoint_args.size());
  for (const auto& arg : entrypoint_args) {
    c_entrypoint_args.push_back(arg.c_str());
  }

  FlutterEngineSpawnInfo spawn_info = {};
  spawn_info.struct_size = sizeof(FlutterEngineSpawnInfo);
  spawn_info.entrypoint = entrypoint.empty() ? nullptr : entrypoint.c_str();
  spawn_info.library_uri = libraryUrl.empty() ? nullptr : libraryUrl.c_str();
  spawn_info.initial_route =
      initial_route.empty() ? nullptr : initial_route.c_str();
  spawn_info.entrypoint_argc = static_cast<int>(c_entrypoint_args.size());
  spawn_info.entrypoint_argv =
      c_entrypoint_args.empty() ? nullptr : c_entrypoint_args.data();
  spawn_info.engine_id = engine_id;

  FLUTTER_API_SYMBOL(FlutterEngine) spawned_flutter_engine = nullptr;
  if (embedder_api_.Spawn(engine_, &spawn_info, &spawned_flutter_engine) !=
          kSuccess ||
      !spawned_flutter_engine) {
    return nullptr;
  }

  return std::unique_ptr<AndroidEngine>(new AndroidEngine(
      settings_, jni_facade, thread_host_, *task_runners_,
      spawned_flutter_engine,
      apk_asset_provider_ ? apk_asset_provider_->Clone() : nullptr,
      android_rendering_api_, platform_view_->GetAndroidContext()));
}

// |PlatformViewAndroid::Delegate|
const Settings& AndroidEngine::OnPlatformViewGetSettings() const {
  return settings_;
}

std::shared_ptr<fml::BasicTaskRunner>
AndroidEngine::OnPlatformViewGetShutdownSafeIOTaskRunner() const {
  return nullptr;
}

void AndroidEngine::OnPlatformViewCreated(std::unique_ptr<Surface> surface) {
  if (compositor_) {
    // Surface creation is managed via AndroidCompositor.
  }
}

void AndroidEngine::OnPlatformViewDestroyed() {
  if (compositor_) {
    compositor_->DestroySurfaces();
  }
}

void AndroidEngine::OnPlatformViewScheduleFrame() {
  if (!IsValid()) {
    return;
  }
  embedder_api_.ScheduleFrame(engine_);
}

void AndroidEngine::OnPlatformViewSetNextFrameCallback(
    const fml::closure& closure) {
  if (!IsValid() || !closure) {
    return;
  }
  auto* closure_ptr = new fml::closure(closure);
  embedder_api_.SetNextFrameCallback(
      engine_,
      [](void* user_data) {
        auto* cb = static_cast<fml::closure*>(user_data);
        if (cb) {
          (*cb)();
          delete cb;
        }
      },
      closure_ptr);
}

void AndroidEngine::OnPlatformViewSetViewportMetrics(
    int64_t view_id,
    const ViewportMetrics& metrics) {
  if (!IsValid()) {
    return;
  }
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(FlutterWindowMetricsEvent);
  event.width = static_cast<size_t>(metrics.physical_width);
  event.height = static_cast<size_t>(metrics.physical_height);
  event.pixel_ratio = metrics.device_pixel_ratio;
  event.left = 0;
  event.top = 0;
  event.physical_view_inset_top = metrics.physical_view_inset_top;
  event.physical_view_inset_right = metrics.physical_view_inset_right;
  event.physical_view_inset_bottom = metrics.physical_view_inset_bottom;
  event.physical_view_inset_left = metrics.physical_view_inset_left;
  event.display_id = static_cast<FlutterEngineDisplayId>(metrics.display_id);
  event.view_id = view_id;
  if (metrics.physical_max_width_constraint > 0 ||
      metrics.physical_max_height_constraint > 0) {
    event.has_constraints = true;
    event.min_width_constraint =
        static_cast<size_t>(metrics.physical_min_width_constraint);
    event.max_width_constraint =
        static_cast<size_t>(metrics.physical_max_width_constraint);
    event.min_height_constraint =
        static_cast<size_t>(metrics.physical_min_height_constraint);
    event.max_height_constraint =
        static_cast<size_t>(metrics.physical_max_height_constraint);
  }
  embedder_api_.SendWindowMetricsEvent(engine_, &event);
}

void AndroidEngine::OnPlatformViewDispatchPlatformMessage(
    std::unique_ptr<flutter::PlatformMessage> message) {
  if (!IsValid() || !message) {
    return;
  }

  struct ResponseHolder {
    fml::RefPtr<flutter::PlatformMessageResponse> response;
  };

  FlutterPlatformMessageResponseHandle* response_handle = nullptr;
  if (message->response()) {
    auto* holder = new ResponseHolder{message->response()};
    FlutterEngineResult res = embedder_api_.PlatformMessageCreateResponseHandle(
        engine_,
        [](const uint8_t* data, size_t size, void* user_data) {
          auto* h = static_cast<ResponseHolder*>(user_data);
          if (h && h->response) {
            if (data && size > 0) {
              h->response->Complete(std::make_unique<fml::MallocMapping>(
                  fml::MallocMapping::Copy(data, size)));
            } else {
              h->response->CompleteEmpty();
            }
          }
          delete h;
        },
        holder, &response_handle);
    if (res != kSuccess) {
      delete holder;
      response_handle = nullptr;
    }
  }

  FlutterPlatformMessage embedder_message = {};
  embedder_message.struct_size = sizeof(FlutterPlatformMessage);
  embedder_message.channel = message->channel().c_str();
  embedder_message.message = message->data().GetMapping();
  embedder_message.message_size = message->data().GetSize();
  embedder_message.response_handle = response_handle;

  embedder_api_.SendPlatformMessage(engine_, &embedder_message);

  if (response_handle) {
    embedder_api_.PlatformMessageReleaseResponseHandle(engine_,
                                                       response_handle);
  }
}

void AndroidEngine::OnPlatformViewDispatchPointerDataPacket(
    std::unique_ptr<flutter::PointerDataPacket> packet) {
  if (!IsValid() || !packet || packet->GetLength() == 0) {
    return;
  }

  const size_t count = packet->GetLength();
  std::vector<FlutterPointerEvent> events;
  events.reserve(count);

  for (size_t i = 0; i < count; ++i) {
    PointerData pointer_data = packet->GetPointerData(i);
    FlutterPointerEvent event = {};
    event.struct_size = sizeof(FlutterPointerEvent);
    event.phase = ToFlutterPointerPhase(pointer_data.change);
    event.timestamp = pointer_data.time_stamp;
    event.x = pointer_data.physical_x;
    event.y = pointer_data.physical_y;
    event.device = pointer_data.device;
    event.signal_kind = ToFlutterPointerSignalKind(pointer_data.signal_kind);
    event.scroll_delta_x = pointer_data.scroll_delta_x;
    event.scroll_delta_y = pointer_data.scroll_delta_y;
    event.device_kind = ToFlutterPointerDeviceKind(pointer_data.kind);
    event.buttons = pointer_data.buttons;
    event.pan_x = pointer_data.pan_x;
    event.pan_y = pointer_data.pan_y;
    event.scale = pointer_data.scale;
    event.rotation = pointer_data.rotation;
    event.view_id = pointer_data.view_id;
    event.pressure = pointer_data.pressure;
    event.pressure_min = pointer_data.pressure_min;
    event.pressure_max = pointer_data.pressure_max;
    events.push_back(event);
  }

  embedder_api_.SendPointerEvent(engine_, events.data(), events.size());
}

void AndroidEngine::OnPlatformViewDispatchSemanticsAction(
    int64_t view_id,
    int32_t node_id,
    flutter::SemanticsAction action,
    fml::MallocMapping args) {
  if (!IsValid()) {
    return;
  }
  FlutterSendSemanticsActionInfo action_info = {};
  action_info.struct_size = sizeof(FlutterSendSemanticsActionInfo);
  action_info.view_id = view_id;
  action_info.node_id = static_cast<uint64_t>(node_id);
  action_info.action = static_cast<FlutterSemanticsAction>(action);
  action_info.data = args.GetMapping();
  action_info.data_length = args.GetSize();
  embedder_api_.SendSemanticsAction(engine_, &action_info);
}

void AndroidEngine::OnPlatformViewSetSemanticsEnabled(bool enabled) {
  if (!IsValid()) {
    return;
  }
  embedder_api_.UpdateSemanticsEnabled(engine_, enabled);
}

void AndroidEngine::OnPlatformViewSetAccessibilityFeatures(int32_t flags) {
  if (!IsValid()) {
    return;
  }
  embedder_api_.UpdateAccessibilityFeatures(
      engine_, static_cast<FlutterAccessibilityFeature>(flags));
}

void AndroidEngine::OnPlatformViewRegisterTexture(
    std::shared_ptr<flutter::Texture> texture) {
  if (!IsValid() || !texture) {
    return;
  }
  int64_t texture_id = texture->Id();
  external_textures_[texture_id] = texture;
  embedder_api_.RegisterExternalTexture(engine_, texture_id);
}

void AndroidEngine::OnPlatformViewUnregisterTexture(int64_t texture_id) {
  if (!IsValid()) {
    return;
  }
  external_textures_.erase(texture_id);
  embedder_api_.UnregisterExternalTexture(engine_, texture_id);
}

void AndroidEngine::OnPlatformViewMarkTextureFrameAvailable(
    int64_t texture_id) {
  if (!IsValid()) {
    return;
  }
  embedder_api_.MarkExternalTextureFrameAvailable(engine_, texture_id);
}

void AndroidEngine::LoadDartDeferredLibrary(
    intptr_t loading_unit_id,
    std::unique_ptr<const fml::Mapping> snapshot_data,
    std::unique_ptr<const fml::Mapping> snapshot_instructions) {
  if (!IsValid()) {
    return;
  }
  FlutterLoadDeferredLibraryInfo load_info = {};
  load_info.struct_size = sizeof(FlutterLoadDeferredLibraryInfo);
  load_info.loading_unit_id = loading_unit_id;
  if (snapshot_data) {
    load_info.isolate_snapshot_data = snapshot_data->GetMapping();
    load_info.isolate_snapshot_data_size = snapshot_data->GetSize();
  }
  if (snapshot_instructions) {
    load_info.isolate_snapshot_instructions =
        snapshot_instructions->GetMapping();
    load_info.isolate_snapshot_instructions_size =
        snapshot_instructions->GetSize();
  }
  embedder_api_.LoadDartDeferredLibrary(engine_, &load_info);
}

void AndroidEngine::LoadDartDeferredLibraryError(
    intptr_t loading_unit_id,
    const std::string error_message,
    bool transient) {
  if (!IsValid()) {
    return;
  }
  FlutterLoadDeferredLibraryErrorInfo error_info = {};
  error_info.struct_size = sizeof(FlutterLoadDeferredLibraryErrorInfo);
  error_info.loading_unit_id = loading_unit_id;
  error_info.error_message = error_message.c_str();
  error_info.transient = transient;
  embedder_api_.LoadDartDeferredLibraryError(engine_, &error_info);
}

void AndroidEngine::UpdateAssetResolverByType(
    std::unique_ptr<AssetResolver> updated_asset_resolver,
    AssetResolver::AssetResolverType type) {
  if (!IsValid() || !apk_asset_provider_) {
    return;
  }
  FlutterAssetResolverConfig config =
      apk_asset_provider_->GetAssetResolverConfig();
  embedder_api_.UpdateAssetResolver(engine_, &config);
}

}  // namespace flutter
