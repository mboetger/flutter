// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/platform_view_android_delegate/platform_view_android_delegate.h"

#include <cstring>
#include <utility>

namespace flutter {
namespace {
void putStringAttributesIntoBuffer(
    const FlutterStringAttribute* const* attributes,
    size_t count,
    int32_t* buffer,
    size_t* position,
    std::vector<std::vector<uint8_t>>& string_attribute_args) {
  if (attributes == nullptr || count == 0) {
    buffer[(*position)++] = PlatformViewAndroidDelegate::kEmptyStringIndex;
    return;
  }
  buffer[(*position)++] = count;
  for (size_t i = 0; i < count; ++i) {
    const FlutterStringAttribute* attribute = attributes[i];
    if (attribute == nullptr) {
      buffer[(*position)++] = 0;
      buffer[(*position)++] = 0;
      buffer[(*position)++] =
          static_cast<int32_t>(FlutterStringAttributeType::kSpellOut);
      buffer[(*position)++] = PlatformViewAndroidDelegate::kEmptyStringIndex;
      continue;
    }
    buffer[(*position)++] = attribute->start;
    buffer[(*position)++] = attribute->end;
    buffer[(*position)++] = static_cast<int32_t>(attribute->type);
    switch (attribute->type) {
      case FlutterStringAttributeType::kSpellOut:
        buffer[(*position)++] = PlatformViewAndroidDelegate::kEmptyStringIndex;
        break;
      case FlutterStringAttributeType::kLocale:
        buffer[(*position)++] = string_attribute_args.size();
        if (attribute->locale != nullptr &&
            attribute->locale->locale != nullptr) {
          std::string locale(attribute->locale->locale);
          string_attribute_args.push_back({locale.begin(), locale.end()});
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
  if (string == nullptr || string[0] == '\0') {
    buffer[(*position)++] = PlatformViewAndroidDelegate::kEmptyStringIndex;
  } else {
    buffer[(*position)++] = strings.size();
    strings.emplace_back(string);
  }
}

void putTransformationIntoBuffer(const FlutterTransformation& transform,
                                 float* buffer_float32) {
  if (transform.scaleX == 0.0 && transform.scaleY == 0.0 &&
      transform.skewX == 0.0 && transform.skewY == 0.0 &&
      transform.transX == 0.0 && transform.transY == 0.0 &&
      transform.pers0 == 0.0 && transform.pers1 == 0.0 &&
      transform.pers2 == 0.0) {
    std::memset(buffer_float32, 0, 16 * sizeof(float));
    buffer_float32[0] = 1.0f;
    buffer_float32[5] = 1.0f;
    buffer_float32[10] = 1.0f;
    buffer_float32[15] = 1.0f;
    return;
  }

  // Column 0
  buffer_float32[0] = static_cast<float>(transform.scaleX);
  buffer_float32[1] = static_cast<float>(transform.skewY);
  buffer_float32[2] = 0.0f;
  buffer_float32[3] = static_cast<float>(transform.pers0);

  // Column 1
  buffer_float32[4] = static_cast<float>(transform.skewX);
  buffer_float32[5] = static_cast<float>(transform.scaleY);
  buffer_float32[6] = 0.0f;
  buffer_float32[7] = static_cast<float>(transform.pers1);

  // Column 2
  buffer_float32[8] = 0.0f;
  buffer_float32[9] = 0.0f;
  buffer_float32[10] = 1.0f;
  buffer_float32[11] = 0.0f;

  // Column 3
  buffer_float32[12] = static_cast<float>(transform.transX);
  buffer_float32[13] = static_cast<float>(transform.transY);
  buffer_float32[14] = 0.0f;
  buffer_float32[15] =
      static_cast<float>(transform.pers2 != 0.0 ? transform.pers2 : 1.0);
}

int64_t flagsToInt64(const FlutterSemanticsFlags* flags,
                     FlutterSemanticsFlag deprecated_flags) {
  if (flags == nullptr) {
    return static_cast<int64_t>(deprecated_flags);
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
    const FlutterSemanticsUpdate2& update) {
  {
    size_t num_bytes = 0;
    for (size_t i = 0; i < update.node_count; ++i) {
      const FlutterSemanticsNode2* node =
          update.nodes != nullptr ? update.nodes[i] : nullptr;
      if (node == nullptr) {
        continue;
      }
      num_bytes += kBytesPerNode;
      if (node->children_in_traversal_order != nullptr) {
        num_bytes += node->child_count * kBytesPerChild;
      }
      if (node->children_in_hit_test_order != nullptr) {
        num_bytes += node->hit_test_child_count * kBytesPerChild;
      }
      if (node->custom_accessibility_actions != nullptr) {
        num_bytes +=
            node->custom_accessibility_actions_count * kBytesPerCustomAction;
      }
      if (node->label_attributes != nullptr) {
        num_bytes += node->label_attribute_count * kBytesPerStringAttribute;
      }
      if (node->value_attributes != nullptr) {
        num_bytes += node->value_attribute_count * kBytesPerStringAttribute;
      }
      if (node->increased_value_attributes != nullptr) {
        num_bytes +=
            node->increased_value_attribute_count * kBytesPerStringAttribute;
      }
      if (node->decreased_value_attributes != nullptr) {
        num_bytes +=
            node->decreased_value_attribute_count * kBytesPerStringAttribute;
      }
      if (node->hint_attributes != nullptr) {
        num_bytes += node->hint_attribute_count * kBytesPerStringAttribute;
      }
    }
    // The encoding defined here is used in:
    //
    //  * AccessibilityBridge.java
    //  * AccessibilityBridgeTest.java
    //  * accessibility_bridge.mm
    //
    // If any of the encoding structure or length is changed, those locations
    // must be updated (at a minimum).
    std::vector<uint8_t> buffer(num_bytes);
    std::vector<std::string> strings;
    std::vector<std::vector<uint8_t>> string_attribute_args;

    if (!buffer.empty()) {
      int32_t* buffer_int32 = reinterpret_cast<int32_t*>(buffer.data());
      float* buffer_float32 = reinterpret_cast<float*>(buffer.data());

      size_t position = 0;
      for (size_t i = 0; i < update.node_count; ++i) {
        const FlutterSemanticsNode2* node =
            update.nodes != nullptr ? update.nodes[i] : nullptr;
        if (node == nullptr) {
          continue;
        }
        // If you edit this code, make sure you update kBytesPerNode
        // and/or kBytesPerChild above to match the number of values you are
        // sending.
        buffer_int32[position++] = node->id;
        int64_t flags = flagsToInt64(node->flags2, node->flags__deprecated__);
        std::memcpy(&buffer_int32[position], &flags, 8);
        position += 2;
        buffer_int32[position++] = node->actions;
        buffer_int32[position++] = node->max_value_length;
        buffer_int32[position++] = node->current_value_length;
        buffer_int32[position++] = node->text_selection_base;
        buffer_int32[position++] = node->text_selection_extent;
        buffer_int32[position++] = static_cast<int32_t>(node->platform_view_id);
        buffer_int32[position++] = node->scroll_child_count;
        buffer_int32[position++] = node->scroll_index;
        buffer_int32[position++] = node->traversal_parent;
        buffer_float32[position++] = static_cast<float>(node->scroll_position);
        buffer_float32[position++] =
            static_cast<float>(node->scroll_extent_max);
        buffer_float32[position++] =
            static_cast<float>(node->scroll_extent_min);
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
        putTransformationIntoBuffer(node->transform, &buffer_float32[position]);
        position += 16;
        putTransformationIntoBuffer(node->hit_test_transform,
                                    &buffer_float32[position]);
        position += 16;

        const size_t traversal_children =
            node->children_in_traversal_order != nullptr ? node->child_count
                                                         : 0;
        buffer_int32[position++] = traversal_children;
        for (size_t c = 0; c < traversal_children; ++c) {
          buffer_int32[position++] = node->children_in_traversal_order[c];
        }

        const size_t hit_test_children =
            node->children_in_hit_test_order != nullptr
                ? node->hit_test_child_count
                : 0;
        buffer_int32[position++] = hit_test_children;
        for (size_t c = 0; c < hit_test_children; ++c) {
          buffer_int32[position++] = node->children_in_hit_test_order[c];
        }

        const size_t custom_actions =
            node->custom_accessibility_actions != nullptr
                ? node->custom_accessibility_actions_count
                : 0;
        buffer_int32[position++] = custom_actions;
        for (size_t a = 0; a < custom_actions; ++a) {
          buffer_int32[position++] = node->custom_accessibility_actions[a];
        }
      }
    }

    // custom accessibility actions.
    size_t valid_action_count = 0;
    for (size_t i = 0; i < update.custom_action_count; ++i) {
      if (update.custom_actions != nullptr &&
          update.custom_actions[i] != nullptr) {
        valid_action_count++;
      }
    }
    size_t num_action_bytes = valid_action_count * kBytesPerAction;
    std::vector<uint8_t> actions_buffer(num_action_bytes);

    if (!actions_buffer.empty()) {
      int32_t* actions_buffer_int32 =
          reinterpret_cast<int32_t*>(actions_buffer.data());

      std::vector<std::string> action_strings;
      size_t actions_position = 0;
      for (size_t i = 0; i < update.custom_action_count; ++i) {
        const FlutterSemanticsCustomAction2* action =
            update.custom_actions != nullptr ? update.custom_actions[i]
                                             : nullptr;
        if (action == nullptr) {
          continue;
        }
        // If you edit this code, make sure you update kBytesPerAction
        // to match the number of values you are
        // sending.
        actions_buffer_int32[actions_position++] = action->id;
        actions_buffer_int32[actions_position++] =
            static_cast<int32_t>(action->override_action);
        putStringIntoBuffer(action->label, actions_buffer_int32,
                            &actions_position, action_strings);
        putStringIntoBuffer(action->hint, actions_buffer_int32,
                            &actions_position, action_strings);
      }
      // Calling NewDirectByteBuffer in API level 22 and below with a size of
      // zero will cause a JNI crash.
      jni_facade_->FlutterViewUpdateCustomAccessibilityActions(actions_buffer,
                                                               action_strings);
    }

    if (!buffer.empty()) {
      jni_facade_->FlutterViewUpdateSemantics(buffer, strings,
                                              string_attribute_args);
    }
  }
}

}  // namespace flutter
