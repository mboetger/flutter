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
  node0.transform = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  node0.hit_test_transform = {1, 0, 0, 0, 1, 0, 0, 0, 1};

  const FlutterSemanticsNode2* nodes[] = {&node0};
  FlutterSemanticsUpdate2 update = {};
  update.struct_size = sizeof(FlutterSemanticsUpdate2);
  update.node_count = 1;
  update.nodes = const_cast<FlutterSemanticsNode2**>(nodes);

  std::vector<uint8_t> expected_buffer(
      PlatformViewAndroidDelegate::kBytesPerNode);
  std::vector<std::vector<uint8_t>> expected_string_attribute_args(0);
  size_t position = 0;
  int32_t* buffer_int32 = reinterpret_cast<int32_t*>(&expected_buffer[0]);
  float* buffer_float32 = reinterpret_cast<float*>(&expected_buffer[0]);
  std::vector<std::string> expected_strings;
  buffer_int32[position++] = node0.id;
  int64_t expected_flags = 0;
  std::memcpy(&buffer_int32[position], &expected_flags, 8);
  position += 2;
  buffer_int32[position++] = node0.actions;
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
  buffer_int32[position++] = node0.text_direction;
  buffer_float32[position++] = node0.rect.left;
  buffer_float32[position++] = node0.rect.top;
  buffer_float32[position++] = node0.rect.right;
  buffer_float32[position++] = node0.rect.bottom;

  // 4x4 identity matrix in col-major order
  buffer_float32[position++] = 1.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 1.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 1.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 1.0f;

  buffer_float32[position++] = 1.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 1.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 1.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 0.0f;
  buffer_float32[position++] = 1.0f;

  buffer_int32[position++] = 0;  // node0.children_in_traversal_order
  buffer_int32[position++] = 0;  // node0.children_in_hit_test_order
  buffer_int32[position++] = 0;  // node0.custom_accessibility_actions

  EXPECT_CALL(*jni_mock,
              FlutterViewUpdateSemantics(expected_buffer, expected_strings,
                                         expected_string_attribute_args));

  delegate->UpdateSemantics(&update);
}

TEST(PlatformViewShell, UpdateSemanticsWithStringAttributes) {
  auto jni_mock = std::make_shared<JNIMock>();
  auto delegate = std::make_unique<PlatformViewAndroidDelegate>(jni_mock);

  FlutterSemanticsFlags flags = {};
  flags.struct_size = sizeof(FlutterSemanticsFlags);

  FlutterStringAttribute spell_out_attr = {};
  spell_out_attr.struct_size = sizeof(FlutterStringAttribute);
  spell_out_attr.start = 0;
  spell_out_attr.end = 5;
  spell_out_attr.type = FlutterStringAttributeType::kSpellOut;

  FlutterLocaleStringAttribute locale_attr_data = {};
  locale_attr_data.struct_size = sizeof(FlutterLocaleStringAttribute);
  locale_attr_data.locale = "en-US";

  FlutterStringAttribute locale_attr = {};
  locale_attr.struct_size = sizeof(FlutterStringAttribute);
  locale_attr.start = 6;
  locale_attr.end = 10;
  locale_attr.type = FlutterStringAttributeType::kLocale;
  locale_attr.locale = &locale_attr_data;

  const FlutterStringAttribute* label_attrs[] = {&spell_out_attr, &locale_attr};

  FlutterSemanticsNode2 node0 = {};
  node0.struct_size = sizeof(FlutterSemanticsNode2);
  node0.id = 0;
  node0.flags2 = &flags;
  node0.label = "hello world";
  node0.label_attributes = label_attrs;
  node0.label_attribute_count = 2;
  node0.transform = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  node0.hit_test_transform = {1, 0, 0, 0, 1, 0, 0, 0, 1};

  const FlutterSemanticsNode2* nodes[] = {&node0};
  FlutterSemanticsUpdate2 update = {};
  update.struct_size = sizeof(FlutterSemanticsUpdate2);
  update.node_count = 1;
  update.nodes = const_cast<FlutterSemanticsNode2**>(nodes);

  EXPECT_CALL(*jni_mock, FlutterViewUpdateSemantics(::testing::_, ::testing::_,
                                                    ::testing::_))
      .WillOnce(
          [](const std::vector<uint8_t>& buffer,
             const std::vector<std::string>& strings,
             const std::vector<std::vector<uint8_t>>& string_attribute_args) {
            EXPECT_FALSE(buffer.empty());
            EXPECT_EQ(strings.size(), 1u);
            EXPECT_EQ(strings[0], "hello world");
            EXPECT_EQ(string_attribute_args.size(), 1u);
            std::string loc(string_attribute_args[0].begin(),
                            string_attribute_args[0].end());
            EXPECT_EQ(loc, "en-US");
          });

  delegate->UpdateSemantics(&update);
}

