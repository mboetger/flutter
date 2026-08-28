// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_engine.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include "flutter/fml/logging.h"
#include "flutter/fml/trace_event.h"
#include "flutter/shell/platform/android/android_thread_config.h"

namespace flutter {

namespace {

// Accessibility Bridge wire protocol constants.
// Must stay in 100% sync with AccessibilityBridge.java and PlatformViewAndroidDelegate.
//
// Field count breakdown per SemanticsNode (73 fixed int32/float32 slots = 292 bytes):
// - 1  slot : id
// - 2  slots: flags (64-bit int encoded as two 32-bit words)
// - 1  slot : actions
// - 1  slot : maxValueLength
// - 1  slot : currentValueLength
// - 1  slot : textSelectionBase
// - 1  slot : textSelectionExtent
// - 1  slot : platformViewId
// - 1  slot : scrollChildren
// - 1  slot : scrollIndex
// - 1  slot : traversalParent
// - 3  slots: scrollPosition, scrollExtentMax, scrollExtentMin (3 floats)
// - 1  slot : role
// - 10 slots: string indices & attribute headers for identifier, label, value,
//             increasedValue, decreasedValue, hint, tooltip, linkUrl, locale,
//             minValue, maxValue (10 strings/headers)
// - 2  slots: headingLevel, textDirection
// - 4  slots: rect (left, top, right, bottom floats)
// - 16 slots: transform (4x4 column-major matrix floats)
// - 16 slots: hitTestTransform (4x4 column-major matrix floats)
// - 3  slots: traversalOrderChildCount, hitTestOrderChildCount, customActionCount
// Total = 73 32-bit words.
constexpr size_t kBytesPerNode = 73 * sizeof(int32_t);

// 4 bytes per child ID in traversal order or hit test order.
constexpr size_t kBytesPerChild = sizeof(int32_t);

// 4 bytes per custom accessibility action ID.
constexpr size_t kBytesPerCustomAction = sizeof(int32_t);

// 16 bytes per string attribute: start (int32), end (int32), type (int32), string_args index (int32).
constexpr size_t kBytesPerStringAttribute = 4 * sizeof(int32_t);

// 16 bytes per custom action entry: id (int32), overrideId (int32), label index (int32), hint index (int32).
constexpr size_t kBytesPerAction = 4 * sizeof(int32_t);

// Sent to Java as -1 when a string or string attribute table is empty.
constexpr int32_t kEmptyStringIndex = -1;

// Standard Vulkan 1.1 API version encoding: (1 << 22) | (1 << 12).
constexpr uint32_t kVulkanApiVersion11 = (1 << 22) | (1 << 12);

// Sentinel non-zero pointer value for Vulkan handles when custom compositor is used.
constexpr uintptr_t kVulkanSentinelHandle = 1;

void PutStringIntoBuffer(const char* str,
                         int32_t* buffer,
                         size_t* position,
                         std::vector<std::string>& strings) {
  if (str == nullptr || str[0] == '\0') {
    buffer[(*position)++] = kEmptyStringIndex;
  } else {
    buffer[(*position)++] = static_cast<int32_t>(strings.size());
    strings.emplace_back(str);
  }
}

void PutStringAttributesIntoBuffer(
    size_t attributes_count,
    const FlutterStringAttribute** attributes,
    int32_t* buffer,
    size_t* position,
    std::vector<std::vector<uint8_t>>& string_attribute_args) {
  if (attributes == nullptr || attributes_count == 0) {
    buffer[(*position)++] = kEmptyStringIndex;
    return;
  }
  buffer[(*position)++] = static_cast<int32_t>(attributes_count);
  for (size_t i = 0; i < attributes_count; ++i) {
    const FlutterStringAttribute* attr = attributes[i];
    if (attr == nullptr) {
      continue;
    }
    buffer[(*position)++] = static_cast<int32_t>(attr->start);
    buffer[(*position)++] = static_cast<int32_t>(attr->end);
    buffer[(*position)++] = static_cast<int32_t>(attr->type);
    switch (attr->type) {
      case kSpellOut:
        buffer[(*position)++] = kEmptyStringIndex;
        break;
      case kLocale: {
        const FlutterLocaleStringAttribute* locale_attr = attr->locale;
        if (locale_attr != nullptr && locale_attr->locale != nullptr) {
          buffer[(*position)++] =
              static_cast<int32_t>(string_attribute_args.size());
          const char* loc = locale_attr->locale;
          string_attribute_args.emplace_back(loc, loc + std::strlen(loc));
        } else {
          buffer[(*position)++] = kEmptyStringIndex;
        }
        break;
      }
    }
  }
}

void PutTransformationIntoBuffer(const FlutterTransformation& transform,
                                 float* buffer,
                                 size_t* position) {
  // 4x4 Column-major layout expected by AccessibilityBridge.java:
  // col 0: [scaleX, skewY,  0, pers0]
  // col 1: [skewX,  scaleY, 0, pers1]
  // col 2: [0,      0,      1, 0]
  // col 3: [transX, transY, 0, pers2]
  buffer[(*position)++] = static_cast<float>(transform.scaleX);
  buffer[(*position)++] = static_cast<float>(transform.skewY);
  buffer[(*position)++] = 0.0f;
  buffer[(*position)++] = static_cast<float>(transform.pers0);

  buffer[(*position)++] = static_cast<float>(transform.skewX);
  buffer[(*position)++] = static_cast<float>(transform.scaleY);
  buffer[(*position)++] = 0.0f;
  buffer[(*position)++] = static_cast<float>(transform.pers1);

  buffer[(*position)++] = 0.0f;
  buffer[(*position)++] = 0.0f;
  buffer[(*position)++] = 1.0f;
  buffer[(*position)++] = 0.0f;

  buffer[(*position)++] = static_cast<float>(transform.transX);
  buffer[(*position)++] = static_cast<float>(transform.transY);
  buffer[(*position)++] = 0.0f;
  buffer[(*position)++] = static_cast<float>(transform.pers2);
}

int64_t ConvertFlagsToInt64(const FlutterSemanticsNode2* node) {
  if (node == nullptr) {
    return 0;
  }
  if (node->flags2 != nullptr) {
    const FlutterSemanticsFlags& flags = *(node->flags2);
    int64_t result = 0;
    if (flags.is_checked != kFlutterCheckStateNone) {
      result |= (INT64_C(1) << 0);
    }
    if (flags.is_checked == kFlutterCheckStateTrue) {
      result |= (INT64_C(1) << 1);
    }
    if (flags.is_selected == kFlutterTristateTrue) {
      result |= (INT64_C(1) << 2);
    }
    if (flags.is_button) {
      result |= (INT64_C(1) << 3);
    }
    if (flags.is_text_field) {
      result |= (INT64_C(1) << 4);
    }
    if (flags.is_focused == kFlutterTristateTrue) {
      result |= (INT64_C(1) << 5);
    }
    if (flags.is_enabled != kFlutterTristateNone) {
      result |= (INT64_C(1) << 6);
    }
    if (flags.is_enabled == kFlutterTristateTrue) {
      result |= (INT64_C(1) << 7);
    }
    if (flags.is_in_mutually_exclusive_group) {
      result |= (INT64_C(1) << 8);
    }
    if (flags.is_header) {
      result |= (INT64_C(1) << 9);
    }
    if (flags.is_obscured) {
      result |= (INT64_C(1) << 10);
    }
    if (flags.scopes_route) {
      result |= (INT64_C(1) << 11);
    }
    if (flags.names_route) {
      result |= (INT64_C(1) << 12);
    }
    if (flags.is_hidden) {
      result |= (INT64_C(1) << 13);
    }
    if (flags.is_image) {
      result |= (INT64_C(1) << 14);
    }
    if (flags.is_live_region) {
      result |= (INT64_C(1) << 15);
    }
    if (flags.is_toggled != kFlutterTristateNone) {
      result |= (INT64_C(1) << 16);
    }
    if (flags.is_toggled == kFlutterTristateTrue) {
      result |= (INT64_C(1) << 17);
    }
    if (flags.has_implicit_scrolling) {
      result |= (INT64_C(1) << 18);
    }
    if (flags.is_multiline) {
      result |= (INT64_C(1) << 19);
    }
    if (flags.is_read_only) {
      result |= (INT64_C(1) << 20);
    }
    if (flags.is_focused != kFlutterTristateNone) {
      result |= (INT64_C(1) << 21);
    }
    if (flags.is_link) {
      result |= (INT64_C(1) << 22);
    }
    if (flags.is_slider) {
      result |= (INT64_C(1) << 23);
    }
    if (flags.is_keyboard_key) {
      result |= (INT64_C(1) << 24);
    }
    if (flags.is_checked == kFlutterCheckStateMixed) {
      result |= (INT64_C(1) << 25);
    }
    if (flags.is_expanded != kFlutterTristateNone) {
      result |= (INT64_C(1) << 26);
    }
    if (flags.is_expanded == kFlutterTristateTrue) {
      result |= (INT64_C(1) << 27);
    }
    if (flags.is_selected != kFlutterTristateNone) {
      result |= (INT64_C(1) << 28);
    }
    if (flags.is_required != kFlutterTristateNone) {
      result |= (INT64_C(1) << 29);
    }
    if (flags.is_required == kFlutterTristateTrue) {
      result |= (INT64_C(1) << 30);
    }
    if (flags.is_accessibility_focus_blocked) {
      result |= (INT64_C(1) << 31);
    }
    return result;
  }
  return static_cast<int64_t>(node->flags__deprecated__);
}

FlutterPointerPhase ConvertPointerChangeToPhase(PointerData::Change change) {
  switch (change) {
    case PointerData::Change::kCancel:
      return kCancel;
    case PointerData::Change::kAdd:
      return kAdd;
    case PointerData::Change::kRemove:
      return kRemove;
    case PointerData::Change::kHover:
      return kHover;
    case PointerData::Change::kDown:
      return kDown;
    case PointerData::Change::kMove:
      return kMove;
    case PointerData::Change::kUp:
      return kUp;
    case PointerData::Change::kPanZoomStart:
      return kPanZoomStart;
    case PointerData::Change::kPanZoomUpdate:
      return kPanZoomUpdate;
    case PointerData::Change::kPanZoomEnd:
      return kPanZoomEnd;
  }
  return kCancel;
}

FlutterPointerDeviceKind ConvertPointerDeviceKind(
    PointerData::DeviceKind kind) {
  switch (kind) {
    case PointerData::DeviceKind::kTouch:
      return kFlutterPointerDeviceKindTouch;
    case PointerData::DeviceKind::kMouse:
      return kFlutterPointerDeviceKindMouse;
    case PointerData::DeviceKind::kStylus:
      return kFlutterPointerDeviceKindStylus;
    case PointerData::DeviceKind::kInvertedStylus:
      return kFlutterPointerDeviceKindInvertedStylus;
    case PointerData::DeviceKind::kTrackpad:
      return kFlutterPointerDeviceKindTrackpad;
  }
  return kFlutterPointerDeviceKindTouch;
}

FlutterPointerSignalKind ConvertPointerSignalKind(
    PointerData::SignalKind signal_kind) {
  switch (signal_kind) {
    case PointerData::SignalKind::kNone:
      return kFlutterPointerSignalKindNone;
    case PointerData::SignalKind::kScroll:
      return kFlutterPointerSignalKindScroll;
    case PointerData::SignalKind::kScrollInertiaCancel:
      return kFlutterPointerSignalKindScrollInertiaCancel;
    case PointerData::SignalKind::kScale:
      return kFlutterPointerSignalKindScale;
  }
  return kFlutterPointerSignalKindNone;
}

FlutterRendererConfig CreateRendererConfig(AndroidRenderingAPI rendering_api) {
  FlutterRendererConfig renderer_config = {};
  switch (rendering_api) {
#if !SLIMPELLER
    case AndroidRenderingAPI::kSoftware:
      renderer_config.type = kSoftware;
      renderer_config.software.struct_size =
          sizeof(FlutterSoftwareRendererConfig);
      renderer_config.software.surface_present_callback =
          [](void* user_data, const void* allocation, size_t row_bytes,
             size_t height) -> bool { return true; };
      break;
    case AndroidRenderingAPI::kSkiaOpenGLES:
#endif  // !SLIMPELLER
    case AndroidRenderingAPI::kImpellerOpenGLES:
    case AndroidRenderingAPI::kImpellerAutoselect:
      renderer_config.type = kOpenGL;
      renderer_config.open_gl.struct_size = sizeof(FlutterOpenGLRendererConfig);
      renderer_config.open_gl.make_current = [](void* user_data) -> bool {
        return true;
      };
      renderer_config.open_gl.clear_current = [](void* user_data) -> bool {
        return true;
      };
      renderer_config.open_gl.present = [](void* user_data) -> bool {
        return true;
      };
      renderer_config.open_gl.fbo_callback = [](void* user_data) -> uint32_t {
        return 0;
      };
      break;
    case AndroidRenderingAPI::kImpellerVulkan:
      renderer_config.type = kVulkan;
      renderer_config.vulkan.struct_size = sizeof(FlutterVulkanRendererConfig);
      renderer_config.vulkan.version = kVulkanApiVersion11;
      // When a FlutterCompositor is provided, the compositor manages backing stores
      // directly. The embedder validation still requires non-null handles and procs.
      renderer_config.vulkan.instance =
          reinterpret_cast<FlutterVulkanInstanceHandle>(kVulkanSentinelHandle);
      renderer_config.vulkan.physical_device =
          reinterpret_cast<FlutterVulkanPhysicalDeviceHandle>(
              kVulkanSentinelHandle);
      renderer_config.vulkan.device =
          reinterpret_cast<FlutterVulkanDeviceHandle>(kVulkanSentinelHandle);
      renderer_config.vulkan.queue =
          reinterpret_cast<FlutterVulkanQueueHandle>(kVulkanSentinelHandle);
      renderer_config.vulkan.get_instance_proc_address_callback =
          [](void* user_data, FlutterVulkanInstanceHandle instance,
             const char* name) -> void* { return nullptr; };
      renderer_config.vulkan.get_next_image_callback =
          [](void* user_data,
             const FlutterFrameInfo* frame_info) -> FlutterVulkanImage {
        return {};
      };
      renderer_config.vulkan.present_image_callback =
          [](void* user_data, const FlutterVulkanImage* image) -> bool {
        return true;
      };
      break;
  }
  return renderer_config;
}

}  // namespace

AndroidEngine::AndroidEngine(const Settings& settings,
                             std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
                             AndroidRenderingAPI rendering_api,
                             fml::RefPtr<fml::TaskRunner> platform_task_runner,
                             fml::RefPtr<fml::TaskRunner> raster_task_runner,
                             fml::RefPtr<fml::TaskRunner> ui_task_runner,
                             fml::RefPtr<fml::TaskRunner> io_task_runner)
    : settings_(settings),
      jni_facade_(std::move(jni_facade)),
      rendering_api_(rendering_api),
      platform_task_runner_(std::move(platform_task_runner)),
      raster_task_runner_(std::move(raster_task_runner)),
      ui_task_runner_(std::move(ui_task_runner)),
      io_task_runner_(std::move(io_task_runner)) {
  embedder_api_.struct_size = sizeof(FlutterEngineProcTable);
  FlutterEngineResult result = FlutterEngineGetProcAddresses(&embedder_api_);
  FML_CHECK(result == kSuccess)
      << "Failed to populate FlutterEngineProcTable in AndroidEngine.";

  surface_manager_ = std::make_shared<AndroidSurfaceManager>(rendering_api_);
  compositor_ = std::make_unique<AndroidCompositor>(
      surface_manager_, jni_facade_, raster_task_runner_);

  // Setup custom task runner configuration if task runners are provided.
  if (platform_task_runner_ || raster_task_runner_ || ui_task_runner_) {
    custom_task_runners_.struct_size = sizeof(FlutterCustomTaskRunners);
    custom_task_runners_.thread_priority_setter = &AndroidThreadPrioritySetter;

    if (platform_task_runner_) {
      platform_runner_ctx_.runner = platform_task_runner_.get();
      platform_runner_ctx_.engine_holder = engine_handle_holder_;
      platform_runner_desc_.struct_size = sizeof(FlutterTaskRunnerDescription);
      platform_runner_desc_.user_data = &platform_runner_ctx_;
      platform_runner_desc_.runs_task_on_current_thread_callback =
          [](void* user_data) -> bool {
        return static_cast<TaskRunnerContext*>(user_data)
            ->runner->RunsTasksOnCurrentThread();
      };
      platform_runner_desc_.post_task_callback =
          [](FlutterTask task, uint64_t target_time_nanos,
             void* user_data) -> void {
        auto* ctx = static_cast<TaskRunnerContext*>(user_data);
        uint64_t now = FlutterEngineGetCurrentTime();
        fml::TimeDelta delay =
            (target_time_nanos > now)
                ? fml::TimeDelta::FromNanoseconds(target_time_nanos - now)
                : fml::TimeDelta::Zero();
        ctx->runner->PostDelayedTask(
            [engine_holder = ctx->engine_holder, task]() {
              FLUTTER_API_SYMBOL(FlutterEngine) engine =
                  engine_holder != nullptr ? engine_holder->load() : nullptr;
              if (engine != nullptr) {
                FlutterEngineRunTask(engine, &task);
              }
            },
            delay);
      };
      custom_task_runners_.platform_task_runner = &platform_runner_desc_;
    }

    if (raster_task_runner_) {
      render_runner_ctx_.runner = raster_task_runner_.get();
      render_runner_ctx_.engine_holder = engine_handle_holder_;
      render_runner_desc_.struct_size = sizeof(FlutterTaskRunnerDescription);
      render_runner_desc_.user_data = &render_runner_ctx_;
      render_runner_desc_.runs_task_on_current_thread_callback =
          [](void* user_data) -> bool {
        return static_cast<TaskRunnerContext*>(user_data)
            ->runner->RunsTasksOnCurrentThread();
      };
      render_runner_desc_.post_task_callback =
          [](FlutterTask task, uint64_t target_time_nanos,
             void* user_data) -> void {
        auto* ctx = static_cast<TaskRunnerContext*>(user_data);
        uint64_t now = FlutterEngineGetCurrentTime();
        fml::TimeDelta delay =
            (target_time_nanos > now)
                ? fml::TimeDelta::FromNanoseconds(target_time_nanos - now)
                : fml::TimeDelta::Zero();
        ctx->runner->PostDelayedTask(
            [engine_holder = ctx->engine_holder, task]() {
              FLUTTER_API_SYMBOL(FlutterEngine) engine =
                  engine_holder != nullptr ? engine_holder->load() : nullptr;
              if (engine != nullptr) {
                FlutterEngineRunTask(engine, &task);
              }
            },
            delay);
      };
      custom_task_runners_.render_task_runner = &render_runner_desc_;
    }

    if (ui_task_runner_) {
      ui_runner_ctx_.runner = ui_task_runner_.get();
      ui_runner_ctx_.engine_holder = engine_handle_holder_;
      ui_runner_desc_.struct_size = sizeof(FlutterTaskRunnerDescription);
      ui_runner_desc_.user_data = &ui_runner_ctx_;
      ui_runner_desc_.runs_task_on_current_thread_callback =
          [](void* user_data) -> bool {
        return static_cast<TaskRunnerContext*>(user_data)
            ->runner->RunsTasksOnCurrentThread();
      };
      ui_runner_desc_.post_task_callback =
          [](FlutterTask task, uint64_t target_time_nanos,
             void* user_data) -> void {
        auto* ctx = static_cast<TaskRunnerContext*>(user_data);
        uint64_t now = FlutterEngineGetCurrentTime();
        fml::TimeDelta delay =
            (target_time_nanos > now)
                ? fml::TimeDelta::FromNanoseconds(target_time_nanos - now)
                : fml::TimeDelta::Zero();
        ctx->runner->PostDelayedTask(
            [engine_holder = ctx->engine_holder, task]() {
              FLUTTER_API_SYMBOL(FlutterEngine) engine =
                  engine_holder != nullptr ? engine_holder->load() : nullptr;
              if (engine != nullptr) {
                FlutterEngineRunTask(engine, &task);
              }
            },
            delay);
      };
      custom_task_runners_.ui_task_runner = &ui_runner_desc_;
    }
  }
}

AndroidEngine::AndroidEngine(
    const Settings& settings,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    AndroidRenderingAPI rendering_api,
    FLUTTER_API_SYMBOL(FlutterEngine) engine_handle,
    std::shared_ptr<AndroidSurfaceManager> surface_manager,
    std::unique_ptr<AndroidCompositor> compositor,
    fml::RefPtr<fml::TaskRunner> platform_task_runner,
    fml::RefPtr<fml::TaskRunner> raster_task_runner,
    fml::RefPtr<fml::TaskRunner> ui_task_runner,
    fml::RefPtr<fml::TaskRunner> io_task_runner)
    : settings_(settings),
      jni_facade_(std::move(jni_facade)),
      rendering_api_(rendering_api),
      platform_task_runner_(std::move(platform_task_runner)),
      raster_task_runner_(std::move(raster_task_runner)),
      ui_task_runner_(std::move(ui_task_runner)),
      io_task_runner_(std::move(io_task_runner)),
      surface_manager_(std::move(surface_manager)),
      compositor_(std::move(compositor)),
      engine_(engine_handle),
      is_spawned_(true) {
  embedder_api_.struct_size = sizeof(FlutterEngineProcTable);
  FlutterEngineResult result = FlutterEngineGetProcAddresses(&embedder_api_);
  FML_CHECK(result == kSuccess)
      << "Failed to populate FlutterEngineProcTable in spawned AndroidEngine.";

  if (engine_ != nullptr) {
    engine_handle_holder_->store(engine_);
  }
}

AndroidEngine::~AndroidEngine() {
  if (engine_handle_holder_ != nullptr) {
    engine_handle_holder_->store(nullptr);
  }

  // Release any remaining pending response handles.
  {
    std::lock_guard<std::mutex> lock(pending_responses_mutex_);
    for (auto& pair : pending_responses_) {
      if (pair.second != nullptr &&
          embedder_api_.PlatformMessageReleaseResponseHandle != nullptr) {
        embedder_api_.PlatformMessageReleaseResponseHandle(
            engine_,
            const_cast<FlutterPlatformMessageResponseHandle*>(pair.second));
      }
    }
    pending_responses_.clear();
  }

  if (engine_ != nullptr) {
    if (embedder_api_.Deinitialize) {
      embedder_api_.Deinitialize(engine_);
    } else if (embedder_api_.Shutdown) {
      embedder_api_.Shutdown(engine_);
    }
    engine_ = nullptr;
  }
}

bool AndroidEngine::IsValid() const {
  return embedder_api_.struct_size == sizeof(FlutterEngineProcTable);
}

bool AndroidEngine::IsRunning() const {
  return engine_ != nullptr;
}

bool AndroidEngine::Launch(
    std::unique_ptr<APKAssetProvider> apk_asset_provider,
    const std::string& entrypoint,
    const std::string& library_url,
    const std::vector<std::string>& entrypoint_args,
    int64_t engine_id) {
  TRACE_EVENT0("flutter", "AndroidEngine::Launch");

  apk_asset_provider_ = std::move(apk_asset_provider);
  FlutterRendererConfig renderer_config = CreateRendererConfig(rendering_api_);

  FlutterProjectArgs project_args = {};
  project_args.struct_size = sizeof(FlutterProjectArgs);

  if (!entrypoint.empty()) {
    project_args.custom_dart_entrypoint = entrypoint.c_str();
  }

  std::vector<const char*> entrypoint_argv;
  if (!entrypoint_args.empty()) {
    entrypoint_argv.reserve(entrypoint_args.size());
    for (const auto& arg : entrypoint_args) {
      entrypoint_argv.push_back(arg.c_str());
    }
    project_args.dart_entrypoint_argc =
        static_cast<int>(entrypoint_argv.size());
    project_args.dart_entrypoint_argv = entrypoint_argv.data();
  }

  // Setup asset resolvers.
  FlutterAssetResolver resolver = {};
  const FlutterAssetResolver* resolvers[1] = {nullptr};
  if (apk_asset_provider_) {
    resolver = apk_asset_provider_->GetAssetResolver();
    resolvers[0] = &resolver;
    project_args.asset_resolvers = resolvers;
    project_args.asset_resolvers_count = 1;
  }

  // Setup custom task runners if present.
  if (custom_task_runners_.struct_size == sizeof(FlutterCustomTaskRunners)) {
    project_args.custom_task_runners = &custom_task_runners_;
  }

  // Setup compositor.
  FlutterCompositor flutter_compositor = compositor_->GetFlutterCompositor();
  project_args.compositor = &flutter_compositor;

  // Setup callbacks.
  project_args.platform_message_callback = &AndroidEngine::OnPlatformMessageThunk;
  project_args.vsync_callback = &AndroidEngine::OnVsyncThunk;
  project_args.update_semantics_callback2 =
      &AndroidEngine::OnUpdateSemanticsThunk2;
  project_args.request_dart_deferred_library_callback =
      &AndroidEngine::OnRequestDartDeferredLibraryThunk;
  project_args.on_pre_engine_restart_callback =
      &AndroidEngine::OnPreEngineRestartThunk;

  FlutterEngineResult result =
      embedder_api_.Initialize(FLUTTER_ENGINE_VERSION, &renderer_config,
                               &project_args, this, &engine_);
  if (result != kSuccess || engine_ == nullptr) {
    FML_LOG(ERROR) << "AndroidEngine::Launch: FlutterEngineInitialize failed: "
                   << result;
    return false;
  }

  engine_handle_holder_->store(engine_);

  result = embedder_api_.RunInitialized(engine_);
  if (result != kSuccess) {
    FML_LOG(ERROR) << "AndroidEngine::Launch: FlutterEngineRunInitialized failed: "
                   << result;
    return false;
  }

  return true;
}

std::unique_ptr<AndroidEngine> AndroidEngine::Spawn(
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    const std::string& entrypoint,
    const std::string& library_url,
    const std::string& initial_route,
    const std::vector<std::string>& entrypoint_args,
    int64_t engine_id) const {
  TRACE_EVENT0("flutter", "AndroidEngine::Spawn");

  if (!IsRunning()) {
    FML_LOG(ERROR) << "AndroidEngine::Spawn: Parent engine is not running.";
    return nullptr;
  }

  FlutterRendererConfig renderer_config = CreateRendererConfig(rendering_api_);

  std::vector<const char*> entrypoint_argv;
  if (!entrypoint_args.empty()) {
    entrypoint_argv.reserve(entrypoint_args.size());
    for (const auto& arg : entrypoint_args) {
      entrypoint_argv.push_back(arg.c_str());
    }
  }

  auto child_surface_manager =
      std::make_shared<AndroidSurfaceManager>(rendering_api_);
  auto child_compositor = std::make_unique<AndroidCompositor>(
      child_surface_manager, jni_facade, raster_task_runner_);
  FlutterCompositor child_flutter_compositor =
      child_compositor->GetFlutterCompositor();

  // Create child AndroidEngine shell first so that we can pass user_data = child_engine.get().
  std::unique_ptr<AndroidEngine> child_engine(new AndroidEngine(
      settings_, jni_facade, rendering_api_, /*engine_handle=*/nullptr,
      child_surface_manager, std::move(child_compositor), platform_task_runner_,
      raster_task_runner_, ui_task_runner_, io_task_runner_));

  FlutterProjectArgs project_args = {};
  project_args.struct_size = sizeof(FlutterProjectArgs);
  project_args.compositor = &child_flutter_compositor;
  project_args.platform_message_callback = &AndroidEngine::OnPlatformMessageThunk;
  project_args.vsync_callback = &AndroidEngine::OnVsyncThunk;
  project_args.update_semantics_callback2 =
      &AndroidEngine::OnUpdateSemanticsThunk2;
  project_args.request_dart_deferred_library_callback =
      &AndroidEngine::OnRequestDartDeferredLibraryThunk;
  project_args.on_pre_engine_restart_callback =
      &AndroidEngine::OnPreEngineRestartThunk;

  FlutterEngineSpawnInfo spawn_info = {};
  spawn_info.struct_size = sizeof(FlutterEngineSpawnInfo);
  spawn_info.entrypoint = entrypoint.empty() ? nullptr : entrypoint.c_str();
  spawn_info.library_path = library_url.empty() ? nullptr : library_url.c_str();
  spawn_info.initial_route =
      initial_route.empty() ? nullptr : initial_route.c_str();
  spawn_info.entrypoint_argc = static_cast<int>(entrypoint_argv.size());
  spawn_info.entrypoint_argv =
      entrypoint_argv.empty() ? nullptr : entrypoint_argv.data();
  spawn_info.renderer_config = &renderer_config;
  spawn_info.project_args = &project_args;
  spawn_info.user_data = child_engine.get();

  FLUTTER_API_SYMBOL(FlutterEngine) spawned_handle = nullptr;
  FlutterEngineResult result =
      embedder_api_.Spawn(engine_, &spawn_info, &spawned_handle);
  if (result != kSuccess || spawned_handle == nullptr) {
    FML_LOG(ERROR) << "AndroidEngine::Spawn: FlutterEngineSpawn failed: "
                   << result;
    return nullptr;
  }

  child_engine->engine_ = spawned_handle;
  child_engine->engine_handle_holder_->store(spawned_handle);

  return child_engine;
}

void AndroidEngine::OnSurfaceCreated(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  if (compositor_) {
    compositor_->OnSurfaceCreated(std::move(native_window));
  }
}

void AndroidEngine::OnSurfaceWindowChanged(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  if (compositor_) {
    compositor_->OnSurfaceWindowChanged(std::move(native_window));
  }
}

void AndroidEngine::OnSurfaceDestroyed() {
  if (compositor_) {
    compositor_->OnSurfaceDestroyed();
  }
}

void AndroidEngine::SetViewportMetrics(int64_t view_id,
                                       const ViewportMetrics& metrics) {
  if (!IsRunning()) {
    return;
  }
  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(FlutterWindowMetricsEvent);
  event.view_id = view_id;
  event.width = metrics.physical_width;
  event.height = metrics.physical_height;
  event.pixel_ratio = metrics.device_pixel_ratio;
  event.physical_view_inset_top = metrics.physical_view_inset_top;
  event.physical_view_inset_right = metrics.physical_view_inset_right;
  event.physical_view_inset_bottom = metrics.physical_view_inset_bottom;
  event.physical_view_inset_left = metrics.physical_view_inset_left;
  event.display_id = metrics.display_id;

  embedder_api_.SendWindowMetricsEvent(engine_, &event);
}

void AndroidEngine::SetViewportMetrics(const FlutterWindowMetricsEvent& event) {
  if (!IsRunning()) {
    return;
  }
  embedder_api_.SendWindowMetricsEvent(engine_, &event);
}

void AndroidEngine::DispatchPointerDataPacket(const uint8_t* data,
                                              size_t size) {
  if (!IsRunning() || data == nullptr || size == 0) {
    return;
  }

  size_t count = size / sizeof(PointerData);
  if (count == 0) {
    return;
  }

  const auto* pointer_data = reinterpret_cast<const PointerData*>(data);
  std::vector<FlutterPointerEvent> events(count);

  for (size_t i = 0; i < count; ++i) {
    const PointerData& pd = pointer_data[i];
    FlutterPointerEvent& event = events[i];
    event.struct_size = sizeof(FlutterPointerEvent);
    event.phase = ConvertPointerChangeToPhase(pd.change);
    event.timestamp = static_cast<size_t>(pd.time_stamp);
    event.x = pd.physical_x;
    event.y = pd.physical_y;
    event.device = static_cast<int32_t>(pd.device);
    event.signal_kind = ConvertPointerSignalKind(pd.signal_kind);
    event.scroll_delta_x = pd.scroll_delta_x;
    event.scroll_delta_y = pd.scroll_delta_y;
    event.device_kind = ConvertPointerDeviceKind(pd.kind);
    event.buttons = pd.buttons;
    event.pan_x = pd.pan_x;
    event.pan_y = pd.pan_y;
    event.scale = pd.scale;
    event.rotation = pd.rotation;
    event.view_id = pd.view_id;
    event.pressure = pd.pressure;
    event.pressure_min = pd.pressure_min;
    event.pressure_max = pd.pressure_max;
  }

  embedder_api_.SendPointerEvent(engine_, events.data(), events.size());
}

void AndroidEngine::DispatchPointerEvents(const FlutterPointerEvent* events,
                                          size_t count) {
  if (!IsRunning() || events == nullptr || count == 0) {
    return;
  }
  embedder_api_.SendPointerEvent(engine_, events, count);
}

void AndroidEngine::SendPlatformMessage(const char* channel,
                                        const uint8_t* message,
                                        size_t message_size,
                                        int32_t response_id) {
  if (!IsRunning() || channel == nullptr) {
    return;
  }

  FlutterPlatformMessage msg = {};
  msg.struct_size = sizeof(FlutterPlatformMessage);
  msg.channel = channel;
  msg.message = message;
  msg.message_size = message_size;

  embedder_api_.SendPlatformMessage(engine_, &msg);
}

void AndroidEngine::SendPlatformMessageResponse(int32_t response_id,
                                                const uint8_t* data,
                                                size_t data_size) {
  const FlutterPlatformMessageResponseHandle* handle = nullptr;
  {
    std::lock_guard<std::mutex> lock(pending_responses_mutex_);
    auto it = pending_responses_.find(response_id);
    if (it != pending_responses_.end()) {
      handle = it->second;
      pending_responses_.erase(it);
    }
  }

  if (handle == nullptr) {
    return;
  }

  if (IsRunning()) {
    embedder_api_.SendPlatformMessageResponse(engine_, handle, data,
                                              data_size);
  } else if (embedder_api_.PlatformMessageReleaseResponseHandle != nullptr) {
    embedder_api_.PlatformMessageReleaseResponseHandle(
        engine_,
        const_cast<FlutterPlatformMessageResponseHandle*>(handle));
  }
}

void AndroidEngine::SetSemanticsEnabled(bool enabled) {
  if (!IsRunning()) {
    return;
  }
  embedder_api_.UpdateSemanticsEnabled(engine_, enabled);
}

void AndroidEngine::SetAccessibilityFeatures(int32_t flags) {
  if (!IsRunning()) {
    return;
  }
  embedder_api_.UpdateAccessibilityFeatures(
      engine_, static_cast<FlutterAccessibilityFeature>(flags));
}

void AndroidEngine::DispatchSemanticsAction(int64_t view_id,
                                            int32_t node_id,
                                            FlutterSemanticsAction action,
                                            const uint8_t* data,
                                            size_t data_size) {
  if (!IsRunning()) {
    return;
  }
  embedder_api_.DispatchSemanticsAction(engine_, node_id, action, data,
                                        data_size);
}

bool AndroidEngine::RegisterExternalTexture(int64_t texture_id) {
  if (!IsRunning()) {
    return false;
  }
  return embedder_api_.RegisterExternalTexture(engine_, texture_id) ==
         kSuccess;
}

bool AndroidEngine::UnregisterExternalTexture(int64_t texture_id) {
  if (!IsRunning()) {
    return false;
  }
  return embedder_api_.UnregisterExternalTexture(engine_, texture_id) ==
         kSuccess;
}

bool AndroidEngine::MarkExternalTextureFrameAvailable(int64_t texture_id) {
  if (!IsRunning()) {
    return false;
  }
  return embedder_api_.MarkExternalTextureFrameAvailable(engine_, texture_id) ==
         kSuccess;
}

bool AndroidEngine::LoadDartDeferredLibrary(
    intptr_t loading_unit_id,
    const uint8_t* snapshot_data,
    size_t snapshot_data_size,
    const uint8_t* snapshot_instructions,
    size_t snapshot_instructions_size) {
  if (!IsRunning()) {
    return false;
  }

  FlutterLoadDeferredLibraryInfo info = {};
  info.struct_size = sizeof(FlutterLoadDeferredLibraryInfo);
  info.loading_unit_id = loading_unit_id;
  info.isolate_snapshot_data = snapshot_data;
  info.isolate_snapshot_data_size = snapshot_data_size;
  info.isolate_snapshot_instructions = snapshot_instructions;
  info.isolate_snapshot_instructions_size = snapshot_instructions_size;

  return embedder_api_.LoadDartDeferredLibrary(engine_, &info) == kSuccess;
}

bool AndroidEngine::LoadDartDeferredLibraryError(intptr_t loading_unit_id,
                                                 const char* error_message,
                                                 bool transient) {
  if (!IsRunning()) {
    return false;
  }

  FlutterLoadDeferredLibraryErrorInfo info = {};
  info.struct_size = sizeof(FlutterLoadDeferredLibraryErrorInfo);
  info.loading_unit_id = loading_unit_id;
  info.error_message = error_message;
  info.transient = transient;

  return embedder_api_.LoadDartDeferredLibraryError(engine_, &info) ==
         kSuccess;
}

FlutterEngineScreenshot AndroidEngine::Screenshot(
    FlutterEngineScreenshotType type,
    bool base64_encode) {
  if (!IsRunning()) {
    return {};
  }

  FlutterEngineScreenshotInfo info = {};
  info.struct_size = sizeof(FlutterEngineScreenshotInfo);
  info.type = type;
  info.base64_encode = base64_encode;

  FlutterEngineScreenshot screenshot = {};
  screenshot.struct_size = sizeof(FlutterEngineScreenshot);

  if (embedder_api_.GetScreenshot(engine_, &info, &screenshot) != kSuccess) {
    return {};
  }

  return screenshot;
}

void AndroidEngine::ReleaseScreenshot(
    const FlutterEngineScreenshot* screenshot) {
  if (screenshot != nullptr && embedder_api_.ReleaseScreenshot != nullptr) {
    embedder_api_.ReleaseScreenshot(
        const_cast<FlutterEngineScreenshot*>(screenshot));
  }
}

void AndroidEngine::OnVsync(intptr_t baton,
                            uint64_t frame_start_time_nanos,
                            uint64_t frame_target_time_nanos) {
  if (IsRunning()) {
    embedder_api_.OnVsync(engine_, baton, frame_start_time_nanos,
                          frame_target_time_nanos);
  }
}

void AndroidEngine::NotifyLowMemoryWarning() {
  if (IsRunning()) {
    embedder_api_.NotifyLowMemoryWarning(engine_);
  }
}

// static
void AndroidEngine::OnPlatformMessageThunk(
    const FlutterPlatformMessage* message,
    void* user_data) {
  if (user_data != nullptr) {
    static_cast<AndroidEngine*>(user_data)->HandlePlatformMessage(message);
  }
}

// static
void AndroidEngine::OnVsyncThunk(void* user_data, intptr_t baton) {
  if (user_data != nullptr) {
    static_cast<AndroidEngine*>(user_data)->HandleVsyncRequest(baton);
  }
}

// static
void AndroidEngine::OnUpdateSemanticsThunk2(
    const FlutterSemanticsUpdate2* update,
    void* user_data) {
  if (user_data != nullptr) {
    static_cast<AndroidEngine*>(user_data)->HandleSemanticsUpdate2(update);
  }
}

// static
void AndroidEngine::OnRequestDartDeferredLibraryThunk(intptr_t loading_unit_id,
                                                      void* user_data) {
  if (user_data != nullptr) {
    static_cast<AndroidEngine*>(user_data)->HandleRequestDartDeferredLibrary(
        loading_unit_id);
  }
}

// static
void AndroidEngine::OnPreEngineRestartThunk(void* user_data) {
  if (user_data != nullptr) {
    static_cast<AndroidEngine*>(user_data)->HandlePreEngineRestart();
  }
}

void AndroidEngine::HandlePlatformMessage(
    const FlutterPlatformMessage* message) {
  if (message == nullptr || jni_facade_ == nullptr) {
    return;
  }

  int32_t response_id = next_response_id_.fetch_add(1);
  if (message->response_handle != nullptr) {
    std::lock_guard<std::mutex> lock(pending_responses_mutex_);
    pending_responses_[response_id] = message->response_handle;
  }

  fml::MallocMapping data =
      fml::MallocMapping::Copy(message->message, message->message_size);
  auto platform_msg = std::make_unique<PlatformMessage>(
      message->channel, std::move(data), nullptr);

  jni_facade_->FlutterViewHandlePlatformMessage(std::move(platform_msg),
                                                response_id);
}

void AndroidEngine::HandleVsyncRequest(intptr_t baton) {
  // External vsync events are driven via AndroidEngine::OnVsync.
}

void AndroidEngine::HandleSemanticsUpdate2(
    const FlutterSemanticsUpdate2* update) {
  if (update == nullptr || jni_facade_ == nullptr) {
    return;
  }

  size_t num_bytes = 0;
  for (size_t i = 0; i < update->node_count; ++i) {
    const FlutterSemanticsNode2* node = update->nodes[i];
    if (node == nullptr) {
      continue;
    }
    num_bytes += kBytesPerNode;
    num_bytes += node->child_count * kBytesPerChild;  // traversal order
    num_bytes += node->child_count * kBytesPerChild;  // hit test order
    num_bytes += node->custom_accessibility_actions_count *
                 kBytesPerCustomAction;
    num_bytes += node->label_attribute_count * kBytesPerStringAttribute;
    num_bytes += node->value_attribute_count * kBytesPerStringAttribute;
    num_bytes += node->increased_value_attribute_count *
                 kBytesPerStringAttribute;
    num_bytes += node->decreased_value_attribute_count *
                 kBytesPerStringAttribute;
    num_bytes += node->hint_attribute_count * kBytesPerStringAttribute;
  }

  std::vector<uint8_t> buffer(num_bytes);
  std::vector<std::string> strings;
  std::vector<std::vector<uint8_t>> string_attribute_args;

  if (!buffer.empty()) {
    int32_t* buffer_int32 = reinterpret_cast<int32_t*>(buffer.data());
    float* buffer_float32 = reinterpret_cast<float*>(buffer.data());
    size_t position = 0;

    for (size_t i = 0; i < update->node_count; ++i) {
      const FlutterSemanticsNode2* node = update->nodes[i];
      if (node == nullptr) {
        continue;
      }
      buffer_int32[position++] = node->id;
      int64_t flags = ConvertFlagsToInt64(node);
      std::memcpy(&buffer_int32[position], &flags, sizeof(int64_t));
      position += 2;
      buffer_int32[position++] = node->actions;
      buffer_int32[position++] = node->max_value_length;
      buffer_int32[position++] = node->current_value_length;
      buffer_int32[position++] = node->text_selection_base;
      buffer_int32[position++] = node->text_selection_extent;
      buffer_int32[position++] = node->platform_view_id;
      buffer_int32[position++] = node->scroll_child_count;
      buffer_int32[position++] = node->scroll_index;
      buffer_int32[position++] = node->traversal_parent;
      buffer_float32[position++] = static_cast<float>(node->scroll_position);
      buffer_float32[position++] = static_cast<float>(node->scroll_extent_max);
      buffer_float32[position++] = static_cast<float>(node->scroll_extent_min);
      buffer_int32[position++] = static_cast<int32_t>(node->role);

      PutStringIntoBuffer(node->identifier, buffer_int32, &position, strings);
      PutStringIntoBuffer(node->label, buffer_int32, &position, strings);
      PutStringAttributesIntoBuffer(node->label_attribute_count,
                                    node->label_attributes, buffer_int32,
                                    &position, string_attribute_args);

      PutStringIntoBuffer(node->value, buffer_int32, &position, strings);
      PutStringAttributesIntoBuffer(node->value_attribute_count,
                                    node->value_attributes, buffer_int32,
                                    &position, string_attribute_args);

      PutStringIntoBuffer(node->increased_value, buffer_int32, &position,
                          strings);
      PutStringAttributesIntoBuffer(node->increased_value_attribute_count,
                                    node->increased_value_attributes,
                                    buffer_int32, &position,
                                    string_attribute_args);

      PutStringIntoBuffer(node->decreased_value, buffer_int32, &position,
                          strings);
      PutStringAttributesIntoBuffer(node->decreased_value_attribute_count,
                                    node->decreased_value_attributes,
                                    buffer_int32, &position,
                                    string_attribute_args);

      PutStringIntoBuffer(node->hint, buffer_int32, &position, strings);
      PutStringAttributesIntoBuffer(node->hint_attribute_count,
                                    node->hint_attributes, buffer_int32,
                                    &position, string_attribute_args);

      PutStringIntoBuffer(node->tooltip, buffer_int32, &position, strings);
      PutStringIntoBuffer(node->link_url, buffer_int32, &position, strings);
      PutStringIntoBuffer(node->locale, buffer_int32, &position, strings);
      PutStringIntoBuffer(node->min_value, buffer_int32, &position, strings);
      PutStringIntoBuffer(node->max_value, buffer_int32, &position, strings);

      buffer_int32[position++] = node->heading_level;
      buffer_int32[position++] = node->text_direction;
      buffer_float32[position++] = static_cast<float>(node->rect.left);
      buffer_float32[position++] = static_cast<float>(node->rect.top);
      buffer_float32[position++] = static_cast<float>(node->rect.right);
      buffer_float32[position++] = static_cast<float>(node->rect.bottom);

      PutTransformationIntoBuffer(node->transform, buffer_float32, &position);
      PutTransformationIntoBuffer(node->hit_test_transform, buffer_float32,
                                  &position);

      buffer_int32[position++] = static_cast<int32_t>(node->child_count);
      if (node->children_in_traversal_order != nullptr) {
        for (size_t c = 0; c < node->child_count; ++c) {
          buffer_int32[position++] = node->children_in_traversal_order[c];
        }
      }

      buffer_int32[position++] = static_cast<int32_t>(node->child_count);
      if (node->children_in_hit_test_order != nullptr) {
        for (size_t c = 0; c < node->child_count; ++c) {
          buffer_int32[position++] = node->children_in_hit_test_order[c];
        }
      }

      buffer_int32[position++] =
          static_cast<int32_t>(node->custom_accessibility_actions_count);
      if (node->custom_accessibility_actions != nullptr) {
        for (size_t a = 0; a < node->custom_accessibility_actions_count; ++a) {
          buffer_int32[position++] = node->custom_accessibility_actions[a];
        }
      }
    }

    jni_facade_->FlutterViewUpdateSemantics(buffer, strings,
                                            string_attribute_args);
  }

  // Custom accessibility actions updates.
  if (update->custom_action_count > 0 && update->custom_actions != nullptr) {
    size_t num_action_bytes = update->custom_action_count * kBytesPerAction;
    std::vector<uint8_t> actions_buffer(num_action_bytes);
    int32_t* actions_buffer_int32 =
        reinterpret_cast<int32_t*>(actions_buffer.data());
    std::vector<std::string> action_strings;
    size_t actions_position = 0;

    for (size_t i = 0; i < update->custom_action_count; ++i) {
      const FlutterSemanticsCustomAction2* action = update->custom_actions[i];
      if (action == nullptr) {
        continue;
      }
      actions_buffer_int32[actions_position++] = action->id;
      actions_buffer_int32[actions_position++] = action->override_action;
      PutStringIntoBuffer(action->label, actions_buffer_int32,
                          &actions_position, action_strings);
      PutStringIntoBuffer(action->hint, actions_buffer_int32, &actions_position,
                          action_strings);
    }

    jni_facade_->FlutterViewUpdateCustomAccessibilityActions(actions_buffer,
                                                             action_strings);
  }
}

void AndroidEngine::HandleRequestDartDeferredLibrary(intptr_t loading_unit_id) {
  if (jni_facade_ != nullptr) {
    jni_facade_->RequestDartDeferredLibrary(loading_unit_id);
  }
}

void AndroidEngine::HandlePreEngineRestart() {
  if (jni_facade_ != nullptr) {
    jni_facade_->FlutterViewOnPreEngineRestart();
  }
}

}  // namespace flutter
