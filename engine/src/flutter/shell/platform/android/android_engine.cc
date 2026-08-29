// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include "flutter/shell/platform/android/android_engine.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

#include "flutter/display_list/geometry/dl_geometry_types.h"
#include "flutter/display_list/geometry/dl_path.h"
#include "flutter/display_list/geometry/dl_path_builder.h"
#include "flutter/flow/embedded_views.h"
#include "flutter/fml/logging.h"
#include "flutter/shell/platform/android/android_mutators_mapper.h"

#if FML_OS_ANDROID
#include "flutter/fml/platform/android/jni_util.h"
#include "flutter/fml/platform/android/scoped_java_ref.h"
#endif

namespace flutter {

namespace {

constexpr size_t kPointerDataFieldCount = 36;
constexpr size_t kBytesPerField = 8;
constexpr size_t kBytesPerPointerEntry =
    kPointerDataFieldCount * kBytesPerField;

constexpr size_t kBytesPerNode = 73 * sizeof(int32_t);
constexpr size_t kBytesPerChild = sizeof(int32_t);
constexpr size_t kBytesPerCustomAction = sizeof(int32_t);
constexpr size_t kBytesPerAction = 4 * sizeof(int32_t);
constexpr size_t kBytesPerStringAttribute = 4 * sizeof(int32_t);
constexpr int32_t kEmptyStringIndex = -1;

struct MessageResponseContext {
  std::shared_ptr<PlatformViewAndroidJNI> jni;
  int32_t response_id = 0;
};

void OnPlatformMessageDataResponse(const uint8_t* data,
                                   size_t data_size,
                                   void* user_data) {
  auto* context = static_cast<MessageResponseContext*>(user_data);
  if (context != nullptr) {
    if (context->jni != nullptr) {
      std::unique_ptr<fml::Mapping> mapping;
      if (data != nullptr && data_size > 0) {
        mapping = std::make_unique<fml::MallocMapping>(
            fml::MallocMapping::Copy(data, data + data_size));
      }
      context->jni->FlutterViewHandlePlatformMessageResponse(
          context->response_id, std::move(mapping));
    }
    delete context;
  }
}

void PutStringAttributesIntoBuffer(
    size_t attribute_count,
    const FlutterStringAttribute** attributes,
    int32_t* buffer,
    size_t* position,
    std::vector<std::vector<uint8_t>>& string_attribute_args) {
  if (attributes == nullptr || attribute_count == 0) {
    buffer[(*position)++] = kEmptyStringIndex;
    return;
  }
  buffer[(*position)++] = static_cast<int32_t>(attribute_count);
  for (size_t i = 0; i < attribute_count; ++i) {
    const auto* attribute = attributes[i];
    if (attribute == nullptr) {
      continue;
    }
    buffer[(*position)++] = static_cast<int32_t>(attribute->start);
    buffer[(*position)++] = static_cast<int32_t>(attribute->end);
    buffer[(*position)++] = static_cast<int32_t>(attribute->type);
    switch (attribute->type) {
      case kSpellOut:
        buffer[(*position)++] = kEmptyStringIndex;
        break;
      case kLocale:
        if (attribute->locale != nullptr &&
            attribute->locale->locale != nullptr) {
          std::string locale_str(attribute->locale->locale);
          buffer[(*position)++] =
              static_cast<int32_t>(string_attribute_args.size());
          string_attribute_args.push_back(
              {locale_str.begin(), locale_str.end()});
        } else {
          buffer[(*position)++] = kEmptyStringIndex;
        }
        break;
    }
  }
}

void PutCStringIntoBuffer(const char* str,
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

int64_t FlagsToInt64(const FlutterSemanticsNode2* node) {
  if (node == nullptr) {
    return 0;
  }
  if (node->flags2 != nullptr) {
    int64_t result = 0;
    const auto& f = *node->flags2;
    if (f.is_checked != kFlutterCheckStateNone) {
      result |= (INT64_C(1) << 0);
    }
    if (f.is_checked == kFlutterCheckStateTrue) {
      result |= (INT64_C(1) << 1);
    }
    if (f.is_selected == kFlutterTristateTrue) {
      result |= (INT64_C(1) << 2);
    }
    if (f.is_button) {
      result |= (INT64_C(1) << 3);
    }
    if (f.is_text_field) {
      result |= (INT64_C(1) << 4);
    }
    if (f.is_focused == kFlutterTristateTrue) {
      result |= (INT64_C(1) << 5);
    }
    if (f.is_enabled != kFlutterTristateNone) {
      result |= (INT64_C(1) << 6);
    }
    if (f.is_enabled == kFlutterTristateTrue) {
      result |= (INT64_C(1) << 7);
    }
    if (f.is_in_mutually_exclusive_group) {
      result |= (INT64_C(1) << 8);
    }
    if (f.is_header) {
      result |= (INT64_C(1) << 9);
    }
    if (f.is_obscured) {
      result |= (INT64_C(1) << 10);
    }
    if (f.scopes_route) {
      result |= (INT64_C(1) << 11);
    }
    if (f.names_route) {
      result |= (INT64_C(1) << 12);
    }
    if (f.is_hidden) {
      result |= (INT64_C(1) << 13);
    }
    if (f.is_image) {
      result |= (INT64_C(1) << 14);
    }
    if (f.is_live_region) {
      result |= (INT64_C(1) << 15);
    }
    if (f.is_toggled != kFlutterTristateNone) {
      result |= (INT64_C(1) << 16);
    }
    if (f.is_toggled == kFlutterTristateTrue) {
      result |= (INT64_C(1) << 17);
    }
    if (f.has_implicit_scrolling) {
      result |= (INT64_C(1) << 18);
    }
    if (f.is_multiline) {
      result |= (INT64_C(1) << 19);
    }
    if (f.is_read_only) {
      result |= (INT64_C(1) << 20);
    }
    if (f.is_focused != kFlutterTristateNone) {
      result |= (INT64_C(1) << 21);
    }
    if (f.is_link) {
      result |= (INT64_C(1) << 22);
    }
    if (f.is_slider) {
      result |= (INT64_C(1) << 23);
    }
    if (f.is_keyboard_key) {
      result |= (INT64_C(1) << 24);
    }
    if (f.is_checked == kFlutterCheckStateMixed) {
      result |= (INT64_C(1) << 25);
    }
    if (f.is_expanded != kFlutterTristateNone) {
      result |= (INT64_C(1) << 26);
    }
    if (f.is_expanded == kFlutterTristateTrue) {
      result |= (INT64_C(1) << 27);
    }
    if (f.is_selected != kFlutterTristateNone) {
      result |= (INT64_C(1) << 28);
    }
    if (f.is_required != kFlutterTristateNone) {
      result |= (INT64_C(1) << 29);
    }
    if (f.is_required == kFlutterTristateTrue) {
      result |= (INT64_C(1) << 30);
    }
    if (f.is_accessibility_focus_blocked) {
      result |= (INT64_C(1) << 31);
    }
    return result;
  }
  return static_cast<int64_t>(node->flags__deprecated__);
}

void TransformationToColMajor(const FlutterTransformation& t, float* out) {
  out[0] = static_cast<float>(t.scaleX);
  out[1] = static_cast<float>(t.skewY);
  out[2] = 0.0f;
  out[3] = static_cast<float>(t.pers0);

  out[4] = static_cast<float>(t.skewX);
  out[5] = static_cast<float>(t.scaleY);
  out[6] = 0.0f;
  out[7] = static_cast<float>(t.pers1);

  out[8] = 0.0f;
  out[9] = 0.0f;
  out[10] = 1.0f;
  out[11] = 0.0f;

  out[12] = static_cast<float>(t.transX);
  out[13] = static_cast<float>(t.transY);
  out[14] = 0.0f;
  out[15] = static_cast<float>(t.pers2);
}

}  // namespace

class AndroidEngine::CompositorDelegate
    : public AndroidCompositorPlatformViewDelegate {
 public:
  explicit CompositorDelegate(AndroidEngine* engine) : engine_(engine) {}
  ~CompositorDelegate() override = default;

  void DetachEngine() { engine_ = nullptr; }

  void OnPlatformViewPresented(
      int64_t view_id,
      const FlutterPoint& offset,
      const FlutterSize& size,
      size_t mutations_count,
      const FlutterPlatformViewMutation** mutations) override {
    if (engine_ != nullptr) {
      engine_->OnPlatformViewPresented(view_id, offset, size, mutations_count,
                                       mutations);
    }
  }

  void OnFramePresented() override {
    if (engine_ != nullptr) {
      engine_->OnFramePresented();
    }
  }

 private:
  AndroidEngine* engine_;
};

AndroidEngine::AndroidEngine(const flutter::Settings& settings,
                             std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
                             AndroidRenderingAPI android_rendering_api)
    : settings_(settings),
      jni_facade_(std::move(jni_facade)),
      android_rendering_api_(android_rendering_api) {
  task_runners_ = std::make_shared<AndroidTaskRunners>("io.flutter");
  surface_manager_ =
      std::make_shared<AndroidSurfaceManager>(android_rendering_api_);
  compositor_delegate_ = std::make_shared<CompositorDelegate>(this);
  compositor_ = std::make_shared<AndroidCompositor>(surface_manager_,
                                                    compositor_delegate_);
}

AndroidEngine::AndroidEngine(const flutter::Settings& settings,
                             std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
                             std::shared_ptr<AndroidTaskRunners> task_runners,
                             AndroidRenderingAPI android_rendering_api)
    : settings_(settings),
      jni_facade_(std::move(jni_facade)),
      android_rendering_api_(android_rendering_api),
      task_runners_(std::move(task_runners)) {
  surface_manager_ =
      std::make_shared<AndroidSurfaceManager>(android_rendering_api_);
  compositor_delegate_ = std::make_shared<CompositorDelegate>(this);
  compositor_ = std::make_shared<AndroidCompositor>(surface_manager_,
                                                    compositor_delegate_);
}

AndroidEngine::~AndroidEngine() {
  if (compositor_delegate_ != nullptr) {
    compositor_delegate_->DetachEngine();
  }
  if (compositor_ != nullptr) {
    compositor_->SetPlatformViewDelegate(nullptr);
  }

  if (engine_ != nullptr) {
    if (surface_attached_) {
      FlutterEngineNotifyDestroyed(engine_);
      surface_attached_ = false;
    }
    FlutterEngineShutdown(engine_);
    engine_ = nullptr;
  }

  std::lock_guard<std::mutex> lock(pending_responses_mutex_);
  pending_responses_.clear();
}

bool AndroidEngine::IsValid() const {
  return is_valid_ && engine_ != nullptr;
}

FlutterPointerPhase AndroidEngine::ToFlutterPointerPhase(int64_t change) {
  switch (change) {
    case 0:
      return kCancel;
    case 1:
      return kAdd;
    case 2:
      return kRemove;
    case 3:
      return kHover;
    case 4:
      return kDown;
    case 5:
      return kMove;
    case 6:
      return kUp;
    case 7:
      return kPanZoomStart;
    case 8:
      return kPanZoomUpdate;
    case 9:
      return kPanZoomEnd;
    default:
      return kCancel;
  }
}

FlutterPointerDeviceKind AndroidEngine::ToFlutterPointerDeviceKind(
    int64_t kind) {
  switch (kind) {
    case 0:
      return kFlutterPointerDeviceKindTouch;
    case 1:
      return kFlutterPointerDeviceKindMouse;
    case 2:
      return kFlutterPointerDeviceKindStylus;
    case 3:
      return kFlutterPointerDeviceKindInvertedStylus;
    case 4:
      return kFlutterPointerDeviceKindTrackpad;
    default:
      return kFlutterPointerDeviceKindTouch;
  }
}

FlutterPointerSignalKind AndroidEngine::ToFlutterPointerSignalKind(
    int64_t signal_kind) {
  switch (signal_kind) {
    case 0:
      return kFlutterPointerSignalKindNone;
    case 1:
      return kFlutterPointerSignalKindScroll;
    case 2:
      return kFlutterPointerSignalKindScrollInertiaCancel;
    case 3:
      return kFlutterPointerSignalKindScale;
    default:
      return kFlutterPointerSignalKindNone;
  }
}

std::vector<FlutterPointerEvent> AndroidEngine::UnpackPointerDataPacket(
    const uint8_t* buffer,
    size_t position) {
  std::vector<FlutterPointerEvent> events;
  if (buffer == nullptr || position < kBytesPerPointerEntry ||
      position % kBytesPerPointerEntry != 0) {
    return events;
  }

  size_t count = position / kBytesPerPointerEntry;
  events.reserve(count);

  for (size_t i = 0; i < count; ++i) {
    const uint8_t* entry = buffer + i * kBytesPerPointerEntry;
    const int64_t* int_fields = reinterpret_cast<const int64_t*>(entry);
    const double* double_fields = reinterpret_cast<const double*>(entry);

    FlutterPointerEvent event = {};
    event.struct_size = sizeof(FlutterPointerEvent);
    event.timestamp = static_cast<size_t>(int_fields[1]);
    event.phase = ToFlutterPointerPhase(int_fields[2]);
    event.device_kind = ToFlutterPointerDeviceKind(int_fields[3]);
    event.signal_kind = ToFlutterPointerSignalKind(int_fields[4]);
    event.device = static_cast<int32_t>(int_fields[5]);

    event.x = double_fields[7];
    event.y = double_fields[8];
    event.buttons = int_fields[11];
    event.pressure = double_fields[14];
    event.pressure_min = double_fields[15];
    event.pressure_max = double_fields[16];
    event.scroll_delta_x = double_fields[27];
    event.scroll_delta_y = double_fields[28];
    event.pan_x = double_fields[29];
    event.pan_y = double_fields[30];
    event.scale = double_fields[33];
    event.rotation = double_fields[34];
    event.view_id = int_fields[35];

    events.push_back(event);
  }

  return events;
}

void AndroidEngine::SerializeSemanticsUpdate(
    const FlutterSemanticsUpdate2* update,
    std::vector<uint8_t>& buffer,
    std::vector<std::string>& strings,
    std::vector<std::vector<uint8_t>>& string_attribute_args,
    std::vector<uint8_t>& actions_buffer,
    std::vector<std::string>& action_strings) {
  if (update == nullptr) {
    return;
  }

  size_t num_bytes = 0;
  if (update->node_count > 0 && update->nodes != nullptr) {
    for (size_t i = 0; i < update->node_count; ++i) {
      const FlutterSemanticsNode2* node = update->nodes[i];
      if (node == nullptr) {
        continue;
      }
      num_bytes += kBytesPerNode;
      num_bytes += node->child_count * kBytesPerChild;
      num_bytes += node->child_count * kBytesPerChild;
      num_bytes +=
          node->custom_accessibility_actions_count * kBytesPerCustomAction;
      num_bytes += node->label_attribute_count * kBytesPerStringAttribute;
      num_bytes += node->value_attribute_count * kBytesPerStringAttribute;
      num_bytes +=
          node->increased_value_attribute_count * kBytesPerStringAttribute;
      num_bytes +=
          node->decreased_value_attribute_count * kBytesPerStringAttribute;
      num_bytes += node->hint_attribute_count * kBytesPerStringAttribute;
    }
  }

  buffer.resize(num_bytes);
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
      int64_t flags = FlagsToInt64(node);
      std::memcpy(&buffer_int32[position], &flags, sizeof(int64_t));
      position += 2;
      buffer_int32[position++] = static_cast<int32_t>(node->actions);
      buffer_int32[position++] = node->max_value_length;
      buffer_int32[position++] = node->current_value_length;
      buffer_int32[position++] = node->text_selection_base;
      buffer_int32[position++] = node->text_selection_extent;
      buffer_int32[position++] = static_cast<int32_t>(node->platform_view_id);
      buffer_int32[position++] = node->scroll_child_count;
      buffer_int32[position++] = node->scroll_index;
      buffer_int32[position++] = node->traversal_parent;
      buffer_float32[position++] = static_cast<float>(node->scroll_position);
      buffer_float32[position++] = static_cast<float>(node->scroll_extent_max);
      buffer_float32[position++] = static_cast<float>(node->scroll_extent_min);
      buffer_int32[position++] = static_cast<int32_t>(node->role);

      PutCStringIntoBuffer(node->identifier, buffer_int32, &position, strings);
      PutCStringIntoBuffer(node->label, buffer_int32, &position, strings);
      PutStringAttributesIntoBuffer(node->label_attribute_count,
                                    node->label_attributes, buffer_int32,
                                    &position, string_attribute_args);

      PutCStringIntoBuffer(node->value, buffer_int32, &position, strings);
      PutStringAttributesIntoBuffer(node->value_attribute_count,
                                    node->value_attributes, buffer_int32,
                                    &position, string_attribute_args);

      PutCStringIntoBuffer(node->increased_value, buffer_int32, &position,
                           strings);
      PutStringAttributesIntoBuffer(node->increased_value_attribute_count,
                                    node->increased_value_attributes,
                                    buffer_int32, &position,
                                    string_attribute_args);

      PutCStringIntoBuffer(node->decreased_value, buffer_int32, &position,
                           strings);
      PutStringAttributesIntoBuffer(node->decreased_value_attribute_count,
                                    node->decreased_value_attributes,
                                    buffer_int32, &position,
                                    string_attribute_args);

      PutCStringIntoBuffer(node->hint, buffer_int32, &position, strings);
      PutStringAttributesIntoBuffer(node->hint_attribute_count,
                                    node->hint_attributes, buffer_int32,
                                    &position, string_attribute_args);

      PutCStringIntoBuffer(node->tooltip, buffer_int32, &position, strings);
      PutCStringIntoBuffer(node->link_url, buffer_int32, &position, strings);
      PutCStringIntoBuffer(node->locale, buffer_int32, &position, strings);
      PutCStringIntoBuffer(node->min_value, buffer_int32, &position, strings);
      PutCStringIntoBuffer(node->max_value, buffer_int32, &position, strings);

      buffer_int32[position++] = node->heading_level;
      buffer_int32[position++] = static_cast<int32_t>(node->text_direction);
      buffer_float32[position++] = static_cast<float>(node->rect.left);
      buffer_float32[position++] = static_cast<float>(node->rect.top);
      buffer_float32[position++] = static_cast<float>(node->rect.right);
      buffer_float32[position++] = static_cast<float>(node->rect.bottom);

      TransformationToColMajor(node->transform, &buffer_float32[position]);
      position += 16;
      TransformationToColMajor(node->hit_test_transform,
                               &buffer_float32[position]);
      position += 16;

      buffer_int32[position++] = static_cast<int32_t>(node->child_count);
      for (size_t c = 0; c < node->child_count; ++c) {
        buffer_int32[position++] = node->children_in_traversal_order != nullptr
                                       ? node->children_in_traversal_order[c]
                                       : 0;
      }

      buffer_int32[position++] = static_cast<int32_t>(node->child_count);
      for (size_t c = 0; c < node->child_count; ++c) {
        buffer_int32[position++] = node->children_in_hit_test_order != nullptr
                                       ? node->children_in_hit_test_order[c]
                                       : 0;
      }

      buffer_int32[position++] =
          static_cast<int32_t>(node->custom_accessibility_actions_count);
      for (size_t a = 0; a < node->custom_accessibility_actions_count; ++a) {
        buffer_int32[position++] = node->custom_accessibility_actions != nullptr
                                       ? node->custom_accessibility_actions[a]
                                       : 0;
      }
    }
  }

