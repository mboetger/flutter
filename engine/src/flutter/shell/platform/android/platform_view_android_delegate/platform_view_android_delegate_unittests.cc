// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/platform_view_android_delegate/platform_view_android_delegate.h"

#include <cstring>

#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace flutter {
namespace testing {

namespace {

void WriteIdentityColMajor4x4(float* dst) {
  for (size_t i = 0; i < 16; ++i) {
    dst[i] = (i % 5 == 0) ? 1.0f : 0.0f;
  }
}

void WriteFlagsBitfield(const FlutterSemanticsFlags& flags, int32_t* dst) {
  uint64_t bitfield = 0;
  if (flags.is_checked != kFlutterCheckStateNone) {
    bitfield |= (uint64_t{1} << 0);
  }
  if (flags.is_checked == kFlutterCheckStateTrue) {
    bitfield |= (uint64_t{1} << 1);
  }
  if (flags.is_selected == kFlutterTristateTrue) {
    bitfield |= (uint64_t{1} << 2);
  }
  if (flags.is_button) {
    bitfield |= (uint64_t{1} << 3);
  }
  if (flags.is_text_field) {
    bitfield |= (uint64_t{1} << 4);
  }
  if (flags.is_focused == kFlutterTristateTrue) {
    bitfield |= (uint64_t{1} << 5);
  }
  if (flags.is_enabled != kFlutterTristateNone) {
    bitfield |= (uint64_t{1} << 6);
  }
  if (flags.is_enabled == kFlutterTristateTrue) {
    bitfield |= (uint64_t{1} << 7);
  }
  if (flags.is_in_mutually_exclusive_group) {
    bitfield |= (uint64_t{1} << 8);
  }
  if (flags.is_header) {
    bitfield |= (uint64_t{1} << 9);
  }
  if (flags.is_obscured) {
    bitfield |= (uint64_t{1} << 10);
  }
  if (flags.scopes_route) {
    bitfield |= (uint64_t{1} << 11);
  }
  if (flags.names_route) {
    bitfield |= (uint64_t{1} << 12);
  }
  if (flags.is_hidden) {
    bitfield |= (uint64_t{1} << 13);
  }
  if (flags.is_image) {
    bitfield |= (uint64_t{1} << 14);
  }
  if (flags.is_live_region) {
    bitfield |= (uint64_t{1} << 15);
  }
  if (flags.is_toggled != kFlutterTristateNone) {
    bitfield |= (uint64_t{1} << 16);
  }
  if (flags.is_toggled == kFlutterTristateTrue) {
    bitfield |= (uint64_t{1} << 17);
  }
  if (flags.has_implicit_scrolling) {
    bitfield |= (uint64_t{1} << 18);
  }
  if (flags.is_multiline) {
    bitfield |= (uint64_t{1} << 19);
  }
  if (flags.is_read_only) {
    bitfield |= (uint64_t{1} << 20);
  }
  if (flags.is_focused != kFlutterTristateNone) {
    bitfield |= (uint64_t{1} << 21);
  }
  if (flags.is_link) {
    bitfield |= (uint64_t{1} << 22);
  }
  if (flags.is_slider) {
    bitfield |= (uint64_t{1} << 23);
  }
  if (flags.is_keyboard_key) {
    bitfield |= (uint64_t{1} << 24);
  }
  if (flags.is_checked == kFlutterCheckStateMixed) {
    bitfield |= (uint64_t{1} << 25);
  }
  if (flags.is_expanded != kFlutterTristateNone) {
    bitfield |= (uint64_t{1} << 26);
  }
  if (flags.is_expanded == kFlutterTristateTrue) {
    bitfield |= (uint64_t{1} << 27);
  }
  if (flags.is_selected != kFlutterTristateNone) {
    bitfield |= (uint64_t{1} << 28);
  }
  if (flags.is_required != kFlutterTristateNone) {
    bitfield |= (uint64_t{1} << 29);
  }
  if (flags.is_required == kFlutterTristateTrue) {
    bitfield |= (uint64_t{1} << 30);
  }
  std::memcpy(dst, &bitfield, 2 * sizeof(int32_t));
}

}  // namespace

TEST(PlatformViewShell, UpdateSemanticsDoesFlutterViewUpdateSemantics) {
  auto jni_mock = std::make_shared<JNIMock>();
  auto delegate = std::make_unique<PlatformViewAndroidDelegate>(jni_mock);

  FlutterSemanticsFlags flags = {};
  flags.struct_size = sizeof(FlutterSemanticsFlags);
  FlutterSemanticsNode2 node0 = {};
  node0.struct_size = sizeof(FlutterSemanticsNode2);
  node0.id = 0;
  node0.flags2 = &flags;
  node0.identifier = "identifier";
  node0.label = "label";
  node0.tooltip = "tooltip";
  node0.traversal_parent = -1;
  FlutterSemanticsNode2* node_ptrs[] = {&node0};
  FlutterSemanticsUpdate2 update = {};
  update.struct_size = sizeof(FlutterSemanticsUpdate2);
  update.node_count = 1;
  update.nodes = node_ptrs;

  std::vector<uint8_t> expected_buffer(
      PlatformViewAndroidDelegate::kBytesPerNode);
  std::vector<std::vector<uint8_t>> expected_string_attribute_args(0);
  size_t position = 0;
  int32_t* buffer_int32 = reinterpret_cast<int32_t*>(&expected_buffer[0]);
  float* buffer_float32 = reinterpret_cast<float*>(&expected_buffer[0]);
  std::vector<std::string> expected_strings;
  buffer_int32[position++] = node0.id;
  WriteFlagsBitfield(flags, &buffer_int32[position]);
  position += 2;
  buffer_int32[position++] = static_cast<int32_t>(node0.actions);
  buffer_int32[position++] = node0.max_value_length;
  buffer_int32[position++] = node0.current_value_length;
  buffer_int32[position++] = node0.text_selection_base;
  buffer_int32[position++] = node0.text_selection_extent;
  buffer_int32[position++] = node0.platform_view_id;
  buffer_int32[position++] = node0.scroll_child_count;
  buffer_int32[position++] = node0.scroll_index;
  buffer_int32[position++] = node0.traversal_parent;
  buffer_float32[position++] = static_cast<float>(node0.scroll_position);
  buffer_float32[position++] = static_cast<float>(node0.scroll_extent_max);
  buffer_float32[position++] = static_cast<float>(node0.scroll_extent_min);
  buffer_int32[position++] = static_cast<int32_t>(node0.role);
  buffer_int32[position++] = expected_strings.size();  // node0.identifier
  expected_strings.push_back(node0.identifier);
  buffer_int32[position++] = expected_strings.size();  // node0.label
  expected_strings.push_back(node0.label);
  buffer_int32[position++] = -1;  // node0.label_attributes
  buffer_int32[position++] = -1;  // node0.value
  buffer_int32[position++] = -1;  // node0.value_attributes
  buffer_int32[position++] = -1;  // node0.increased_value
  buffer_int32[position++] = -1;  // node0.increased_value_attributes
  buffer_int32[position++] = -1;  // node0.decreased_value
  buffer_int32[position++] = -1;  // node0.decreased_value_attributes
  buffer_int32[position++] = -1;  // node0.hint
  buffer_int32[position++] = -1;  // node0.hint_attributes
  buffer_int32[position++] = expected_strings.size();  // node0.tooltip
  expected_strings.push_back(node0.tooltip);
  buffer_int32[position++] = -1;  // node0.link_url
  buffer_int32[position++] = -1;  // node0.locale
  buffer_int32[position++] = -1;  // node0.min_value
  buffer_int32[position++] = -1;  // node0.max_value
  buffer_int32[position++] = node0.heading_level;
  buffer_int32[position++] = static_cast<int32_t>(node0.text_direction);
  buffer_float32[position++] = static_cast<float>(node0.rect.left);
  buffer_float32[position++] = static_cast<float>(node0.rect.top);
  buffer_float32[position++] = static_cast<float>(node0.rect.right);
  buffer_float32[position++] = static_cast<float>(node0.rect.bottom);
  WriteIdentityColMajor4x4(&buffer_float32[position]);
  position += 16;
  WriteIdentityColMajor4x4(&buffer_float32[position]);
  position += 16;
  buffer_int32[position++] = 0;  // node0.child_count
  buffer_int32[position++] = 0;  // node0.custom_accessibility_actions_count
  EXPECT_CALL(*jni_mock,
              FlutterViewUpdateSemantics(expected_buffer, expected_strings,
                                         expected_string_attribute_args));
  delegate->UpdateSemantics(update);
}

TEST(PlatformViewShell, UpdateSemanticsDoesUpdateLinkUrl) {
  auto jni_mock = std::make_shared<JNIMock>();
  auto delegate = std::make_unique<PlatformViewAndroidDelegate>(jni_mock);

  FlutterSemanticsFlags flags = {};
  flags.struct_size = sizeof(FlutterSemanticsFlags);
  FlutterSemanticsNode2 node0 = {};
  node0.struct_size = sizeof(FlutterSemanticsNode2);
  node0.id = 0;
  node0.flags2 = &flags;
  node0.identifier = "identifier";
  node0.label = "label";
  node0.link_url = "url";
  node0.traversal_parent = -1;
  FlutterSemanticsNode2* node_ptrs[] = {&node0};
  FlutterSemanticsUpdate2 update = {};
  update.struct_size = sizeof(FlutterSemanticsUpdate2);
  update.node_count = 1;
  update.nodes = node_ptrs;

  std::vector<uint8_t> expected_buffer(
      PlatformViewAndroidDelegate::kBytesPerNode);
  std::vector<std::vector<uint8_t>> expected_string_attribute_args(0);
  size_t position = 0;
  int32_t* buffer_int32 = reinterpret_cast<int32_t*>(&expected_buffer[0]);
  float* buffer_float32 = reinterpret_cast<float*>(&expected_buffer[0]);
  std::vector<std::string> expected_strings;
  buffer_int32[position++] = node0.id;
  WriteFlagsBitfield(flags, &buffer_int32[position]);
  position += 2;
  buffer_int32[position++] = static_cast<int32_t>(node0.actions);
  buffer_int32[position++] = node0.max_value_length;
  buffer_int32[position++] = node0.current_value_length;
  buffer_int32[position++] = node0.text_selection_base;
  buffer_int32[position++] = node0.text_selection_extent;
  buffer_int32[position++] = node0.platform_view_id;
  buffer_int32[position++] = node0.scroll_child_count;
  buffer_int32[position++] = node0.scroll_index;
  buffer_int32[position++] = node0.traversal_parent;
  buffer_float32[position++] = static_cast<float>(node0.scroll_position);
  buffer_float32[position++] = static_cast<float>(node0.scroll_extent_max);
  buffer_float32[position++] = static_cast<float>(node0.scroll_extent_min);
  buffer_int32[position++] = static_cast<int32_t>(node0.role);
  buffer_int32[position++] = expected_strings.size();  // node0.identifier
  expected_strings.push_back(node0.identifier);
  buffer_int32[position++] = expected_strings.size();  // node0.label
  expected_strings.push_back(node0.label);
  buffer_int32[position++] = -1;  // node0.label_attributes
  buffer_int32[position++] = -1;  // node0.value
  buffer_int32[position++] = -1;  // node0.value_attributes
  buffer_int32[position++] = -1;  // node0.increased_value
  buffer_int32[position++] = -1;  // node0.increased_value_attributes
  buffer_int32[position++] = -1;  // node0.decreased_value
  buffer_int32[position++] = -1;  // node0.decreased_value_attributes
  buffer_int32[position++] = -1;  // node0.hint
  buffer_int32[position++] = -1;  // node0.hint_attributes
  buffer_int32[position++] = -1;  // node0.tooltip
  buffer_int32[position++] = expected_strings.size();  // node0.link_url
  expected_strings.push_back(node0.link_url);
  buffer_int32[position++] = -1;  // node0.locale
  buffer_int32[position++] = -1;  // node0.min_value
  buffer_int32[position++] = -1;  // node0.max_value
  buffer_int32[position++] = node0.heading_level;
  buffer_int32[position++] = static_cast<int32_t>(node0.text_direction);
  buffer_float32[position++] = static_cast<float>(node0.rect.left);
  buffer_float32[position++] = static_cast<float>(node0.rect.top);
  buffer_float32[position++] = static_cast<float>(node0.rect.right);
  buffer_float32[position++] = static_cast<float>(node0.rect.bottom);
  WriteIdentityColMajor4x4(&buffer_float32[position]);
  position += 16;
  WriteIdentityColMajor4x4(&buffer_float32[position]);
  position += 16;
  buffer_int32[position++] = 0;  // node0.child_count
  buffer_int32[position++] = 0;  // node0.custom_accessibility_actions_count
  EXPECT_CALL(*jni_mock,
              FlutterViewUpdateSemantics(expected_buffer, expected_strings,
                                         expected_string_attribute_args));
  delegate->UpdateSemantics(update);
}

TEST(PlatformViewShell, UpdateSemanticsDoesUpdateLocale) {
  auto jni_mock = std::make_shared<JNIMock>();
  auto delegate = std::make_unique<PlatformViewAndroidDelegate>(jni_mock);

  FlutterSemanticsFlags flags = {};
  flags.struct_size = sizeof(FlutterSemanticsFlags);
  FlutterSemanticsNode2 node0 = {};
  node0.struct_size = sizeof(FlutterSemanticsNode2);
  node0.id = 0;
  node0.flags2 = &flags;
  node0.identifier = "identifier";
  node0.label = "label";
  node0.locale = "es-MX";
  node0.traversal_parent = -1;
  FlutterSemanticsNode2* node_ptrs[] = {&node0};
  FlutterSemanticsUpdate2 update = {};
  update.struct_size = sizeof(FlutterSemanticsUpdate2);
  update.node_count = 1;
  update.nodes = node_ptrs;

  std::vector<uint8_t> expected_buffer(
      PlatformViewAndroidDelegate::kBytesPerNode);
  std::vector<std::vector<uint8_t>> expected_string_attribute_args(0);
  size_t position = 0;
  int32_t* buffer_int32 = reinterpret_cast<int32_t*>(&expected_buffer[0]);
  float* buffer_float32 = reinterpret_cast<float*>(&expected_buffer[0]);
  std::vector<std::string> expected_strings;
  buffer_int32[position++] = node0.id;
  WriteFlagsBitfield(flags, &buffer_int32[position]);
  position += 2;
  buffer_int32[position++] = static_cast<int32_t>(node0.actions);
  buffer_int32[position++] = node0.max_value_length;
  buffer_int32[position++] = node0.current_value_length;
  buffer_int32[position++] = node0.text_selection_base;
  buffer_int32[position++] = node0.text_selection_extent;
  buffer_int32[position++] = node0.platform_view_id;
  buffer_int32[position++] = node0.scroll_child_count;
  buffer_int32[position++] = node0.scroll_index;
  buffer_int32[position++] = node0.traversal_parent;
  buffer_float32[position++] = static_cast<float>(node0.scroll_position);
  buffer_float32[position++] = static_cast<float>(node0.scroll_extent_max);
  buffer_float32[position++] = static_cast<float>(node0.scroll_extent_min);
  buffer_int32[position++] = static_cast<int32_t>(node0.role);
  buffer_int32[position++] = expected_strings.size();  // node0.identifier
  expected_strings.push_back(node0.identifier);
  buffer_int32[position++] = expected_strings.size();  // node0.label
  expected_strings.push_back(node0.label);
  buffer_int32[position++] = -1;  // node0.label_attributes
  buffer_int32[position++] = -1;  // node0.value
  buffer_int32[position++] = -1;  // node0.value_attributes
  buffer_int32[position++] = -1;  // node0.increased_value
  buffer_int32[position++] = -1;  // node0.increased_value_attributes
  buffer_int32[position++] = -1;  // node0.decreased_value
  buffer_int32[position++] = -1;  // node0.decreased_value_attributes
  buffer_int32[position++] = -1;  // node0.hint
  buffer_int32[position++] = -1;  // node0.hint_attributes
  buffer_int32[position++] = -1;  // node0.tooltip
  buffer_int32[position++] = -1;  // node0.link_url
  buffer_int32[position++] = expected_strings.size();
  expected_strings.push_back(node0.locale);  // node0.locale
  buffer_int32[position++] = -1;             // node0.min_value
  buffer_int32[position++] = -1;             // node0.max_value
  buffer_int32[position++] = node0.heading_level;
  buffer_int32[position++] = static_cast<int32_t>(node0.text_direction);
  buffer_float32[position++] = static_cast<float>(node0.rect.left);
  buffer_float32[position++] = static_cast<float>(node0.rect.top);
  buffer_float32[position++] = static_cast<float>(node0.rect.right);
  buffer_float32[position++] = static_cast<float>(node0.rect.bottom);
  WriteIdentityColMajor4x4(&buffer_float32[position]);
  position += 16;
  WriteIdentityColMajor4x4(&buffer_float32[position]);
  position += 16;
  buffer_int32[position++] = 0;  // node0.child_count
  buffer_int32[position++] = 0;  // node0.custom_accessibility_actions_count
  EXPECT_CALL(*jni_mock,
              FlutterViewUpdateSemantics(expected_buffer, expected_strings,
                                         expected_string_attribute_args));
  delegate->UpdateSemantics(update);
}

TEST(PlatformViewShell,
     UpdateSemanticsDoesFlutterViewUpdateSemanticsWithStringAttribtes) {
  auto jni_mock = std::make_shared<JNIMock>();
  auto delegate = std::make_unique<PlatformViewAndroidDelegate>(jni_mock);

  FlutterSpellOutStringAttribute spell_out_detail = {};
  spell_out_detail.struct_size = sizeof(FlutterSpellOutStringAttribute);
  FlutterStringAttribute spell_out_attribute = {};
  spell_out_attribute.struct_size = sizeof(FlutterStringAttribute);
  spell_out_attribute.start = 2;
  spell_out_attribute.end = 4;
  spell_out_attribute.type = FlutterStringAttributeType::kSpellOut;
  spell_out_attribute.spell_out = &spell_out_detail;

  const std::string kLocaleStr = "en-US";
  FlutterLocaleStringAttribute locale_detail = {};
  locale_detail.struct_size = sizeof(FlutterLocaleStringAttribute);
  locale_detail.locale = kLocaleStr.c_str();
  FlutterStringAttribute locale_attribute = {};
  locale_attribute.struct_size = sizeof(FlutterStringAttribute);
  locale_attribute.start = 1;
  locale_attribute.end = 3;
  locale_attribute.type = FlutterStringAttributeType::kLocale;
  locale_attribute.locale = &locale_detail;

  const FlutterStringAttribute* label_attrs[] = {&spell_out_attribute};
  const FlutterStringAttribute* hint_attrs[] = {&locale_attribute};

  FlutterSemanticsFlags flags = {};
  flags.struct_size = sizeof(FlutterSemanticsFlags);
  FlutterSemanticsNode2 node0 = {};
  node0.struct_size = sizeof(FlutterSemanticsNode2);
  node0.id = 0;
  node0.flags2 = &flags;
  node0.identifier = "identifier";
  node0.label = "label";
  node0.label_attribute_count = 1;
  node0.label_attributes = label_attrs;
  node0.hint = "hint";
  node0.hint_attribute_count = 1;
  node0.hint_attributes = hint_attrs;
  node0.traversal_parent = -1;
  FlutterSemanticsNode2* node_ptrs[] = {&node0};
  FlutterSemanticsUpdate2 update = {};
  update.struct_size = sizeof(FlutterSemanticsUpdate2);
  update.node_count = 1;
  update.nodes = node_ptrs;

  std::vector<uint8_t> expected_buffer(
      PlatformViewAndroidDelegate::kBytesPerNode +
      // 1 label attribute + 1 hint attribute.
      2 * PlatformViewAndroidDelegate::kBytesPerStringAttribute);
  std::vector<std::vector<uint8_t>> expected_string_attribute_args;
  size_t position = 0;
  int32_t* buffer_int32 = reinterpret_cast<int32_t*>(&expected_buffer[0]);
  float* buffer_float32 = reinterpret_cast<float*>(&expected_buffer[0]);
  std::vector<std::string> expected_strings;
  buffer_int32[position++] = node0.id;
  WriteFlagsBitfield(flags, &buffer_int32[position]);
  position += 2;
  buffer_int32[position++] = static_cast<int32_t>(node0.actions);
  buffer_int32[position++] = node0.max_value_length;
  buffer_int32[position++] = node0.current_value_length;
  buffer_int32[position++] = node0.text_selection_base;
  buffer_int32[position++] = node0.text_selection_extent;
  buffer_int32[position++] = node0.platform_view_id;
  buffer_int32[position++] = node0.scroll_child_count;
  buffer_int32[position++] = node0.scroll_index;
  buffer_int32[position++] = node0.traversal_parent;
  buffer_float32[position++] = static_cast<float>(node0.scroll_position);
  buffer_float32[position++] = static_cast<float>(node0.scroll_extent_max);
  buffer_float32[position++] = static_cast<float>(node0.scroll_extent_min);
  buffer_int32[position++] = static_cast<int32_t>(node0.role);
  buffer_int32[position++] = expected_strings.size();  // node0.identifier
  expected_strings.push_back(node0.identifier);
  buffer_int32[position++] = expected_strings.size();  // node0.label
  expected_strings.push_back(node0.label);
  buffer_int32[position++] = 1;   // node0.label_attributes
  buffer_int32[position++] = 2;   // node0.label_attributes[0].start
  buffer_int32[position++] = 4;   // node0.label_attributes[0].end
  buffer_int32[position++] = 0;   // node0.label_attributes[0].type
  buffer_int32[position++] = -1;  // node0.label_attributes[0].args
  buffer_int32[position++] = -1;  // node0.value
  buffer_int32[position++] = -1;  // node0.value_attributes
  buffer_int32[position++] = -1;  // node0.increased_value
  buffer_int32[position++] = -1;  // node0.increased_value_attributes
  buffer_int32[position++] = -1;  // node0.decreased_value
  buffer_int32[position++] = -1;  // node0.decreased_value_attributes
  buffer_int32[position++] = expected_strings.size();  // node0.hint
  expected_strings.push_back(node0.hint);
  buffer_int32[position++] = 1;  // node0.hint_attributes
  buffer_int32[position++] = 1;  // node0.hint_attributes[0].start
  buffer_int32[position++] = 3;  // node0.hint_attributes[0].end
  buffer_int32[position++] = 1;  // node0.hint_attributes[0].type
  buffer_int32[position++] =
      expected_string_attribute_args.size();  // node0.hint_attributes[0].args
  expected_string_attribute_args.push_back(
      {kLocaleStr.begin(), kLocaleStr.end()});
  buffer_int32[position++] = -1;  // node0.tooltip
  buffer_int32[position++] = -1;  // node0.link_url
  buffer_int32[position++] = -1;  // node0.locale
  buffer_int32[position++] = -1;  // node0.min_value
  buffer_int32[position++] = -1;  // node0.max_value
  buffer_int32[position++] = node0.heading_level;
  buffer_int32[position++] = static_cast<int32_t>(node0.text_direction);
  buffer_float32[position++] = static_cast<float>(node0.rect.left);
  buffer_float32[position++] = static_cast<float>(node0.rect.top);
  buffer_float32[position++] = static_cast<float>(node0.rect.right);
  buffer_float32[position++] = static_cast<float>(node0.rect.bottom);
  WriteIdentityColMajor4x4(&buffer_float32[position]);
  position += 16;
  WriteIdentityColMajor4x4(&buffer_float32[position]);
  position += 16;
  buffer_int32[position++] = 0;  // node0.child_count
  buffer_int32[position++] = 0;  // node0.custom_accessibility_actions_count
  EXPECT_CALL(*jni_mock,
              FlutterViewUpdateSemantics(expected_buffer, expected_strings,
                                         expected_string_attribute_args));
  delegate->UpdateSemantics(update);
}

TEST(PlatformViewShell,
     UpdateSemanticsDoesFlutterViewUpdateCustomAccessibilityActions) {
  auto jni_mock = std::make_shared<JNIMock>();
  auto delegate = std::make_unique<PlatformViewAndroidDelegate>(jni_mock);

  FlutterSemanticsCustomAction2 action0 = {};
  action0.struct_size = sizeof(FlutterSemanticsCustomAction2);
  action0.id = 0;
  action0.override_action = static_cast<FlutterSemanticsAction>(1);
  action0.label = "label";
  action0.hint = "hint";
  FlutterSemanticsCustomAction2* action_ptrs[] = {&action0};
  FlutterSemanticsUpdate2 update = {};
  update.struct_size = sizeof(FlutterSemanticsUpdate2);
  update.custom_action_count = 1;
  update.custom_actions = action_ptrs;

  std::vector<uint8_t> expected_actions_buffer(
      PlatformViewAndroidDelegate::kBytesPerAction);
  int32_t* actions_buffer_int32 =
      reinterpret_cast<int32_t*>(&expected_actions_buffer[0]);
  std::vector<std::string> expected_action_strings;
  actions_buffer_int32[0] = action0.id;
  actions_buffer_int32[1] = static_cast<int32_t>(action0.override_action);
  actions_buffer_int32[2] = expected_action_strings.size();
  expected_action_strings.push_back(action0.label);
  actions_buffer_int32[3] = expected_action_strings.size();
  expected_action_strings.push_back(action0.hint);

  EXPECT_CALL(*jni_mock, FlutterViewUpdateCustomAccessibilityActions(
                             expected_actions_buffer, expected_action_strings));
  delegate->UpdateSemantics(update);
}

TEST(PlatformViewShell, UpdateSemanticsEmptyDoesNotCrash) {
  auto jni_mock = std::make_shared<JNIMock>();
  auto delegate = std::make_unique<PlatformViewAndroidDelegate>(jni_mock);

  EXPECT_CALL(*jni_mock, FlutterViewUpdateCustomAccessibilityActions(
                             ::testing::_, ::testing::_))
      .Times(0);
  EXPECT_CALL(*jni_mock, FlutterViewUpdateSemantics(::testing::_, ::testing::_,
                                                    ::testing::_))
      .Times(0);

  FlutterSemanticsUpdate2 update = {};
  update.struct_size = sizeof(FlutterSemanticsUpdate2);
  delegate->UpdateSemantics(update);
}

}  // namespace testing
}  // namespace flutter