TEST(PlatformViewShell, UpdateSemanticsWithCustomAccessibilityActions) {
  auto jni_mock = std::make_shared<JNIMock>();
  auto delegate = std::make_unique<PlatformViewAndroidDelegate>(jni_mock);

  FlutterSemanticsCustomAction2 action0 = {};
  action0.struct_size = sizeof(FlutterSemanticsCustomAction2);
  action0.id = 42;
  action0.override_action = kFlutterSemanticsActionTap;
  action0.label = "custom tap";
  action0.hint = "custom hint";

  const FlutterSemanticsCustomAction2* actions[] = {&action0};
  FlutterSemanticsUpdate2 update = {};
  update.struct_size = sizeof(FlutterSemanticsUpdate2);
  update.node_count = 0;
  update.nodes = nullptr;
  update.custom_action_count = 1;
  update.custom_actions = const_cast<FlutterSemanticsCustomAction2**>(actions);

  EXPECT_CALL(*jni_mock, FlutterViewUpdateCustomAccessibilityActions(
                             ::testing::_, ::testing::_))
      .WillOnce([](const std::vector<uint8_t>& actions_buffer,
                   const std::vector<std::string>& action_strings) {
        EXPECT_EQ(actions_buffer.size(), 4 * sizeof(int32_t));
        EXPECT_EQ(action_strings.size(), 2u);
        EXPECT_EQ(action_strings[0], "custom tap");
        EXPECT_EQ(action_strings[1], "custom hint");
      });

  delegate->UpdateSemantics(&update);
}

TEST(PlatformViewShell, UpdateSemanticsNullOrEmptyDoesNotCrash) {
  auto jni_mock = std::make_shared<JNIMock>();
  auto delegate = std::make_unique<PlatformViewAndroidDelegate>(jni_mock);

  // Null update
  delegate->UpdateSemantics(nullptr);

  // Empty update
  FlutterSemanticsUpdate2 empty_update = {};
  empty_update.struct_size = sizeof(FlutterSemanticsUpdate2);
  delegate->UpdateSemantics(&empty_update);
}

TEST(PlatformViewShell, UpdateSemanticsWithChildrenArrays) {
  auto jni_mock = std::make_shared<JNIMock>();
  auto delegate = std::make_unique<PlatformViewAndroidDelegate>(jni_mock);

  FlutterSemanticsFlags flags = {};
  flags.struct_size = sizeof(FlutterSemanticsFlags);

  int32_t traversal_children[] = {1, 2, 3};
  int32_t hit_test_children[] = {3, 2, 1};
  int32_t custom_actions[] = {10, 20};

  FlutterSemanticsNode2 node0 = {};
  node0.struct_size = sizeof(FlutterSemanticsNode2);
  node0.id = 0;
  node0.flags2 = &flags;
  node0.transform = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  node0.hit_test_transform = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  node0.child_count = 3;
  node0.children_in_traversal_order = traversal_children;
  node0.children_in_hit_test_order = hit_test_children;
  node0.custom_accessibility_actions_count = 2;
  node0.custom_accessibility_actions = custom_actions;

  const FlutterSemanticsNode2* nodes[] = {&node0};
  FlutterSemanticsUpdate2 update = {};
  update.struct_size = sizeof(FlutterSemanticsUpdate2);
  update.node_count = 1;
  update.nodes = const_cast<FlutterSemanticsNode2**>(nodes);

  EXPECT_CALL(*jni_mock, FlutterViewUpdateSemantics(::testing::_, ::testing::_,
                                                    ::testing::_))
      .WillOnce(
          [](const std::vector<uint8_t>& buffer,
             const std::vector<std::string>& strings,
             const std::vector<std::vector<uint8_t>>& string_attribute_args) {
            EXPECT_FALSE(buffer.empty());
          });

  delegate->UpdateSemantics(&update);
}

}  // namespace testing
}  // namespace flutter
