// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/platform_view_android_delegate/platform_view_android_delegate.h"

#include <cstring>
#include <utility>

namespace flutter {
namespace {

size_t getValidStringAttributeCount(const FlutterStringAttribute** attributes,
                                    size_t count) {
  if (attributes == nullptr || count == 0) {
    return 0;
  }
  size_t valid = 0;
  for (size_t i = 0; i < count; ++i) {
    if (attributes[i] != nullptr) {
      valid++;
    }
  }
  return valid;
}

void putStringAttributesIntoBuffer(
    const FlutterStringAttribute** attributes,
    size_t count,
    int32_t* buffer,
    size_t* position,
    std::vector<std::vector<uint8_t>>& string_attribute_args) {
  size_t valid_count = getValidStringAttributeCount(attributes, count);
  if (valid_count == 0) {
    buffer[(*position)++] = PlatformViewAndroidDelegate::kEmptyStringIndex;
    return;
  }
  buffer[(*position)++] = static_cast<int32_t>(valid_count);
  for (size_t i = 0; i < count; ++i) {
    const auto* attribute = attributes[i];
    if (!attribute) {
      continue;
    }
    buffer[(*position)++] = static_cast<int32_t>(attribute->start);
    buffer[(*position)++] = static_cast<int32_t>(attribute->end);
    buffer[(*position)++] = static_cast<int32_t>(attribute->type);
    switch (attribute->type) {
      case FlutterStringAttributeType::kSpellOut:
        buffer[(*position)++] = PlatformViewAndroidDelegate::kEmptyStringIndex;
        break;
      case FlutterStringAttributeType::kLocale:
        buffer[(*position)++] = string_attribute_args.size();
        if (attribute->locale && attribute->locale->locale) {
          const char* loc = attribute->locale->locale;
          string_attribute_args.push_back(
              std::vector<uint8_t>(loc, loc + std::strlen(loc)));
        } else {
          string_attribute_args.push_back({});
        }
        break;
    }
  }
}

void putStringIntoBuffer(const char* string,
                         int32_t* buffer,
                         size_t* position,
                         std::vector<std::string>& strings) {
  if (string == nullptr || std::strlen(string) == 0) {
    buffer[(*position)++] = PlatformViewAndroidDelegate::kEmptyStringIndex;
  } else {
    buffer[(*position)++] = strings.size();
    strings.push_back(std::string(string));
  }
}

void putTransformationIntoBuffer(const FlutterTransformation& transform,
                                 float* buffer,
                                 size_t* position) {
  // FlutterTransformation is row-major 3x3:
  // [ scaleX,  skewX,  transX ]
  // [ skewY,   scaleY, transY ]
  // [ pers0,   pers1,  pers2  ]
  // 4x4 column-major matrix:
  // col 0: [scaleX, skewY,  0, pers0]
  // col 1: [skewX,  scaleY, 0, pers1]
  // col 2: [0,      0,      1, 0    ]
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

int64_t flagsToInt64(const FlutterSemanticsFlags* flags) {
  if (!flags) {
    return 0;
  }
  int64_t result = 0;
  if (flags->is_checked != kFlutterCheckStateNone) {
    result |= (INT64_C(1) << 0);
  }
  if (flags->is_checked == kFlutterCheckStateTrue) {
    result |= (INT64_C(1) << 1);
  }
  if (flags->is_selected == kFlutterTristateTrue) {
    result |= (INT64_C(1) << 2);
  }
  if (flags->is_button) {
    result |= (INT64_C(1) << 3);
  }
  if (flags->is_text_field) {
    result |= (INT64_C(1) << 4);
  }
  if (flags->is_focused == kFlutterTristateTrue) {
    result |= (INT64_C(1) << 5);
  }
  if (flags->is_enabled != kFlutterTristateNone) {
    result |= (INT64_C(1) << 6);
  }
  if (flags->is_enabled == kFlutterTristateTrue) {
    result |= (INT64_C(1) << 7);
  }
  if (flags->is_in_mutually_exclusive_group) {
    result |= (INT64_C(1) << 8);
  }
  if (flags->is_header) {
    result |= (INT64_C(1) << 9);
  }
  if (flags->is_obscured) {
    result |= (INT64_C(1) << 10);
  }
  if (flags->scopes_route) {
    result |= (INT64_C(1) << 11);
  }
  if (flags->names_route) {
    result |= (INT64_C(1) << 12);
  }
  if (flags->is_hidden) {
    result |= (INT64_C(1) << 13);
  }
  if (flags->is_image) {
    result |= (INT64_C(1) << 14);
  }
  if (flags->is_live_region) {
    result |= (INT64_C(1) << 15);
  }
  if (flags->is_toggled != kFlutterTristateNone) {
    result |= (INT64_C(1) << 16);
  }
  if (flags->is_toggled == kFlutterTristateTrue) {
    result |= (INT64_C(1) << 17);
  }
  if (flags->has_implicit_scrolling) {
    result |= (INT64_C(1) << 18);
  }
  if (flags->is_multiline) {
    result |= (INT64_C(1) << 19);
  }
  if (flags->is_read_only) {
    result |= (INT64_C(1) << 20);
  }
  if (flags->is_focused != kFlutterTristateNone) {
    result |= (INT64_C(1) << 21);
  }
  if (flags->is_link) {
    result |= (INT64_C(1) << 22);
  }
  if (flags->is_slider) {
    result |= (INT64_C(1) << 23);
  }
  if (flags->is_keyboard_key) {
    result |= (INT64_C(1) << 24);
  }
  if (flags->is_checked == kFlutterCheckStateMixed) {
    result |= (INT64_C(1) << 25);
  }
  if (flags->is_expanded != kFlutterTristateNone) {
    result |= (INT64_C(1) << 26);
  }
  if (flags->is_expanded == kFlutterTristateTrue) {
    result |= (INT64_C(1) << 27);
  }
  if (flags->is_selected != kFlutterTristateNone) {
    result |= (INT64_C(1) << 28);
  }
  if (flags->is_required != kFlutterTristateNone) {
    result |= (INT64_C(1) << 29);
  }
  if (flags->is_required == kFlutterTristateTrue) {
    result |= (INT64_C(1) << 30);
  }
  if (flags->is_accessibility_focus_blocked) {
    result |= (INT64_C(1) << 31);
  }
  return result;
}
}  // namespace

PlatformViewAndroidDelegate::PlatformViewAndroidDelegate(
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade)
    : jni_facade_(std::move(jni_facade)) {};

void PlatformViewAndroidDelegate::UpdateSemantics(
    const FlutterSemanticsUpdate2* update) {
  if (!update) {
    return;
  }

  size_t num_bytes = 0;
  if (update->nodes && update->node_count > 0) {
    for (size_t i = 0; i < update->node_count; ++i) {
      if (!update->nodes[i]) {
        continue;
      }
      const auto* node = update->nodes[i];
      num_bytes += kBytesPerNode;
      const size_t traversal_count =
          (node->children_in_traversal_order && node->child_count > 0)
              ? node->child_count
              : 0;
      const size_t hit_test_count =
          (node->children_in_hit_test_order && node->child_count > 0)
              ? node->child_count
              : 0;
      const size_t custom_actions_count =
          (node->custom_accessibility_actions &&
           node->custom_accessibility_actions_count > 0)
              ? node->custom_accessibility_actions_count
              : 0;

      num_bytes += traversal_count * kBytesPerChild;
      num_bytes += hit_test_count * kBytesPerChild;
      num_bytes += custom_actions_count * kBytesPerCustomAction;

      num_bytes += getValidStringAttributeCount(node->label_attributes,
                                                node->label_attribute_count) *
                   kBytesPerStringAttribute;
      num_bytes += getValidStringAttributeCount(node->value_attributes,
                                                node->value_attribute_count) *
                   kBytesPerStringAttribute;
      num_bytes +=
          getValidStringAttributeCount(node->increased_value_attributes,
                                       node->increased_value_attribute_count) *
          kBytesPerStringAttribute;
      num_bytes +=
          getValidStringAttributeCount(node->decreased_value_attributes,
                                       node->decreased_value_attribute_count) *
          kBytesPerStringAttribute;
      num_bytes += getValidStringAttributeCount(node->hint_attributes,
                                                node->hint_attribute_count) *
                   kBytesPerStringAttribute;
    }
  }

  std::vector<uint8_t> buffer(num_bytes);
  std::vector<std::string> strings;
  std::vector<std::vector<uint8_t>> string_attribute_args;

  if (!buffer.empty()) {
    int32_t* buffer_int32 = reinterpret_cast<int32_t*>(buffer.data());
    float* buffer_float32 = reinterpret_cast<float*>(buffer.data());

    size_t position = 0;
    for (size_t i = 0; i < update->node_count; ++i) {
      if (!update->nodes[i]) {
        continue;
      }
      const auto* node = update->nodes[i];
      buffer_int32[position++] = node->id;
      int64_t flags = flagsToInt64(node->flags2);
      std::memcpy(&buffer_int32[position], &flags, 8);
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

      putStringIntoBuffer(node->identifier, buffer_int32, &position, strings);
      putStringIntoBuffer(node->label, buffer_int32, &position, strings);
      putStringAttributesIntoBuffer(node->label_attributes,
                                    node->label_attribute_count, buffer_int32,
                                    &position, string_attribute_args);

      putStringIntoBuffer(node->value, buffer_int32, &position, strings);
      putStringAttributesIntoBuffer(node->value_attributes,
                                    node->value_attribute_count, buffer_int32,
                                    &position, string_attribute_args);

      putStringIntoBuffer(node->increased_value, buffer_int32, &position,
                          strings);
      putStringAttributesIntoBuffer(node->increased_value_attributes,
                                    node->increased_value_attribute_count,
                                    buffer_int32, &position,
                                    string_attribute_args);

      putStringIntoBuffer(node->decreased_value, buffer_int32, &position,
                          strings);
      putStringAttributesIntoBuffer(node->decreased_value_attributes,
                                    node->decreased_value_attribute_count,
                                    buffer_int32, &position,
                                    string_attribute_args);

      putStringIntoBuffer(node->hint, buffer_int32, &position, strings);
      putStringAttributesIntoBuffer(node->hint_attributes,
                                    node->hint_attribute_count, buffer_int32,
                                    &position, string_attribute_args);

      putStringIntoBuffer(node->tooltip, buffer_int32, &position, strings);
      putStringIntoBuffer(node->link_url, buffer_int32, &position, strings);
      putStringIntoBuffer(node->locale, buffer_int32, &position, strings);
      putStringIntoBuffer(node->min_value, buffer_int32, &position, strings);
      putStringIntoBuffer(node->max_value, buffer_int32, &position, strings);

      buffer_int32[position++] = node->heading_level;
      buffer_int32[position++] = static_cast<int32_t>(node->text_direction);
      buffer_float32[position++] = static_cast<float>(node->rect.left);
      buffer_float32[position++] = static_cast<float>(node->rect.top);
      buffer_float32[position++] = static_cast<float>(node->rect.right);
      buffer_float32[position++] = static_cast<float>(node->rect.bottom);

      putTransformationIntoBuffer(node->transform, buffer_float32, &position);
      putTransformationIntoBuffer(node->hit_test_transform, buffer_float32,
                                  &position);

      const size_t traversal_count =
          (node->children_in_traversal_order && node->child_count > 0)
              ? node->child_count
              : 0;
      buffer_int32[position++] = static_cast<int32_t>(traversal_count);
      for (size_t j = 0; j < traversal_count; ++j) {
        buffer_int32[position++] = node->children_in_traversal_order[j];
      }

      const size_t hit_test_count =
          (node->children_in_hit_test_order && node->child_count > 0)
              ? node->child_count
              : 0;
      buffer_int32[position++] = static_cast<int32_t>(hit_test_count);
      for (size_t j = 0; j < hit_test_count; ++j) {
        buffer_int32[position++] = node->children_in_hit_test_order[j];
      }

      const size_t custom_actions_count =
          (node->custom_accessibility_actions &&
           node->custom_accessibility_actions_count > 0)
              ? node->custom_accessibility_actions_count
              : 0;
      buffer_int32[position++] = static_cast<int32_t>(custom_actions_count);
      for (size_t j = 0; j < custom_actions_count; ++j) {
        buffer_int32[position++] = node->custom_accessibility_actions[j];
      }
    }
  }

  // Custom accessibility actions.
  if (update->custom_actions && update->custom_action_count > 0) {
    size_t num_action_bytes = update->custom_action_count * kBytesPerAction;
    std::vector<uint8_t> actions_buffer(num_action_bytes);

    int32_t* actions_buffer_int32 =
        reinterpret_cast<int32_t*>(actions_buffer.data());

    std::vector<std::string> action_strings;
    size_t actions_position = 0;
    for (size_t i = 0; i < update->custom_action_count; ++i) {
      if (!update->custom_actions[i]) {
        continue;
      }
      const auto* action = update->custom_actions[i];
      actions_buffer_int32[actions_position++] = action->id;
      actions_buffer_int32[actions_position++] =
          static_cast<int32_t>(action->override_action);
      putStringIntoBuffer(action->label, actions_buffer_int32,
                          &actions_position, action_strings);
      putStringIntoBuffer(action->hint, actions_buffer_int32, &actions_position,
                          action_strings);
    }
    if (actions_position > 0) {
      actions_buffer.resize(actions_position * sizeof(int32_t));
      jni_facade_->FlutterViewUpdateCustomAccessibilityActions(actions_buffer,
                                                               action_strings);
    }
  }

  if (!buffer.empty()) {
    jni_facade_->FlutterViewUpdateSemantics(buffer, strings,
                                            string_attribute_args);
  }
}

}  // namespace flutter