  size_t num_action_bytes = update->custom_action_count * kBytesPerAction;
  actions_buffer.resize(num_action_bytes);
  if (!actions_buffer.empty() && update->custom_actions != nullptr) {
    int32_t* actions_buffer_int32 =
        reinterpret_cast<int32_t*>(actions_buffer.data());
    size_t actions_position = 0;
    for (size_t i = 0; i < update->custom_action_count; ++i) {
      const FlutterSemanticsCustomAction2* action = update->custom_actions[i];
      if (action == nullptr) {
        continue;
      }
      actions_buffer_int32[actions_position++] = action->id;
      actions_buffer_int32[actions_position++] =
          static_cast<int32_t>(action->override_action);
      PutCStringIntoBuffer(action->label, actions_buffer_int32,
                           &actions_position, action_strings);
      PutCStringIntoBuffer(action->hint, actions_buffer_int32,
                           &actions_position, action_strings);
    }
  }
}

bool AndroidEngine::Launch(std::unique_ptr<APKAssetProvider> apk_asset_provider,
                           const std::string& entrypoint,
                           const std::string& library_url,
                           const std::vector<std::string>& entrypoint_args,
                           int64_t engine_id) {
  if (engine_ != nullptr) {
    FML_LOG(ERROR) << "AndroidEngine has already been launched.";
    return false;
  }

  apk_asset_provider_ = std::move(apk_asset_provider);
  if (apk_asset_provider_ != nullptr) {
    asset_resolver_ = apk_asset_provider_->ToFlutterAssetResolver();
    asset_resolvers_array_[0] = &asset_resolver_;
  }

  // Populate Renderer Config
  std::memset(&renderer_config_, 0, sizeof(FlutterRendererConfig));
  switch (android_rendering_api_) {
#if !SLIMPELLER
    case AndroidRenderingAPI::kSoftware:
      renderer_config_.type = kSoftware;
      renderer_config_.software.struct_size =
          sizeof(FlutterSoftwareRendererConfig);
      renderer_config_.software.surface_present_callback =
          [](void* user_data, const void* allocation, size_t row_bytes,
             size_t height) -> bool {
        auto* engine = static_cast<AndroidEngine*>(user_data);
        return engine != nullptr && engine->GetSurfaceManager() != nullptr &&
               engine->GetSurfaceManager()->PresentSoftware(allocation,
                                                            row_bytes, height);
      };
      break;
    case AndroidRenderingAPI::kSkiaOpenGLES:
#endif  // !SLIMPELLER
    case AndroidRenderingAPI::kImpellerAutoselect:
    case AndroidRenderingAPI::kImpellerOpenGLES:
      renderer_config_.type = kOpenGL;
      renderer_config_.open_gl.struct_size =
          sizeof(FlutterOpenGLRendererConfig);
      renderer_config_.open_gl.make_current = [](void* user_data) -> bool {
        auto* engine = static_cast<AndroidEngine*>(user_data);
        return engine != nullptr && engine->GetSurfaceManager() != nullptr &&
               engine->GetSurfaceManager()->MakeCurrent();
      };
      renderer_config_.open_gl.clear_current = [](void* user_data) -> bool {
        auto* engine = static_cast<AndroidEngine*>(user_data);
        return engine != nullptr && engine->GetSurfaceManager() != nullptr &&
               engine->GetSurfaceManager()->ClearCurrent();
      };
      renderer_config_.open_gl.present = [](void* user_data) -> bool {
        auto* engine = static_cast<AndroidEngine*>(user_data);
        return engine != nullptr && engine->GetSurfaceManager() != nullptr &&
               engine->GetSurfaceManager()->Present();
      };
      renderer_config_.open_gl.fbo_callback = [](void* user_data) -> uint32_t {
        auto* engine = static_cast<AndroidEngine*>(user_data);
        return (engine != nullptr && engine->GetSurfaceManager() != nullptr)
                   ? engine->GetSurfaceManager()->GetFBO()
                   : 0;
      };
      renderer_config_.open_gl.make_resource_current =
          [](void* user_data) -> bool {
        auto* engine = static_cast<AndroidEngine*>(user_data);
        return engine != nullptr && engine->GetSurfaceManager() != nullptr &&
               engine->GetSurfaceManager()->MakeResourceCurrent();
      };
      renderer_config_.open_gl.gl_proc_resolver =
          [](void*, const char* name) -> void* {
#if FML_OS_ANDROID
        return reinterpret_cast<void*>(eglGetProcAddress(name));
#else
        return nullptr;
#endif
      };
      break;
    case AndroidRenderingAPI::kImpellerVulkan:
      renderer_config_.type = kVulkan;
      break;
  }

  // Populate Compositor Config
  compositor_->PopulateCompositorConfig(&embedder_compositor_);

  // Populate Project Args
  std::memset(&project_args_, 0, sizeof(FlutterProjectArgs));
  project_args_.struct_size = sizeof(FlutterProjectArgs);
  project_args_.custom_task_runners = &task_runners_->GetCustomTaskRunners();
  project_args_.compositor = &embedder_compositor_;
  if (apk_asset_provider_ != nullptr) {
    project_args_.asset_resolvers = asset_resolvers_array_;
    project_args_.asset_resolvers_count = 1;
  }

  project_args_.custom_dart_entrypoint =
      entrypoint.empty() ? nullptr : entrypoint.c_str();

  std::vector<const char*> entrypoint_argv_ptrs;
  entrypoint_argv_ptrs.reserve(entrypoint_args.size());
  for (const auto& arg : entrypoint_args) {
    entrypoint_argv_ptrs.push_back(arg.c_str());
  }
  project_args_.dart_entrypoint_argc = entrypoint_argv_ptrs.size();
  project_args_.dart_entrypoint_argv =
      entrypoint_argv_ptrs.empty() ? nullptr : entrypoint_argv_ptrs.data();

  project_args_.platform_message_callback =
      &AndroidEngine::OnPlatformMessageCallback;
  project_args_.update_semantics_callback2 =
      &AndroidEngine::OnUpdateSemantics2Callback;
  project_args_.dart_deferred_library_loader_callback =
      &AndroidEngine::OnDartDeferredLibraryRequestCallback;
  project_args_.raster_context_setup_callback =
      &AndroidEngine::OnRasterContextSetupCallback;
  project_args_.raster_context_teardown_callback =
      &AndroidEngine::OnRasterContextTeardownCallback;

  FlutterEngineResult result =
      FlutterEngineInitialize(FLUTTER_ENGINE_VERSION, &renderer_config_,
                              &project_args_, this, &engine_);
  if (result != kSuccess || engine_ == nullptr) {
    FML_LOG(ERROR) << "Failed to initialize FlutterEngine: " << result;
    return false;
  }

  task_runners_->SetEngine(engine_);

  result = FlutterEngineRunInitialized(engine_);
  if (result != kSuccess) {
    FML_LOG(ERROR) << "Failed to run initialized FlutterEngine: " << result;
    FlutterEngineShutdown(engine_);
    engine_ = nullptr;
    return false;
  }

  is_valid_ = true;
  engine_id_ = engine_id;

  if (surface_attached_) {
    FlutterEngineNotifyCreated(engine_);
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
  if (!IsValid()) {
    FML_LOG(ERROR) << "Cannot spawn from an invalid AndroidEngine.";
    return nullptr;
  }

  auto child_task_runners = std::make_shared<AndroidTaskRunners>(
      "io.flutter", task_runners_->GetPlatformTaskRunner(),
      task_runners_->GetUITaskRunner(), task_runners_->GetRasterTaskRunner());

  std::unique_ptr<AndroidEngine> child(
      new AndroidEngine(settings_, std::move(jni_facade),
                        std::move(child_task_runners), android_rendering_api_));

  std::memset(&child->renderer_config_, 0, sizeof(FlutterRendererConfig));
  switch (child->android_rendering_api_) {
#if !SLIMPELLER
    case AndroidRenderingAPI::kSoftware:
      child->renderer_config_.type = kSoftware;
      child->renderer_config_.software.struct_size =
          sizeof(FlutterSoftwareRendererConfig);
      child->renderer_config_.software.surface_present_callback =
          [](void* user_data, const void* allocation, size_t row_bytes,
             size_t height) -> bool {
        auto* engine = static_cast<AndroidEngine*>(user_data);
        return engine != nullptr && engine->GetSurfaceManager() != nullptr &&
               engine->GetSurfaceManager()->PresentSoftware(allocation,
                                                            row_bytes, height);
      };
      break;
    case AndroidRenderingAPI::kSkiaOpenGLES:
#endif  // !SLIMPELLER
    case AndroidRenderingAPI::kImpellerAutoselect:
    case AndroidRenderingAPI::kImpellerOpenGLES:
      child->renderer_config_.type = kOpenGL;
      child->renderer_config_.open_gl.struct_size =
          sizeof(FlutterOpenGLRendererConfig);
      child->renderer_config_.open_gl.make_current =
          [](void* user_data) -> bool {
        auto* engine = static_cast<AndroidEngine*>(user_data);
        return engine != nullptr && engine->GetSurfaceManager() != nullptr &&
               engine->GetSurfaceManager()->MakeCurrent();
      };
      child->renderer_config_.open_gl.clear_current =
          [](void* user_data) -> bool {
        auto* engine = static_cast<AndroidEngine*>(user_data);
        return engine != nullptr && engine->GetSurfaceManager() != nullptr &&
               engine->GetSurfaceManager()->ClearCurrent();
      };
      child->renderer_config_.open_gl.present = [](void* user_data) -> bool {
        auto* engine = static_cast<AndroidEngine*>(user_data);
        return engine != nullptr && engine->GetSurfaceManager() != nullptr &&
               engine->GetSurfaceManager()->Present();
      };
      child->renderer_config_.open_gl.fbo_callback =
          [](void* user_data) -> uint32_t {
        auto* engine = static_cast<AndroidEngine*>(user_data);
        return (engine != nullptr && engine->GetSurfaceManager() != nullptr)
                   ? engine->GetSurfaceManager()->GetFBO()
                   : 0;
      };
      child->renderer_config_.open_gl.make_resource_current =
          [](void* user_data) -> bool {
        auto* engine = static_cast<AndroidEngine*>(user_data);
        return engine != nullptr && engine->GetSurfaceManager() != nullptr &&
               engine->GetSurfaceManager()->MakeResourceCurrent();
      };
      child->renderer_config_.open_gl.gl_proc_resolver =
          [](void*, const char* name) -> void* {
#if FML_OS_ANDROID
        return reinterpret_cast<void*>(eglGetProcAddress(name));
#else
        return nullptr;
#endif
      };
      break;
    case AndroidRenderingAPI::kImpellerVulkan:
      child->renderer_config_.type = kVulkan;
      break;
  }

  // Populate Compositor Config for child engine
  child->compositor_->PopulateCompositorConfig(&child->embedder_compositor_);

  // Populate Project Args for child engine
  std::memset(&child->project_args_, 0, sizeof(FlutterProjectArgs));
  child->project_args_.struct_size = sizeof(FlutterProjectArgs);
  child->project_args_.custom_task_runners = nullptr;
  child->project_args_.compositor = &child->embedder_compositor_;
  if (apk_asset_provider_ != nullptr) {
    child->apk_asset_provider_ = apk_asset_provider_->Clone();
    child->asset_resolver_ =
        child->apk_asset_provider_->ToFlutterAssetResolver();
    child->asset_resolvers_array_[0] = &child->asset_resolver_;
    child->project_args_.asset_resolvers = child->asset_resolvers_array_;
    child->project_args_.asset_resolvers_count = 1;
  }
  child->project_args_.platform_message_callback =
      &AndroidEngine::OnPlatformMessageCallback;
  child->project_args_.update_semantics_callback2 =
      &AndroidEngine::OnUpdateSemantics2Callback;
  child->project_args_.dart_deferred_library_loader_callback =
      &AndroidEngine::OnDartDeferredLibraryRequestCallback;
  child->project_args_.raster_context_setup_callback =
      &AndroidEngine::OnRasterContextSetupCallback;
  child->project_args_.raster_context_teardown_callback =
      &AndroidEngine::OnRasterContextTeardownCallback;

  std::vector<const char*> argv_ptrs;
  argv_ptrs.reserve(entrypoint_args.size());
  for (const auto& arg : entrypoint_args) {
    argv_ptrs.push_back(arg.c_str());
  }

  FlutterEngineSpawnConfig config = {};
  config.struct_size = sizeof(FlutterEngineSpawnConfig);
  config.entrypoint = entrypoint.empty() ? nullptr : entrypoint.c_str();
  config.library_uri = library_url.empty() ? nullptr : library_url.c_str();
  config.initial_route =
      initial_route.empty() ? nullptr : initial_route.c_str();
  config.entrypoint_argc = argv_ptrs.size();
  config.entrypoint_argv = argv_ptrs.empty() ? nullptr : argv_ptrs.data();
  config.engine_id = engine_id;
  config.user_data = child.get();
  config.renderer_config = &child->renderer_config_;
  config.project_args = &child->project_args_;

  FlutterEngineResult result =
      FlutterEngineSpawn(engine_, &config, &child->engine_);
  if (result != kSuccess || child->engine_ == nullptr) {
    FML_LOG(ERROR) << "Failed to spawn child FlutterEngine: " << result;
    return nullptr;
  }

  child->task_runners_->SetEngine(child->engine_);
  child->is_valid_ = true;
  child->engine_id_ = engine_id;

  return child;
}

void AndroidEngine::NotifySurfaceCreated(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  if (surface_manager_ != nullptr) {
    ANativeWindow* handle =
        native_window.get() != nullptr ? native_window->handle() : nullptr;
    bool is_fake =
        native_window.get() != nullptr && native_window->IsFakeWindow();
    surface_manager_->SetNativeWindow(handle, is_fake);
    bool valid = (native_window.get() != nullptr && native_window->IsValid());
    if (valid && !surface_attached_) {
      surface_attached_ = true;
      if (engine_ != nullptr) {
        FlutterEngineNotifyCreated(engine_);
      }
    }
  }
}

void AndroidEngine::NotifySurfaceWindowChanged(
    fml::RefPtr<AndroidNativeWindow> native_window) {
  if (surface_manager_ != nullptr) {
    ANativeWindow* handle =
        native_window.get() != nullptr ? native_window->handle() : nullptr;
    bool is_fake =
        native_window.get() != nullptr && native_window->IsFakeWindow();
    surface_manager_->SetNativeWindow(handle, is_fake);
    bool valid = (native_window.get() != nullptr && native_window->IsValid());
    if (valid && !surface_attached_) {
      surface_attached_ = true;
      if (engine_ != nullptr) {
        FlutterEngineNotifyCreated(engine_);
      }
    }
  }
}

void AndroidEngine::NotifySurfaceChanged(int width, int height) {
  // Dimensions are tracked by AndroidSurfaceManager on window attach/draw.
}

void AndroidEngine::NotifySurfaceDestroyed() {
  if (surface_manager_ != nullptr) {
    if (surface_attached_) {
      surface_attached_ = false;
      if (engine_ != nullptr) {
        FlutterEngineNotifyDestroyed(engine_);
      }
    }
    surface_manager_->ClearNativeWindow();
  }
}

void AndroidEngine::SetViewportMetrics(int64_t view_id,
                                       const ViewportMetrics& metrics) {
  if (!IsValid()) {
    return;
  }

  FlutterWindowMetricsEvent event = {};
  event.struct_size = sizeof(FlutterWindowMetricsEvent);
  event.width = static_cast<size_t>(std::max(0.0, metrics.physical_width));
  event.height = static_cast<size_t>(std::max(0.0, metrics.physical_height));
  event.pixel_ratio = (!std::isfinite(metrics.device_pixel_ratio) ||
                       metrics.device_pixel_ratio <= 0.0)
                          ? 1.0
                          : metrics.device_pixel_ratio;
  event.left = 0;
  event.top = 0;
  event.physical_view_inset_top = metrics.physical_view_inset_top;
  event.physical_view_inset_right = metrics.physical_view_inset_right;
  event.physical_view_inset_bottom = metrics.physical_view_inset_bottom;
  event.physical_view_inset_left = metrics.physical_view_inset_left;
  event.display_id =
      metrics.display_id >= 0
          ? static_cast<FlutterEngineDisplayId>(metrics.display_id)
          : 0;
  event.view_id = view_id;

  if (metrics.physical_min_width_constraint > 0 ||
      metrics.physical_max_width_constraint > 0 ||
      metrics.physical_min_height_constraint > 0 ||
      metrics.physical_max_height_constraint > 0) {
    event.has_constraints = true;
    event.min_width_constraint = static_cast<size_t>(
        std::max(0.0, metrics.physical_min_width_constraint));
    event.max_width_constraint = static_cast<size_t>(
        std::max(static_cast<double>(event.min_width_constraint),
                 metrics.physical_max_width_constraint));
    event.min_height_constraint = static_cast<size_t>(
        std::max(0.0, metrics.physical_min_height_constraint));
    event.max_height_constraint = static_cast<size_t>(
        std::max(static_cast<double>(event.min_height_constraint),
                 metrics.physical_max_height_constraint));

    event.width = std::clamp(event.width, event.min_width_constraint,
                             event.max_width_constraint);
    event.height = std::clamp(event.height, event.min_height_constraint,
                              event.max_height_constraint);
  } else {
    event.has_constraints = false;
  }

  FlutterEngineSendWindowMetricsEvent(engine_, &event);
}

void AndroidEngine::UpdateDisplayMetrics() {
  if (jni_facade_ != nullptr) {
    double refresh_rate = jni_facade_->GetDisplayRefreshRate();
    FML_DLOG(INFO) << "Display refresh rate: " << refresh_rate;
  }
}

bool AndroidEngine::IsSurfaceControlEnabled() const {
  return false;
}

struct AndroidEngine::Screenshot AndroidEngine::Screenshot(ScreenshotType type,
                                                           bool base64_encode) {
  struct Screenshot result;
  if (!IsValid()) {
    return result;
  }

  FlutterScreenshotType embedder_type = kFlutterScreenshotTypeUncompressedImage;
  if (type == ScreenshotType::kCompressedImage) {
    embedder_type = kFlutterScreenshotTypeCompressedImage;
  } else if (type == ScreenshotType::kSurfaceData) {
    embedder_type = kFlutterScreenshotTypeSurfaceData;
  }

  FlutterScreenshotRequest request = {};
  request.struct_size = sizeof(FlutterScreenshotRequest);
  request.type = embedder_type;
  request.base64_encode = base64_encode;

  FlutterScreenshot screenshot = {};
  screenshot.struct_size = sizeof(FlutterScreenshot);

  if (FlutterEngineScreenshot(engine_, &request, &screenshot) == kSuccess &&
      screenshot.data != nullptr && screenshot.data_length > 0) {
    result.frame_size.width = screenshot.width;
    result.frame_size.height = screenshot.height;
    result.data = std::make_unique<fml::MallocMapping>(fml::MallocMapping::Copy(
        screenshot.data, screenshot.data + screenshot.data_length));
    FlutterEngineFreeScreenshot(&screenshot);
  }

  return result;
}

void AndroidEngine::DispatchPlatformMessage(JNIEnv* env,
                                            const std::string& name,
                                            jobject message_data,
                                            jint message_position,
                                            jint response_id) {
  if (!IsValid()) {
    return;
  }

  const uint8_t* data = nullptr;
#if FML_OS_ANDROID
  if (message_data != nullptr && env != nullptr) {
    data =
        static_cast<const uint8_t*>(env->GetDirectBufferAddress(message_data));
  }
#endif

  FlutterPlatformMessage message = {};
  message.struct_size = sizeof(FlutterPlatformMessage);
  message.channel = name.c_str();
  message.message = data;
  message.message_size = static_cast<size_t>(std::max(0, message_position));

  if (response_id != 0 && jni_facade_ != nullptr) {
    auto* context = new MessageResponseContext{jni_facade_, response_id};
    FlutterPlatformMessageResponseHandle* handle = nullptr;
    if (FlutterPlatformMessageCreateResponseHandle(
            engine_, OnPlatformMessageDataResponse, context, &handle) ==
            kSuccess &&
        handle != nullptr) {
      message.response_handle = handle;
      FlutterEngineSendPlatformMessage(engine_, &message);
      FlutterPlatformMessageReleaseResponseHandle(engine_, handle);
      return;
    } else {
      delete context;
    }
  }

  FlutterEngineSendPlatformMessage(engine_, &message);
}

void AndroidEngine::DispatchEmptyPlatformMessage(JNIEnv* env,
                                                 const std::string& name,
                                                 jint response_id) {
  DispatchPlatformMessage(env, name, nullptr, 0, response_id);
}

void AndroidEngine::DispatchPointerDataPacket(const uint8_t* buffer,
                                              size_t position) {
  if (!IsValid() || buffer == nullptr || position < kBytesPerPointerEntry ||
      position % kBytesPerPointerEntry != 0) {
    return;
  }

  std::vector<FlutterPointerEvent> events =
      UnpackPointerDataPacket(buffer, position);
  if (!events.empty()) {
    FlutterEngineSendPointerEvent(engine_, events.data(), events.size());
  }
}

int32_t AndroidEngine::GenerateNextResponseId() {
  std::lock_guard<std::mutex> lock(pending_responses_mutex_);
  return next_response_id_++;
}

const FlutterPlatformMessageResponseHandle* AndroidEngine::TakePendingResponse(
    int32_t response_id) {
  std::lock_guard<std::mutex> lock(pending_responses_mutex_);
  auto it = pending_responses_.find(response_id);
  if (it != pending_responses_.end()) {
    const FlutterPlatformMessageResponseHandle* handle = it->second;
    pending_responses_.erase(it);
    return handle;
  }
  return nullptr;
}

void AndroidEngine::SendPlatformMessageResponse(
    int32_t response_id,
    std::unique_ptr<fml::Mapping> mapping) {
  if (!IsValid()) {
    return;
  }

  const FlutterPlatformMessageResponseHandle* handle =
      TakePendingResponse(response_id);
  if (handle != nullptr) {
    FlutterEngineSendPlatformMessageResponse(
        engine_, handle, mapping ? mapping->GetMapping() : nullptr,
        mapping ? mapping->GetSize() : 0);
  }
}

void AndroidEngine::SendEmptyPlatformMessageResponse(int32_t response_id) {
  SendPlatformMessageResponse(response_id, nullptr);
}

void AndroidEngine::SetSemanticsEnabled(bool enabled) {
  if (IsValid()) {
    FlutterEngineUpdateSemanticsEnabled(engine_, enabled);
  }
}

void AndroidEngine::SetAccessibilityFeatures(int32_t flags) {
  if (IsValid()) {
    FlutterEngineUpdateAccessibilityFeatures(
        engine_, static_cast<FlutterAccessibilityFeature>(flags));
  }
}

void AndroidEngine::DispatchSemanticsAction(JNIEnv* env,
                                            jint id,
                                            jint action,
                                            jobject args,
                                            jint args_position) {
  if (!IsValid()) {
    return;
  }

  const uint8_t* data = nullptr;
#if FML_OS_ANDROID
  if (args != nullptr && env != nullptr) {
    data = static_cast<const uint8_t*>(env->GetDirectBufferAddress(args));
  }
#endif

  FlutterSendSemanticsActionInfo info = {};
  info.struct_size = sizeof(FlutterSendSemanticsActionInfo);
  info.node_id = id;
  info.action = static_cast<FlutterSemanticsAction>(action);
  info.data = data;
  info.data_length = static_cast<size_t>(std::max(0, args_position));

  FlutterEngineSendSemanticsAction(engine_, &info);
}

void AndroidEngine::RegisterExternalTexture(
    int64_t texture_id,
    const fml::jni::ScopedJavaGlobalRef<jobject>& surface_texture) {
  if (IsValid()) {
    FlutterEngineRegisterExternalTexture(engine_, texture_id);
  }
}

void AndroidEngine::RegisterImageTexture(
    int64_t texture_id,
    const fml::jni::ScopedJavaGlobalRef<jobject>& image_texture_entry,
    int32_t lifecycle) {
  if (IsValid()) {
    FlutterEngineRegisterExternalTexture(engine_, texture_id);
  }
}

void AndroidEngine::UnregisterTexture(int64_t texture_id) {
  if (IsValid()) {
    FlutterEngineUnregisterExternalTexture(engine_, texture_id);
  }
}

void AndroidEngine::MarkTextureFrameAvailable(int64_t texture_id) {
  if (IsValid()) {
    FlutterEngineMarkExternalTextureFrameAvailable(engine_, texture_id);
  }
}

void AndroidEngine::ScheduleFrame() {
  if (IsValid()) {
    FlutterEngineScheduleFrame(engine_);
  }
}

void AndroidEngine::LoadDartDeferredLibrary(
    intptr_t loading_unit_id,
    std::unique_ptr<const fml::Mapping> snapshot_data,
    std::unique_ptr<const fml::Mapping> snapshot_instructions) {
  if (!IsValid()) {
    return;
  }

  FlutterDartDeferredLibrary lib = {};
  lib.struct_size = sizeof(FlutterDartDeferredLibrary);
  lib.loading_unit_id = loading_unit_id;
  lib.data = snapshot_data ? snapshot_data->GetMapping() : nullptr;
  lib.data_size = snapshot_data ? snapshot_data->GetSize() : 0;
  lib.instructions =
      snapshot_instructions ? snapshot_instructions->GetMapping() : nullptr;
  lib.instructions_size =
      snapshot_instructions ? snapshot_instructions->GetSize() : 0;

  struct LibraryContext {
    std::unique_ptr<const fml::Mapping> data;
    std::unique_ptr<const fml::Mapping> instructions;
  };
  auto* context = new LibraryContext{std::move(snapshot_data),
                                     std::move(snapshot_instructions)};
  lib.user_data = context;
  lib.destruction_callback = [](void* user_data) {
    delete static_cast<LibraryContext*>(user_data);
  };

  FlutterEngineLoadDartDeferredLibrary(engine_, &lib);
}

void AndroidEngine::LoadDartDeferredLibraryError(
    intptr_t loading_unit_id,
    const std::string& error_message,
    bool transient) {
  if (!IsValid()) {
    return;
  }

  FlutterDartDeferredLibraryLoadError err = {};
  err.struct_size = sizeof(FlutterDartDeferredLibraryLoadError);
  err.loading_unit_id = loading_unit_id;
  err.error_message = error_message.c_str();
  err.transient = transient;

  FlutterEngineNotifyDartDeferredLibraryLoadError(engine_, &err);
}

void AndroidEngine::UpdateAssetResolverByType(
    std::unique_ptr<APKAssetProvider> updated_asset_resolver,
    int32_t type) {
  apk_asset_provider_ = std::move(updated_asset_resolver);
  if (apk_asset_provider_ != nullptr) {
    asset_resolver_ = apk_asset_provider_->ToFlutterAssetResolver();
    asset_resolvers_array_[0] = &asset_resolver_;
    if (IsValid()) {
      FlutterAssetResolverRegistrationInfo info = {};
      info.struct_size = sizeof(FlutterAssetResolverRegistrationInfo);
      info.resolver = &asset_resolver_;
      FlutterEngineUpdateAssetResolver(engine_, &info);
    }
  }
}

void AndroidEngine::NotifyLowMemoryWarning() {
  if (IsValid()) {
    FlutterEngineNotifyLowMemoryWarning(engine_);
  }
}

void AndroidEngine::OnPlatformMessageCallback(
    const FlutterPlatformMessage* message,
    void* user_data) {
  auto* engine = static_cast<AndroidEngine*>(user_data);
  if (engine == nullptr || engine->jni_facade_ == nullptr ||
      message == nullptr) {
    return;
  }

  int32_t response_id = 0;
  if (message->response_handle != nullptr) {
    response_id = engine->GenerateNextResponseId();
    std::lock_guard<std::mutex> lock(engine->pending_responses_mutex_);
    engine->pending_responses_[response_id] = message->response_handle;
  }

  std::unique_ptr<flutter::PlatformMessage> platform_message;
  if (message->message != nullptr && message->message_size > 0) {
    fml::MallocMapping data_mapping = fml::MallocMapping::Copy(
        message->message, message->message + message->message_size);
    platform_message = std::make_unique<flutter::PlatformMessage>(
        message->channel, std::move(data_mapping), nullptr);
  } else {
    platform_message =
        std::make_unique<flutter::PlatformMessage>(message->channel, nullptr);
  }

  engine->jni_facade_->FlutterViewHandlePlatformMessage(
      std::move(platform_message), response_id);
}

void AndroidEngine::OnUpdateSemantics2Callback(
    const FlutterSemanticsUpdate2* update,
    void* user_data) {
  auto* engine = static_cast<AndroidEngine*>(user_data);
  if (engine == nullptr || engine->jni_facade_ == nullptr ||
      update == nullptr || engine->task_runners_ == nullptr) {
    return;
  }

  std::vector<uint8_t> buffer;
  std::vector<std::string> strings;
  std::vector<std::vector<uint8_t>> string_attribute_args;
  std::vector<uint8_t> actions_buffer;
  std::vector<std::string> action_strings;

  SerializeSemanticsUpdate(update, buffer, strings, string_attribute_args,
                           actions_buffer, action_strings);

  engine->task_runners_->GetPlatformTaskRunner()->PostTask(
      [jni = engine->jni_facade_, buffer = std::move(buffer),
       strings = std::move(strings),
       string_attribute_args = std::move(string_attribute_args),
       actions_buffer = std::move(actions_buffer),
       action_strings = std::move(action_strings)]() mutable {
        if (!buffer.empty()) {
          jni->FlutterViewUpdateSemantics(std::move(buffer), std::move(strings),
                                          std::move(string_attribute_args));
        }
        if (!actions_buffer.empty()) {
          jni->FlutterViewUpdateCustomAccessibilityActions(
              std::move(actions_buffer), std::move(action_strings));
        }
      });
}

void AndroidEngine::OnDartDeferredLibraryRequestCallback(
    int64_t loading_unit_id,
    void* user_data) {
  auto* engine = static_cast<AndroidEngine*>(user_data);
  if (engine != nullptr && engine->jni_facade_ != nullptr) {
    engine->jni_facade_->RequestDartDeferredLibrary(
        static_cast<int>(loading_unit_id));
  }
}

void AndroidEngine::OnRasterContextSetupCallback(void* user_data) {
  auto* engine = static_cast<AndroidEngine*>(user_data);
  if (engine != nullptr && engine->surface_manager_ != nullptr) {
    engine->surface_manager_->MakeResourceCurrent();
  }
}

void AndroidEngine::OnRasterContextTeardownCallback(void* user_data) {
  auto* engine = static_cast<AndroidEngine*>(user_data);
  if (engine != nullptr && engine->surface_manager_ != nullptr) {
    engine->surface_manager_->ClearCurrent();
  }
}

void AndroidEngine::OnPlatformViewPresented(
    int64_t view_id,
    const FlutterPoint& offset,
    const FlutterSize& size,
    size_t mutations_count,
    const FlutterPlatformViewMutation** mutations) {
  if (jni_facade_ == nullptr || task_runners_ == nullptr) {
    return;
  }

  MutatorsStack stack;
  if (mutations != nullptr && mutations_count > 0) {
    for (size_t i = 0; i < mutations_count; ++i) {
      const FlutterPlatformViewMutation* m = mutations[i];
      if (m == nullptr) {
        continue;
      }
      switch (m->type) {
        case kFlutterPlatformViewMutationTypeTransformation: {
          const auto& t = m->transformation;
          DlMatrix matrix = DlMatrix::MakeRow(
              static_cast<DlScalar>(t.scaleX), static_cast<DlScalar>(t.skewX),
              0.0f, static_cast<DlScalar>(t.transX),
              static_cast<DlScalar>(t.skewY), static_cast<DlScalar>(t.scaleY),
              0.0f, static_cast<DlScalar>(t.transY), 0.0f, 0.0f, 1.0f, 0.0f,
              static_cast<DlScalar>(t.pers0), static_cast<DlScalar>(t.pers1),
              0.0f, static_cast<DlScalar>(t.pers2));
          stack.PushTransform(matrix);
          break;
        }
        case kFlutterPlatformViewMutationTypeOpacity: {
          uint8_t alpha =
              static_cast<uint8_t>(std::clamp(m->opacity * 255.0, 0.0, 255.0));
          stack.PushOpacity(alpha);
          break;
        }
        case kFlutterPlatformViewMutationTypeClipRect: {
          const auto& r = m->clip_rect;
          stack.PushClipRect(DlRect::MakeLTRB(
              static_cast<DlScalar>(r.left), static_cast<DlScalar>(r.top),
              static_cast<DlScalar>(r.right), static_cast<DlScalar>(r.bottom)));
          break;
        }
        case kFlutterPlatformViewMutationTypeClipRoundedRect: {
          const auto& rr = m->clip_rounded_rect;
          DlRect bounds =
              DlRect::MakeLTRB(static_cast<DlScalar>(rr.rect.left),
                               static_cast<DlScalar>(rr.rect.top),
                               static_cast<DlScalar>(rr.rect.right),
                               static_cast<DlScalar>(rr.rect.bottom));
          DlRoundingRadii radii = {
              .top_left = DlSize(
                  static_cast<DlScalar>(rr.upper_left_corner_radius.width),
                  static_cast<DlScalar>(rr.upper_left_corner_radius.height)),
              .top_right = DlSize(
                  static_cast<DlScalar>(rr.upper_right_corner_radius.width),
                  static_cast<DlScalar>(rr.upper_right_corner_radius.height)),
              .bottom_left = DlSize(
                  static_cast<DlScalar>(rr.lower_left_corner_radius.width),
                  static_cast<DlScalar>(rr.lower_left_corner_radius.height)),
              .bottom_right = DlSize(
                  static_cast<DlScalar>(rr.lower_right_corner_radius.width),
                  static_cast<DlScalar>(rr.lower_right_corner_radius.height)),
          };
          stack.PushClipRRect(DlRoundRect::MakeRectRadii(bounds, radii));
          break;
        }
        case kFlutterPlatformViewMutationTypeClipRoundSuperellipse: {
          const auto& rse = m->clip_round_superellipse;
          DlRect bounds =
              DlRect::MakeLTRB(static_cast<DlScalar>(rse.rect.left),
                               static_cast<DlScalar>(rse.rect.top),
                               static_cast<DlScalar>(rse.rect.right),
                               static_cast<DlScalar>(rse.rect.bottom));
          DlRoundingRadii radii = {
              .top_left = DlSize(
                  static_cast<DlScalar>(rse.upper_left_corner_radius.width),
                  static_cast<DlScalar>(rse.upper_left_corner_radius.height)),
              .top_right = DlSize(
                  static_cast<DlScalar>(rse.upper_right_corner_radius.width),
                  static_cast<DlScalar>(rse.upper_right_corner_radius.height)),
              .bottom_left = DlSize(
                  static_cast<DlScalar>(rse.lower_left_corner_radius.width),
                  static_cast<DlScalar>(rse.lower_left_corner_radius.height)),
              .bottom_right = DlSize(
                  static_cast<DlScalar>(rse.lower_right_corner_radius.width),
                  static_cast<DlScalar>(rse.lower_right_corner_radius.height)),
          };
          stack.PushClipRSE(DlRoundSuperellipse::MakeRectRadii(bounds, radii));
          break;
        }
        case kFlutterPlatformViewMutationTypeClipPath: {
          DlPathBuilder builder;
          const auto& path = m->clip_path;
          if (path.fill_type == kFlutterPathFillTypeEvenOdd) {
            builder.SetFillType(DlPathFillType::kOdd);
          } else {
            builder.SetFillType(DlPathFillType::kNonZero);
          }
          if (path.segments != nullptr && path.segments_count > 0) {
            for (size_t s = 0; s < path.segments_count; ++s) {
              const auto& seg = path.segments[s];
              switch (seg.verb) {
                case kFlutterPathVerbMove:
                  builder.MoveTo(
                      DlPoint(static_cast<DlScalar>(seg.points[0].x),
                              static_cast<DlScalar>(seg.points[0].y)));
                  break;
                case kFlutterPathVerbLine:
                  builder.LineTo(
                      DlPoint(static_cast<DlScalar>(seg.points[1].x),
                              static_cast<DlScalar>(seg.points[1].y)));
                  break;
                case kFlutterPathVerbQuad:
                  builder.QuadraticCurveTo(
                      DlPoint(static_cast<DlScalar>(seg.points[1].x),
                              static_cast<DlScalar>(seg.points[1].y)),
                      DlPoint(static_cast<DlScalar>(seg.points[2].x),
                              static_cast<DlScalar>(seg.points[2].y)));
                  break;
                case kFlutterPathVerbConic:
                  builder.ConicCurveTo(
                      DlPoint(static_cast<DlScalar>(seg.points[1].x),
                              static_cast<DlScalar>(seg.points[1].y)),
                      DlPoint(static_cast<DlScalar>(seg.points[2].x),
                              static_cast<DlScalar>(seg.points[2].y)),
                      static_cast<DlScalar>(seg.conic_weight));
                  break;
                case kFlutterPathVerbCubic:
                  builder.CubicCurveTo(
                      DlPoint(static_cast<DlScalar>(seg.points[1].x),
                              static_cast<DlScalar>(seg.points[1].y)),
                      DlPoint(static_cast<DlScalar>(seg.points[2].x),
                              static_cast<DlScalar>(seg.points[2].y)),
                      DlPoint(static_cast<DlScalar>(seg.points[3].x),
                              static_cast<DlScalar>(seg.points[3].y)));
                  break;
                case kFlutterPathVerbClose:
                  builder.Close();
                  break;
              }
            }
          }
          stack.PushClipPath(builder.TakePath());
          break;
        }
      }
    }
  }

  int x = static_cast<int>(std::round(offset.x));
  int y = static_cast<int>(std::round(offset.y));
  int width = static_cast<int>(std::round(size.width));
  int height = static_cast<int>(std::round(size.height));

  task_runners_->GetPlatformTaskRunner()->PostTask(
      [jni = jni_facade_, view_id, x, y, width, height,
       stack = std::move(stack)]() mutable {
        jni->FlutterViewOnDisplayPlatformView(view_id, x, y, width, height,
                                              width, height, std::move(stack));
      });
}

void AndroidEngine::OnFramePresented() {
  if (jni_facade_ != nullptr && task_runners_ != nullptr) {
    task_runners_->GetPlatformTaskRunner()->PostTask(
        [jni = jni_facade_]() { jni->FlutterViewEndFrame(); });
  }
}

}  // namespace flutter
