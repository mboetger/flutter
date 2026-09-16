// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/platform_view_android.h"

#include <android/api-level.h>
#include <sys/system_properties.h>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

#include "common/settings.h"
#include "flutter/common/graphics/texture.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/fml/task_runner_util.h"
#include "flutter/fml/trace_event.h"
#include "flutter/shell/common/shell_io_manager.h"
#include "flutter/shell/gpu/gpu_surface_gl_delegate.h"
#include "flutter/shell/platform/android/android_context_dynamic_impeller.h"
#include "flutter/shell/platform/android/android_context_gl_impeller.h"
#include "flutter/shell/platform/android/android_context_vk_impeller.h"
#include "flutter/shell/platform/android/android_rendering_selector.h"
#include "flutter/shell/platform/android/android_surface_dynamic_impeller.h"
#include "flutter/shell/platform/android/android_surface_gl_impeller.h"
#include "flutter/shell/platform/android/image_external_texture_gl_impeller.h"
#include "flutter/shell/platform/android/surface_texture_external_texture_gl_impeller.h"
#include "flutter/shell/platform/android/surface_texture_external_texture_vk_impeller.h"

#if !SLIMPELLER
#include "flutter/shell/platform/android/android_context_gl_skia.h"
#include "flutter/shell/platform/android/android_surface_gl_skia.h"
#include "flutter/shell/platform/android/android_surface_software.h"
#include "flutter/shell/platform/android/image_external_texture_gl_skia.h"
#include "flutter/shell/platform/android/surface_texture_external_texture_gl_skia.h"
#endif  // !SLIMPELLER

#include "fml/logging.h"
#include "impeller/display_list/aiks_context.h"
#if IMPELLER_ENABLE_VULKAN  // b/258506856 for why this is behind an if
#include "flutter/shell/platform/android/android_surface_vk_impeller.h"
#include "flutter/shell/platform/android/image_external_texture_vk_impeller.h"
#endif
#include "flutter/shell/platform/android/android_compositor_adapter.h"
#include "flutter/shell/platform/android/android_external_texture_adapter.h"
#include "flutter/shell/platform/android/context/android_context.h"
#include "flutter/shell/platform/android/external_view_embedder/external_view_embedder_wrapper.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/platform_message_response_android.h"
#include "flutter/shell/platform/android/surface/android_surface.h"
#include "flutter/shell/platform/android/surface/snapshot_surface_producer.h"
#include "flutter/shell/platform/android/vsync_waiter_android.h"

namespace flutter {

namespace {

static constexpr int kMinAPILevelHCPP = 34;
static constexpr int64_t kImplicitViewId = 0;

AndroidContext::ContextSettings CreateContextSettings(
    const Settings& p_settings) {
  AndroidContext::ContextSettings settings;
  settings.enable_gpu_tracing = p_settings.enable_vulkan_gpu_tracing;
  settings.enable_validation = p_settings.enable_vulkan_validation;
  settings.enable_surface_control = p_settings.enable_surface_control;
  return settings;
}

#define ANDROID_SAFE_ACCESS(pointer, member, default_value)               \
  ([=]() {                                                                \
    if ((offsetof(std::remove_pointer<decltype(pointer)>::type, member) + \
             sizeof(pointer->member) <=                                   \
         pointer->struct_size)) {                                         \
      return pointer->member;                                             \
    }                                                                     \
    return static_cast<decltype(pointer->member)>((default_value));       \
  })()

flutter::PointerData::Change ToPointerDataChange(FlutterPointerPhase phase) {
  switch (phase) {
    case FlutterPointerPhase::kCancel:
      return flutter::PointerData::Change::kCancel;
    case FlutterPointerPhase::kUp:
      return flutter::PointerData::Change::kUp;
    case FlutterPointerPhase::kDown:
      return flutter::PointerData::Change::kDown;
    case FlutterPointerPhase::kMove:
      return flutter::PointerData::Change::kMove;
    case FlutterPointerPhase::kAdd:
      return flutter::PointerData::Change::kAdd;
    case FlutterPointerPhase::kRemove:
      return flutter::PointerData::Change::kRemove;
    case FlutterPointerPhase::kHover:
      return flutter::PointerData::Change::kHover;
    case FlutterPointerPhase::kPanZoomStart:
      return flutter::PointerData::Change::kPanZoomStart;
    case FlutterPointerPhase::kPanZoomUpdate:
      return flutter::PointerData::Change::kPanZoomUpdate;
    case FlutterPointerPhase::kPanZoomEnd:
      return flutter::PointerData::Change::kPanZoomEnd;
  }
  return flutter::PointerData::Change::kCancel;
}

FlutterPointerPhase ToFlutterPointerPhase(flutter::PointerData::Change change) {
  switch (change) {
    case flutter::PointerData::Change::kCancel:
      return FlutterPointerPhase::kCancel;
    case flutter::PointerData::Change::kUp:
      return FlutterPointerPhase::kUp;
    case flutter::PointerData::Change::kDown:
      return FlutterPointerPhase::kDown;
    case flutter::PointerData::Change::kMove:
      return FlutterPointerPhase::kMove;
    case flutter::PointerData::Change::kAdd:
      return FlutterPointerPhase::kAdd;
    case flutter::PointerData::Change::kRemove:
      return FlutterPointerPhase::kRemove;
    case flutter::PointerData::Change::kHover:
      return FlutterPointerPhase::kHover;
    case flutter::PointerData::Change::kPanZoomStart:
      return FlutterPointerPhase::kPanZoomStart;
    case flutter::PointerData::Change::kPanZoomUpdate:
      return FlutterPointerPhase::kPanZoomUpdate;
    case flutter::PointerData::Change::kPanZoomEnd:
      return FlutterPointerPhase::kPanZoomEnd;
  }
  return FlutterPointerPhase::kCancel;
}

flutter::PointerData::DeviceKind ToPointerDataKind(
    FlutterPointerDeviceKind device_kind) {
  switch (device_kind) {
    case FlutterPointerDeviceKind::kFlutterPointerDeviceKindMouse:
      return flutter::PointerData::DeviceKind::kMouse;
    case FlutterPointerDeviceKind::kFlutterPointerDeviceKindTouch:
      return flutter::PointerData::DeviceKind::kTouch;
    case FlutterPointerDeviceKind::kFlutterPointerDeviceKindStylus:
      return flutter::PointerData::DeviceKind::kStylus;
    case FlutterPointerDeviceKind::kFlutterPointerDeviceKindInvertedStylus:
      return flutter::PointerData::DeviceKind::kInvertedStylus;
    case FlutterPointerDeviceKind::kFlutterPointerDeviceKindTrackpad:
      return flutter::PointerData::DeviceKind::kTrackpad;
  }
  return flutter::PointerData::DeviceKind::kTouch;
}

FlutterPointerDeviceKind ToFlutterPointerDeviceKind(
    flutter::PointerData::DeviceKind kind) {
  switch (kind) {
    case flutter::PointerData::DeviceKind::kMouse:
      return FlutterPointerDeviceKind::kFlutterPointerDeviceKindMouse;
    case flutter::PointerData::DeviceKind::kTouch:
      return FlutterPointerDeviceKind::kFlutterPointerDeviceKindTouch;
    case flutter::PointerData::DeviceKind::kStylus:
    case flutter::PointerData::DeviceKind::kInvertedStylus:
      return FlutterPointerDeviceKind::kFlutterPointerDeviceKindStylus;
    case flutter::PointerData::DeviceKind::kTrackpad:
      return FlutterPointerDeviceKind::kFlutterPointerDeviceKindTrackpad;
  }
  return FlutterPointerDeviceKind::kFlutterPointerDeviceKindTouch;
}

flutter::PointerData::SignalKind ToPointerDataSignalKind(
    FlutterPointerSignalKind signal_kind) {
  switch (signal_kind) {
    case FlutterPointerSignalKind::kFlutterPointerSignalKindNone:
      return flutter::PointerData::SignalKind::kNone;
    case FlutterPointerSignalKind::kFlutterPointerSignalKindScroll:
      return flutter::PointerData::SignalKind::kScroll;
    case FlutterPointerSignalKind::kFlutterPointerSignalKindScrollInertiaCancel:
      return flutter::PointerData::SignalKind::kScrollInertiaCancel;
    case FlutterPointerSignalKind::kFlutterPointerSignalKindScale:
      return flutter::PointerData::SignalKind::kScale;
  }
  return flutter::PointerData::SignalKind::kNone;
}

FlutterPointerSignalKind ToFlutterPointerSignalKind(
    flutter::PointerData::SignalKind signal_kind) {
  switch (signal_kind) {
    case flutter::PointerData::SignalKind::kNone:
      return FlutterPointerSignalKind::kFlutterPointerSignalKindNone;
    case flutter::PointerData::SignalKind::kScroll:
      return FlutterPointerSignalKind::kFlutterPointerSignalKindScroll;
    case flutter::PointerData::SignalKind::kScrollInertiaCancel:
      return FlutterPointerSignalKind::
          kFlutterPointerSignalKindScrollInertiaCancel;
    case flutter::PointerData::SignalKind::kScale:
      return FlutterPointerSignalKind::kFlutterPointerSignalKindScale;
  }
  return FlutterPointerSignalKind::kFlutterPointerSignalKindNone;
}

}  // namespace

AndroidSurfaceFactoryImpl::AndroidSurfaceFactoryImpl(
    const std::shared_ptr<AndroidContext>& context,
    bool enable_impeller,
    bool lazy_shader_mode)
    : android_context_(context),
      enable_impeller_(enable_impeller),
      lazy_shader_mode_(lazy_shader_mode) {}

AndroidSurfaceFactoryImpl::~AndroidSurfaceFactoryImpl() = default;

std::unique_ptr<AndroidSurface> AndroidSurfaceFactoryImpl::CreateSurface() {
  if (android_context_->IsDynamicSelection()) {
    auto cast_ptr = std::static_pointer_cast<AndroidContextDynamicImpeller>(
        android_context_);
    return std::make_unique<AndroidSurfaceDynamicImpeller>(cast_ptr);
  }
  switch (android_context_->RenderingApi()) {
#if !SLIMPELLER
    case AndroidRenderingAPI::kSoftware:
      return std::make_unique<AndroidSurfaceSoftware>();
    case AndroidRenderingAPI::kSkiaOpenGLES:
      return std::make_unique<AndroidSurfaceGLSkia>(
          std::static_pointer_cast<AndroidContextGLSkia>(android_context_));
#endif  // !SLIMPELLER
    case AndroidRenderingAPI::kImpellerOpenGLES:
      return std::make_unique<AndroidSurfaceGLImpeller>(
          std::static_pointer_cast<AndroidContextGLImpeller>(android_context_));
    case AndroidRenderingAPI::kImpellerVulkan:
      return std::make_unique<AndroidSurfaceVKImpeller>(
          std::static_pointer_cast<AndroidContextVKImpeller>(android_context_));
    case AndroidRenderingAPI::kImpellerAutoselect: {
      auto cast_ptr = std::static_pointer_cast<AndroidContextDynamicImpeller>(
          android_context_);
      return std::make_unique<AndroidSurfaceDynamicImpeller>(cast_ptr);
    }
  }
  FML_UNREACHABLE();
}

static std::shared_ptr<flutter::AndroidContext> CreateAndroidContext(
    const flutter::TaskRunners& task_runners,
    AndroidRenderingAPI android_rendering_api,
    bool enable_opengl_gpu_tracing,
    const AndroidContext::ContextSettings& settings,
    std::shared_ptr<fml::BasicTaskRunner> io_task_runner) {
  switch (android_rendering_api) {
#if !SLIMPELLER
    case AndroidRenderingAPI::kSoftware:
      return std::make_shared<AndroidContext>(AndroidRenderingAPI::kSoftware);
    case AndroidRenderingAPI::kSkiaOpenGLES:
      return std::make_unique<AndroidContextGLSkia>(
          fml::MakeRefCounted<AndroidEnvironmentGL>(),  //
          task_runners                                  //
      );
#endif  // !SLIMPELLER
    case AndroidRenderingAPI::kImpellerVulkan:
      return std::make_unique<AndroidContextVKImpeller>(settings);
    case AndroidRenderingAPI::kImpellerOpenGLES:
      return std::make_unique<AndroidContextGLImpeller>(
          std::make_unique<impeller::egl::Display>(), enable_opengl_gpu_tracing,
          std::move(io_task_runner));
    case AndroidRenderingAPI::kImpellerAutoselect:
      // Determine if we're using GL or Vulkan.
      return std::make_unique<AndroidContextDynamicImpeller>(
          settings, std::move(io_task_runner));
  }
  FML_UNREACHABLE();
}

PlatformViewAndroid::PlatformViewAndroid(
    const flutter::Settings& settings,
    const flutter::TaskRunners& task_runners,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    AndroidRenderingAPI rendering_api,
    std::shared_ptr<fml::BasicTaskRunner> io_task_runner)
    : PlatformViewAndroid(
          settings,
          task_runners,
          jni_facade,
          CreateAndroidContext(
              task_runners,
              rendering_api,
              settings.enable_opengl_gpu_tracing,
              CreateContextSettings(settings),
              io_task_runner ? std::move(io_task_runner)
                             : std::make_shared<fml::WrapperBasicTaskRunner>(
                                   task_runners.GetIOTaskRunner()))) {}

PlatformViewAndroid::PlatformViewAndroid(
    const flutter::Settings& settings,
    const flutter::TaskRunners& task_runners,
    const std::shared_ptr<PlatformViewAndroidJNI>& jni_facade,
    const std::shared_ptr<flutter::AndroidContext>& android_context)
    : task_runners_(task_runners),
      jni_facade_(jni_facade),
      android_context_(android_context),
      platform_view_android_delegate_(jni_facade),
      platform_message_handler_(new PlatformMessageHandlerAndroid(jni_facade)),
      android_embedder_api_(settings.android_embedder_api),
      weak_factory_(this) {
  if (android_context_) {
    FML_CHECK(android_context_->IsValid())
        << "Could not create surface from invalid Android context.";
    surface_factory_ = std::make_shared<AndroidSurfaceFactoryImpl>(
        android_context_,                          //
        settings.enable_impeller,                  //
        settings.impeller_enable_lazy_shader_mode  //
    );
    android_surface_ = surface_factory_->CreateSurface();
    android_meets_hcpp_criteria_ =
        settings.enable_surface_control &&
        android_get_device_api_level() >= kMinAPILevelHCPP &&
        settings.enable_impeller;
    FML_CHECK(android_surface_ && android_surface_->IsValid())
        << "Could not create an OpenGL, Vulkan or Software surface to set "
           "up "
           "rendering.";
  }
  surface_lifecycle_ = std::make_unique<AndroidSurfaceLifecycle>(
      task_runners_.GetRasterTaskRunner(), jni_facade_, android_surface_.get(),
      this);
  external_texture_adapter_ = std::make_unique<AndroidExternalTextureAdapter>(
      android_context_, jni_facade_, this);
  TRACE_EVENT1("flutter", "AndroidEmbedderApiState", "enabled",
               android_embedder_api_ ? "true" : "false");
}

PlatformViewAndroid::~PlatformViewAndroid() = default;

void PlatformViewAndroid::SetPlatformView(PlatformView* platform_view) {
  platform_view_ = platform_view;
}

PlatformView* PlatformViewAndroid::GetPlatformViewDelegate() const {
  return platform_view_;
}

void PlatformViewAndroid::NotifyCreated(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  if (surface_lifecycle_) {
    surface_lifecycle_->NotifyCreated(std::move(native_window));
  }
}

void PlatformViewAndroid::NotifySurfaceWindowChanged(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  if (surface_lifecycle_) {
    surface_lifecycle_->NotifySurfaceWindowChanged(std::move(native_window));
  }
}

void PlatformViewAndroid::NotifyDestroyed() {
  if (surface_lifecycle_) {
    surface_lifecycle_->NotifyDestroyed();
  }
}

void PlatformViewAndroid::NotifyChanged(const DlISize& size) {
  if (surface_lifecycle_) {
    surface_lifecycle_->NotifyChanged(size);
  }
}

void PlatformViewAndroid::SetGpuAvailability(
    FlutterGpuAvailability availability) {
  if (surface_lifecycle_) {
    surface_lifecycle_->SetGpuAvailability(availability);
  }
}

void PlatformViewAndroid::OnSurfaceCreated() {
  if (platform_view_) {
    platform_view_->NotifyCreated();
  }
}

void PlatformViewAndroid::OnSurfaceDestroyed() {
  if (platform_view_) {
    platform_view_->NotifyDestroyed();
  }
}

void PlatformViewAndroid::OnScheduleFrame() {
  if (platform_view_) {
    platform_view_->ScheduleFrame();
  }
}

void PlatformViewAndroid::OnInstallFirstFrameCallback() {
  InstallFirstFrameCallback();
}

void PlatformViewAndroid::DispatchPlatformMessage(JNIEnv* env,
                                                  std::string name,
                                                  jobject java_message_data,
                                                  jint java_message_position,
                                                  jint response_id) {
  uint8_t* message_data =
      static_cast<uint8_t*>(env->GetDirectBufferAddress(java_message_data));
  fml::MallocMapping message =
      fml::MallocMapping::Copy(message_data, java_message_position);

  fml::RefPtr<flutter::PlatformMessageResponse> response;
  if (response_id) {
    response = fml::MakeRefCounted<PlatformMessageResponseAndroid>(
        response_id, jni_facade_, task_runners_.GetPlatformTaskRunner());
  }

  if (platform_view_) {
    platform_view_->DispatchPlatformMessage(
        std::make_unique<flutter::PlatformMessage>(
            std::move(name), std::move(message), std::move(response)));
  }
}

void PlatformViewAndroid::DispatchEmptyPlatformMessage(JNIEnv* env,
                                                       std::string name,
                                                       jint response_id) {
  fml::RefPtr<flutter::PlatformMessageResponse> response;
  if (response_id) {
    response = fml::MakeRefCounted<PlatformMessageResponseAndroid>(
        response_id, jni_facade_, task_runners_.GetPlatformTaskRunner());
  }

  if (platform_view_) {
    platform_view_->DispatchPlatformMessage(
        std::make_unique<flutter::PlatformMessage>(std::move(name),
                                                   std::move(response)));
  }
}

void PlatformViewAndroid::DispatchPointerDataPacket(
    std::unique_ptr<PointerDataPacket> packet) {
  if (android_embedder_api_) {
    TRACE_EVENT1("flutter", "PlatformViewAndroid::DispatchPointerDataPacket",
                 "path", "embedder_api");
    if (!packet) {
      return;
    }
    size_t count = packet->GetLength();
    if (count == 0) {
      return;
    }
    std::vector<FlutterPointerEvent> events(count);
    for (size_t i = 0; i < count; ++i) {
      PointerData data = packet->GetPointerData(i);
      FlutterPointerEvent& event = events[i];
      event.struct_size = sizeof(FlutterPointerEvent);
      event.timestamp = data.time_stamp;
      event.phase = ToFlutterPointerPhase(data.change);
      event.x = data.physical_x;
      event.y = data.physical_y;
      event.device = data.device;
      event.signal_kind = ToFlutterPointerSignalKind(data.signal_kind);
      event.scroll_delta_x = data.scroll_delta_x;
      event.scroll_delta_y = data.scroll_delta_y;
      event.device_kind = ToFlutterPointerDeviceKind(data.kind);
      event.buttons = data.buttons;
      event.pan_x = data.pan_x;
      event.pan_y = data.pan_y;
      event.scale = data.scale;
      event.rotation = data.rotation;
      event.pressure = data.pressure;
      event.pressure_min = data.pressure_min;
      event.pressure_max = data.pressure_max;
      event.tilt = data.tilt;
      event.orientation = data.orientation;
      event.radius_major = data.radius_major;
      event.radius_minor = data.radius_minor;
      event.radius_min = data.radius_min;
      event.radius_max = data.radius_max;
      event.distance = data.distance;
      event.distance_max = data.distance_max;
      event.size = data.size;
      event.embedder_id = data.embedder_id;
      event.view_id = data.view_id;
    }
    SendPointerEvents(events.data(), events.size());
    return;
  }

  TRACE_EVENT1("flutter", "PlatformViewAndroid::DispatchPointerDataPacket",
               "path", "legacy");
  if (platform_view_) {
    platform_view_->DispatchPointerDataPacket(std::move(packet));
  }
}

FlutterEngineResult PlatformViewAndroid::SendPointerEvents(
    const FlutterPointerEvent* events,
    size_t count) {
  if (events == nullptr || count == 0) {
    return kInvalidArguments;
  }

  auto packet = std::make_unique<PointerDataPacket>(count);
  const FlutterPointerEvent* current = events;

  for (size_t i = 0; i < count; ++i) {
    PointerData pointer_data;
    pointer_data.Clear();
    pointer_data.embedder_id = ANDROID_SAFE_ACCESS(current, embedder_id, 0);
    pointer_data.time_stamp = ANDROID_SAFE_ACCESS(current, timestamp, 0);
    pointer_data.change = ToPointerDataChange(
        ANDROID_SAFE_ACCESS(current, phase, FlutterPointerPhase::kCancel));
    pointer_data.physical_x = ANDROID_SAFE_ACCESS(current, x, 0.0);
    pointer_data.physical_y = ANDROID_SAFE_ACCESS(current, y, 0.0);
    pointer_data.physical_delta_x = 0.0;
    pointer_data.physical_delta_y = 0.0;
    pointer_data.device = ANDROID_SAFE_ACCESS(current, device, 0);
    pointer_data.pointer_identifier = 0;
    pointer_data.signal_kind = ToPointerDataSignalKind(ANDROID_SAFE_ACCESS(
        current, signal_kind, kFlutterPointerSignalKindNone));
    pointer_data.scroll_delta_x =
        ANDROID_SAFE_ACCESS(current, scroll_delta_x, 0.0);
    pointer_data.scroll_delta_y =
        ANDROID_SAFE_ACCESS(current, scroll_delta_y, 0.0);
    FlutterPointerDeviceKind device_kind = ANDROID_SAFE_ACCESS(
        current, device_kind, kFlutterPointerDeviceKindTouch);
    pointer_data.kind = ToPointerDataKind(device_kind);
    pointer_data.buttons = ANDROID_SAFE_ACCESS(current, buttons, 0);
    pointer_data.pan_x = ANDROID_SAFE_ACCESS(current, pan_x, 0.0);
    pointer_data.pan_y = ANDROID_SAFE_ACCESS(current, pan_y, 0.0);
    pointer_data.pan_delta_x = 0.0;
    pointer_data.pan_delta_y = 0.0;
    pointer_data.scale = ANDROID_SAFE_ACCESS(current, scale, 0.0);
    pointer_data.rotation = ANDROID_SAFE_ACCESS(current, rotation, 0.0);
    pointer_data.pressure = ANDROID_SAFE_ACCESS(current, pressure, 0.0);
    pointer_data.pressure_min = ANDROID_SAFE_ACCESS(current, pressure_min, 0.0);
    pointer_data.pressure_max = ANDROID_SAFE_ACCESS(current, pressure_max, 0.0);
    pointer_data.tilt = ANDROID_SAFE_ACCESS(current, tilt, 0.0);
    pointer_data.orientation = ANDROID_SAFE_ACCESS(current, orientation, 0.0);
    pointer_data.radius_major = ANDROID_SAFE_ACCESS(current, radius_major, 0.0);
    pointer_data.radius_minor = ANDROID_SAFE_ACCESS(current, radius_minor, 0.0);
    pointer_data.radius_min = ANDROID_SAFE_ACCESS(current, radius_min, 0.0);
    pointer_data.radius_max = ANDROID_SAFE_ACCESS(current, radius_max, 0.0);
    pointer_data.distance = ANDROID_SAFE_ACCESS(current, distance, 0.0);
    pointer_data.distance_max = ANDROID_SAFE_ACCESS(current, distance_max, 0.0);
    pointer_data.size = ANDROID_SAFE_ACCESS(current, size, 0.0);
    pointer_data.view_id =
        ANDROID_SAFE_ACCESS(current, view_id, kImplicitViewId);
    packet->SetPointerData(i, pointer_data);
    current = reinterpret_cast<const FlutterPointerEvent*>(
        reinterpret_cast<const uint8_t*>(current) + current->struct_size);
  }

  if (platform_view_) {
    platform_view_->DispatchPointerDataPacket(std::move(packet));
  }
  return kSuccess;
}

void PlatformViewAndroid::SetViewportMetrics(int64_t view_id,
                                             const ViewportMetrics& metrics) {
  if (android_embedder_api_) {
    TRACE_EVENT1("flutter", "PlatformViewAndroid::SetViewportMetrics", "path",
                 "embedder_api");
    FlutterWindowMetricsEvent event = {};
    event.struct_size = sizeof(FlutterWindowMetricsEvent);
    event.view_id = view_id;
    event.width = static_cast<size_t>(metrics.physical_width);
    event.height = static_cast<size_t>(metrics.physical_height);
    event.pixel_ratio = metrics.device_pixel_ratio;
    event.left = 0;
    event.top = 0;
    event.physical_view_inset_top = metrics.physical_view_inset_top;
    event.physical_view_inset_right = metrics.physical_view_inset_right;
    event.physical_view_inset_bottom = metrics.physical_view_inset_bottom;
    event.physical_view_inset_left = metrics.physical_view_inset_left;
    event.display_id = metrics.display_id;
    event.has_constraints =
        (metrics.physical_min_width_constraint != metrics.physical_width ||
         metrics.physical_max_width_constraint != metrics.physical_width ||
         metrics.physical_min_height_constraint != metrics.physical_height ||
         metrics.physical_max_height_constraint != metrics.physical_height);
    event.min_width_constraint =
        static_cast<size_t>(metrics.physical_min_width_constraint);
    event.max_width_constraint =
        static_cast<size_t>(metrics.physical_max_width_constraint);
    event.min_height_constraint =
        static_cast<size_t>(metrics.physical_min_height_constraint);
    event.max_height_constraint =
        static_cast<size_t>(metrics.physical_max_height_constraint);
    event.display_features_count =
        metrics.physical_display_features_type.size();
    event.display_features_bounds =
        metrics.physical_display_features_bounds.empty()
            ? nullptr
            : metrics.physical_display_features_bounds.data();
    event.display_features_type =
        metrics.physical_display_features_type.empty()
            ? nullptr
            : metrics.physical_display_features_type.data();
    event.display_features_state =
        metrics.physical_display_features_state.empty()
            ? nullptr
            : metrics.physical_display_features_state.data();
    event.physical_padding_top = metrics.physical_padding_top;
    event.physical_padding_right = metrics.physical_padding_right;
    event.physical_padding_bottom = metrics.physical_padding_bottom;
    event.physical_padding_left = metrics.physical_padding_left;
    event.physical_system_gesture_inset_top =
        metrics.physical_system_gesture_inset_top;
    event.physical_system_gesture_inset_right =
        metrics.physical_system_gesture_inset_right;
    event.physical_system_gesture_inset_bottom =
        metrics.physical_system_gesture_inset_bottom;
    event.physical_system_gesture_inset_left =
        metrics.physical_system_gesture_inset_left;
    event.physical_touch_slop = metrics.physical_touch_slop;
    event.physical_display_corner_radius_top_left =
        metrics.physical_display_corner_radius_top_left;
    event.physical_display_corner_radius_top_right =
        metrics.physical_display_corner_radius_top_right;
    event.physical_display_corner_radius_bottom_right =
        metrics.physical_display_corner_radius_bottom_right;
    event.physical_display_corner_radius_bottom_left =
        metrics.physical_display_corner_radius_bottom_left;

    SendWindowMetricsEvent(&event);
    return;
  }

  TRACE_EVENT1("flutter", "PlatformViewAndroid::SetViewportMetrics", "path",
               "legacy");
  if (platform_view_) {
    platform_view_->SetViewportMetrics(view_id, metrics);
  }
}

FlutterEngineResult PlatformViewAndroid::SendWindowMetricsEvent(
    const FlutterWindowMetricsEvent* event) {
  if (event == nullptr) {
    return kInvalidArguments;
  }

  flutter::ViewportMetrics metrics;
  metrics.physical_width = ANDROID_SAFE_ACCESS(event, width, 0.0);
  metrics.physical_height = ANDROID_SAFE_ACCESS(event, height, 0.0);

  if (ANDROID_SAFE_ACCESS(event, has_constraints, false)) {
    metrics.physical_min_width_constraint = ANDROID_SAFE_ACCESS(
        event, min_width_constraint, metrics.physical_width);
    metrics.physical_max_width_constraint = ANDROID_SAFE_ACCESS(
        event, max_width_constraint, metrics.physical_width);
    metrics.physical_min_height_constraint = ANDROID_SAFE_ACCESS(
        event, min_height_constraint, metrics.physical_height);
    metrics.physical_max_height_constraint = ANDROID_SAFE_ACCESS(
        event, max_height_constraint, metrics.physical_height);
  } else {
    metrics.physical_min_width_constraint = metrics.physical_width;
    metrics.physical_max_width_constraint = metrics.physical_width;
    metrics.physical_min_height_constraint = metrics.physical_height;
    metrics.physical_max_height_constraint = metrics.physical_height;
  }

  if (metrics.physical_width < metrics.physical_min_width_constraint ||
      metrics.physical_width > metrics.physical_max_width_constraint ||
      metrics.physical_height < metrics.physical_min_height_constraint ||
      metrics.physical_height > metrics.physical_max_height_constraint) {
    return kInvalidArguments;
  }

  metrics.device_pixel_ratio = ANDROID_SAFE_ACCESS(event, pixel_ratio, 1.0);
  if (metrics.device_pixel_ratio <= 0.0) {
    return kInvalidArguments;
  }

  metrics.physical_view_inset_top =
      ANDROID_SAFE_ACCESS(event, physical_view_inset_top, 0.0);
  metrics.physical_view_inset_right =
      ANDROID_SAFE_ACCESS(event, physical_view_inset_right, 0.0);
  metrics.physical_view_inset_bottom =
      ANDROID_SAFE_ACCESS(event, physical_view_inset_bottom, 0.0);
  metrics.physical_view_inset_left =
      ANDROID_SAFE_ACCESS(event, physical_view_inset_left, 0.0);

  if (metrics.physical_view_inset_top < 0 ||
      metrics.physical_view_inset_right < 0 ||
      metrics.physical_view_inset_bottom < 0 ||
      metrics.physical_view_inset_left < 0) {
    return kInvalidArguments;
  }

  metrics.display_id = ANDROID_SAFE_ACCESS(event, display_id, 0);

  metrics.physical_padding_top =
      ANDROID_SAFE_ACCESS(event, physical_padding_top, 0.0);
  metrics.physical_padding_right =
      ANDROID_SAFE_ACCESS(event, physical_padding_right, 0.0);
  metrics.physical_padding_bottom =
      ANDROID_SAFE_ACCESS(event, physical_padding_bottom, 0.0);
  metrics.physical_padding_left =
      ANDROID_SAFE_ACCESS(event, physical_padding_left, 0.0);

  if (metrics.physical_padding_top < 0 || metrics.physical_padding_right < 0 ||
      metrics.physical_padding_bottom < 0 ||
      metrics.physical_padding_left < 0) {
    return kInvalidArguments;
  }

  metrics.physical_system_gesture_inset_top =
      ANDROID_SAFE_ACCESS(event, physical_system_gesture_inset_top, 0.0);
  metrics.physical_system_gesture_inset_right =
      ANDROID_SAFE_ACCESS(event, physical_system_gesture_inset_right, 0.0);
  metrics.physical_system_gesture_inset_bottom =
      ANDROID_SAFE_ACCESS(event, physical_system_gesture_inset_bottom, 0.0);
  metrics.physical_system_gesture_inset_left =
      ANDROID_SAFE_ACCESS(event, physical_system_gesture_inset_left, 0.0);

  metrics.physical_touch_slop =
      ANDROID_SAFE_ACCESS(event, physical_touch_slop, -1.0);

  metrics.physical_display_corner_radius_top_left =
      ANDROID_SAFE_ACCESS(event, physical_display_corner_radius_top_left, -1.0);
  metrics.physical_display_corner_radius_top_right = ANDROID_SAFE_ACCESS(
      event, physical_display_corner_radius_top_right, -1.0);
  metrics.physical_display_corner_radius_bottom_right = ANDROID_SAFE_ACCESS(
      event, physical_display_corner_radius_bottom_right, -1.0);
  metrics.physical_display_corner_radius_bottom_left = ANDROID_SAFE_ACCESS(
      event, physical_display_corner_radius_bottom_left, -1.0);

  size_t display_features_count =
      ANDROID_SAFE_ACCESS(event, display_features_count, 0);
  const double* display_features_bounds =
      ANDROID_SAFE_ACCESS(event, display_features_bounds, nullptr);
  const int* display_features_type =
      ANDROID_SAFE_ACCESS(event, display_features_type, nullptr);
  const int* display_features_state =
      ANDROID_SAFE_ACCESS(event, display_features_state, nullptr);

  constexpr size_t kBoundsPerDisplayFeature = 4;
  if (display_features_count > 0) {
    if (display_features_count > SIZE_MAX / kBoundsPerDisplayFeature) {
      return kInvalidArguments;
    }
    if (display_features_bounds == nullptr ||
        display_features_type == nullptr || display_features_state == nullptr) {
      return kInvalidArguments;
    }
    metrics.physical_display_features_bounds.assign(
        display_features_bounds,
        display_features_bounds +
            (display_features_count * kBoundsPerDisplayFeature));
    metrics.physical_display_features_type.assign(
        display_features_type, display_features_type + display_features_count);
    metrics.physical_display_features_state.assign(
        display_features_state,
        display_features_state + display_features_count);
  }

  int64_t view_id = ANDROID_SAFE_ACCESS(event, view_id, kImplicitViewId);
  if (platform_view_) {
    platform_view_->SetViewportMetrics(view_id, metrics);
  }
  return kSuccess;
}

// |PlatformView|
void PlatformViewAndroid::HandlePlatformMessage(
    std::unique_ptr<flutter::PlatformMessage> message) {
  if (platform_view_) {
    platform_view_->HandlePlatformMessage(std::move(message));
    return;
  }
  // Called from the ui thread.
  platform_message_handler_->HandlePlatformMessage(std::move(message));
}

// |PlatformView|
void PlatformViewAndroid::OnPreEngineRestart() const {
  if (platform_view_) {
    platform_view_->OnPreEngineRestart();
    return;
  }
  jni_facade_->FlutterViewOnPreEngineRestart();
}

void PlatformViewAndroid::DispatchSemanticsAction(JNIEnv* env,
                                                  jint node_id,
                                                  jint action,
                                                  jobject args,
                                                  jint args_position) {
  if (android_embedder_api_) {
    TRACE_EVENT1("flutter", "PlatformViewAndroid::DispatchSemanticsAction",
                 "path", "embedder_api");
    const uint8_t* args_data = nullptr;
    size_t args_size = 0;
    if (args != nullptr && !env->IsSameObject(args, NULL)) {
      args_data =
          static_cast<const uint8_t*>(env->GetDirectBufferAddress(args));
      args_size = static_cast<size_t>(args_position);
    }
    DispatchSemanticsAction(node_id,
                            static_cast<FlutterSemanticsAction>(action),
                            args_data, args_size);
    return;
  }

  TRACE_EVENT1("flutter", "PlatformViewAndroid::DispatchSemanticsAction",
               "path", "legacy");
  if (!platform_view_) {
    return;
  }
  // TODO(team-android): Remove implicit view assumption.
  // https://github.com/flutter/flutter/issues/142845
  if (env->IsSameObject(args, NULL)) {
    platform_view_->DispatchSemanticsAction(
        kImplicitViewId, node_id, static_cast<flutter::SemanticsAction>(action),
        fml::MallocMapping());
    return;
  }

  uint8_t* args_data = static_cast<uint8_t*>(env->GetDirectBufferAddress(args));
  auto args_vector = fml::MallocMapping::Copy(args_data, args_position);

  platform_view_->DispatchSemanticsAction(
      kImplicitViewId, node_id, static_cast<flutter::SemanticsAction>(action),
      std::move(args_vector));
}

FlutterEngineResult PlatformViewAndroid::DispatchSemanticsAction(
    uint64_t node_id,
    FlutterSemanticsAction action,
    const uint8_t* data,
    size_t data_length) {
  if (data == nullptr && data_length > 0) {
    return kInvalidArguments;
  }

  auto args_vector = (data != nullptr && data_length > 0)
                         ? fml::MallocMapping::Copy(data, data_length)
                         : fml::MallocMapping();

  if (platform_view_) {
    platform_view_->DispatchSemanticsAction(
        kImplicitViewId, node_id, static_cast<flutter::SemanticsAction>(action),
        std::move(args_vector));
  }
  return kSuccess;
}

void PlatformViewAndroid::SetSemanticsEnabled(bool enabled) {
  if (android_embedder_api_) {
    TRACE_EVENT1("flutter", "PlatformViewAndroid::SetSemanticsEnabled", "path",
                 "embedder_api");
    UpdateSemanticsEnabled(enabled);
    return;
  }

  TRACE_EVENT1("flutter", "PlatformViewAndroid::SetSemanticsEnabled", "path",
               "legacy");
  if (platform_view_) {
    platform_view_->SetSemanticsEnabled(enabled);
  }
}

FlutterEngineResult PlatformViewAndroid::UpdateSemanticsEnabled(bool enabled) {
  if (platform_view_) {
    platform_view_->SetSemanticsEnabled(enabled);
  }
  return kSuccess;
}

void PlatformViewAndroid::SetAccessibilityFeatures(int32_t flags) {
  if (platform_view_) {
    platform_view_->SetAccessibilityFeatures(flags);
  }
}

void PlatformViewAndroid::RegisterTexture(
    std::shared_ptr<flutter::Texture> texture) {
  if (platform_view_) {
    platform_view_->RegisterTexture(std::move(texture));
  }
}

void PlatformViewAndroid::UnregisterTexture(int64_t texture_id) {
  if (external_texture_adapter_) {
    external_texture_adapter_->UnregisterExternalTexture(texture_id);
    return;
  }
  if (platform_view_) {
    platform_view_->UnregisterTexture(texture_id);
  }
}

void PlatformViewAndroid::MarkTextureFrameAvailable(int64_t texture_id) {
  if (external_texture_adapter_) {
    external_texture_adapter_->MarkExternalTextureFrameAvailable(texture_id);
    return;
  }
  if (platform_view_) {
    platform_view_->MarkTextureFrameAvailable(texture_id);
  }
}

// |AndroidExternalTextureAdapter::Delegate|
void PlatformViewAndroid::OnRegisterTexture(
    std::shared_ptr<flutter::Texture> texture) {
  RegisterTexture(std::move(texture));
}

// |AndroidExternalTextureAdapter::Delegate|
void PlatformViewAndroid::OnUnregisterTexture(int64_t texture_id) {
  if (platform_view_) {
    platform_view_->UnregisterTexture(texture_id);
  }
}

// |AndroidExternalTextureAdapter::Delegate|
void PlatformViewAndroid::OnMarkTextureFrameAvailable(int64_t texture_id) {
  if (platform_view_) {
    platform_view_->MarkTextureFrameAvailable(texture_id);
  }
}

void PlatformViewAndroid::ScheduleFrame() {
  if (platform_view_) {
    platform_view_->ScheduleFrame();
  }
}

void PlatformViewAndroid::UpdateSemanticsLegacy(
    flutter::SemanticsNodeUpdates update,
    flutter::CustomAccessibilityActionUpdates actions) {
  platform_view_android_delegate_.UpdateSemantics(update, actions);
}

// |PlatformView|
void PlatformViewAndroid::UpdateSemantics(
    int64_t view_id,
    flutter::SemanticsNodeUpdates update,
    flutter::CustomAccessibilityActionUpdates actions) {
  if (platform_view_) {
    platform_view_->UpdateSemantics(view_id, std::move(update),
                                    std::move(actions));
    return;
  }
  UpdateSemanticsLegacy(std::move(update), std::move(actions));
}

// |PlatformView|
void PlatformViewAndroid::SetApplicationLocale(std::string locale) {
  if (platform_view_) {
    platform_view_->SetApplicationLocale(locale);
    return;
  }
  jni_facade_->FlutterViewSetApplicationLocale(std::move(locale));
}

// |PlatformView|
void PlatformViewAndroid::SetSemanticsTreeEnabled(bool enabled) {
  if (platform_view_) {
    platform_view_->SetSemanticsTreeEnabled(enabled);
  }
  jni_facade_->FlutterViewSetSemanticsTreeEnabled(enabled);
}

void PlatformViewAndroid::RegisterExternalTexture(
    int64_t texture_id,
    const fml::jni::ScopedJavaGlobalRef<jobject>& surface_texture) {
  if (external_texture_adapter_) {
    external_texture_adapter_->RegisterSurfaceTexture(texture_id,
                                                      surface_texture);
  }
}

void PlatformViewAndroid::RegisterImageTexture(
    int64_t texture_id,
    const fml::jni::ScopedJavaGlobalRef<jobject>& image_texture_entry,
    ImageExternalTexture::ImageLifecycle lifecycle) {
  if (external_texture_adapter_) {
    external_texture_adapter_->RegisterImageTexture(
        texture_id, image_texture_entry, lifecycle);
  }
}

// |PlatformView|
std::unique_ptr<VsyncWaiter> PlatformViewAndroid::CreateVSyncWaiter() {
  return std::make_unique<VsyncWaiterAndroid>(task_runners_);
}

namespace {
class PlatformViewProtectedAccessor : public PlatformView {
 public:
  using PlatformView::CreateRenderingSurface;
};
}  // namespace

std::unique_ptr<Surface> PlatformViewAndroid::CreateGPUSurface() {
  if (!android_surface_) {
    return nullptr;
  }
  return android_surface_->CreateGPUSurface(
      android_context_->GetMainSkiaContext().get());
}

// |PlatformView|
std::unique_ptr<Surface> PlatformViewAndroid::CreateRenderingSurface() {
  if (platform_view_) {
    return static_cast<PlatformViewProtectedAccessor*>(platform_view_)
        ->CreateRenderingSurface();
  }
  return CreateGPUSurface();
}

// |PlatformView|
std::shared_ptr<ExternalViewEmbedder>
PlatformViewAndroid::CreateExternalViewEmbedder() {
  if (platform_view_) {
    auto embedder = platform_view_->CreateExternalViewEmbedder();
    if (embedder) {
      return embedder;
    }
  }
  auto view_embedder = std::make_shared<AndroidExternalViewEmbedderWrapper>(
      android_meets_hcpp_criteria_, *android_context_, jni_facade_,
      surface_factory_, task_runners_);
  compositor_adapter_ =
      std::make_shared<AndroidCompositorAdapter>(std::move(view_embedder));
  return compositor_adapter_;
}

// |PlatformView|
std::unique_ptr<SnapshotSurfaceProducer>
PlatformViewAndroid::CreateSnapshotSurfaceProducer() {
  if (platform_view_) {
    auto producer = platform_view_->CreateSnapshotSurfaceProducer();
    if (producer) {
      return producer;
    }
  }
  if (!android_surface_) {
    return nullptr;
  }
  return std::make_unique<AndroidSnapshotSurfaceProducer>(*android_surface_);
}

sk_sp<GrDirectContext> PlatformViewAndroid::CreateResourceContextLegacy()
    const {
  if (!android_surface_) {
    return nullptr;
  }
#if !SLIMPELLER
  sk_sp<GrDirectContext> resource_context;
  if (android_surface_->ResourceContextMakeCurrent()) {
    // TODO(chinmaygarde): Currently, this code depends on the fact that only
    // the OpenGL surface will be able to make a resource context current. If
    // this changes, this assumption breaks. Handle the same.
    resource_context = ShellIOManager::CreateCompatibleResourceLoadingContext(
        GrBackendApi::kOpenGL,
        GPUSurfaceGLDelegate::GetDefaultPlatformGLInterface());
  } else {
    FML_DLOG(ERROR) << "Could not make the resource context current.";
  }
  return resource_context;
#else
  android_surface_->ResourceContextMakeCurrent();
  return nullptr;
#endif  //  !SLIMPELLER
}

// |PlatformView|
sk_sp<GrDirectContext> PlatformViewAndroid::CreateResourceContext() const {
  if (platform_view_) {
    return platform_view_->CreateResourceContext();
  }
  return CreateResourceContextLegacy();
}

void PlatformViewAndroid::ReleaseResourceContextLegacy() const {
  if (android_surface_) {
    android_surface_->ResourceContextClearCurrent();
  }
}

// |PlatformView|
void PlatformViewAndroid::ReleaseResourceContext() const {
  if (platform_view_) {
    platform_view_->ReleaseResourceContext();
    return;
  }
  ReleaseResourceContextLegacy();
}

std::shared_ptr<impeller::Context>
PlatformViewAndroid::GetImpellerContextLegacy() const {
  if (android_surface_) {
    return android_surface_->GetImpellerContext();
  }
  return android_context_->GetImpellerContext();
}

// |PlatformView|
std::shared_ptr<impeller::Context> PlatformViewAndroid::GetImpellerContext()
    const {
  if (platform_view_) {
    return platform_view_->GetImpellerContext();
  }
  return GetImpellerContextLegacy();
}

// |PlatformView|
std::unique_ptr<std::vector<std::string>>
PlatformViewAndroid::ComputePlatformResolvedLocales(
    const std::vector<std::string>& supported_locale_data) {
  if (platform_view_) {
    return platform_view_->ComputePlatformResolvedLocales(
        supported_locale_data);
  }
  return jni_facade_->FlutterViewComputePlatformResolvedLocale(
      supported_locale_data);
}

// |PlatformView|
void PlatformViewAndroid::RequestDartDeferredLibrary(intptr_t loading_unit_id) {
  if (platform_view_) {
    platform_view_->RequestDartDeferredLibrary(loading_unit_id);
    return;
  }
  if (jni_facade_->RequestDartDeferredLibrary(loading_unit_id)) {
    return;
  }
  return;  // TODO(garyq): Call LoadDartDeferredLibraryFailure()
}

// |PlatformView|
void PlatformViewAndroid::LoadDartDeferredLibrary(
    intptr_t loading_unit_id,
    std::unique_ptr<const fml::Mapping> snapshot_data,
    std::unique_ptr<const fml::Mapping> snapshot_instructions) {
  if (platform_view_) {
    platform_view_->LoadDartDeferredLibrary(loading_unit_id,
                                            std::move(snapshot_data),
                                            std::move(snapshot_instructions));
  }
}

// |PlatformView|
void PlatformViewAndroid::LoadDartDeferredLibraryError(
    intptr_t loading_unit_id,
    const std::string error_message,
    bool transient) {
  if (platform_view_) {
    platform_view_->LoadDartDeferredLibraryError(loading_unit_id, error_message,
                                                 transient);
  }
}

// |PlatformView|
void PlatformViewAndroid::UpdateAssetResolverByType(
    std::unique_ptr<AssetResolver> updated_asset_resolver,
    AssetResolver::AssetResolverType type) {
  if (platform_view_) {
    platform_view_->UpdateAssetResolverByType(std::move(updated_asset_resolver),
                                              type);
  }
}

void PlatformViewAndroid::InstallFirstFrameCallback() {
  if (platform_view_) {
    platform_view_->SetNextFrameCallback(
        [platform_view = GetWeakPtr(),
         platform_task_runner = task_runners_.GetPlatformTaskRunner()]() {
          // On GPU Task Runner.
          platform_task_runner->PostTask([platform_view]() {
            // Back on Platform Task Runner.
            if (platform_view) {
              platform_view->FireFirstFrameCallback();
            }
          });
        });
  }
}

void PlatformViewAndroid::FireFirstFrameCallback() {
  jni_facade_->FlutterViewOnFirstFrame();
}

double PlatformViewAndroid::GetScaledFontSize(double unscaled_font_size,
                                              int configuration_id) const {
  if (platform_view_) {
    return platform_view_->GetScaledFontSize(unscaled_font_size,
                                             configuration_id);
  }
  return jni_facade_->FlutterViewGetScaledFontSize(unscaled_font_size,
                                                   configuration_id);
}

bool PlatformViewAndroid::IsSurfaceControlEnabled() const {
  // This needs to know if we're actually using HCPP.
  return android_meets_hcpp_criteria_ &&
         android_context_->RenderingApi() ==
             AndroidRenderingAPI::kImpellerVulkan &&
         impeller::ContextVK::Cast(*android_context_->GetImpellerContext())
             .GetShouldEnableSurfaceControlSwapchain();
}

void PlatformViewAndroid::SetupImpellerContext() {
  if (platform_view_) {
    platform_view_->SetupImpellerContext();
  }
  android_context_->SetupImpellerContext();
  android_surface_->SetupImpellerSurface();
}

AndroidSurface::ScreenshotResult PlatformViewAndroid::Screenshot() {
  if (!android_surface_) {
    return {};
  }
  AndroidSurface::ScreenshotResult screenshot;
  fml::AutoResetWaitableEvent latch;
  fml::TaskRunner::RunNowOrPostTask(
      task_runners_.GetRasterTaskRunner(),
      [&latch, surface = android_surface_.get(), &screenshot]() {
        screenshot = surface->Screenshot();
        latch.Signal();
      });
  latch.Wait();
  return screenshot;
}

}  // namespace flutter
