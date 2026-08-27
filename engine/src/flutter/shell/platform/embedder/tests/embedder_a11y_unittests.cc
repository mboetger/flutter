// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Allow access to fml::MessageLoop::GetCurrent() in order to flush platform
// thread tasks.
#define FML_USED_ON_EMBEDDER

#include <functional>

#include "flutter/fml/macros.h"
#include "flutter/fml/message_loop.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/lib/ui/semantics/semantics_node.h"
#include "flutter/shell/platform/embedder/embedder.h"
#include "flutter/shell/platform/embedder/embedder_semantics_update.h"
#include "flutter/shell/platform/embedder/embedder_struct_macros.h"
#include "flutter/shell/platform/embedder/tests/embedder_config_builder.h"
#include "flutter/testing/testing.h"
#include "third_party/tonic/converter/dart_converter.h"

#include "gmock/gmock.h"  // For EXPECT_THAT and matchers
#include "gtest/gtest.h"

// CREATE_FFI_LAMBDA is leaky by design
// NOLINTBEGIN(clang-analyzer-core.StackAddressEscape)

namespace flutter {
namespace testing {

using EmbedderA11yTest = testing::EmbedderTest;
using ::testing::ElementsAre;

#if !defined(OS_FUCHSIA) || (FLUTTER_RUNTIME_MODE == FLUTTER_RUNTIME_MODE_DEBUG)
constexpr static char kTooltip[] = "tooltip";
#endif

TEST_F(EmbedderTest, CannotProvideMultipleSemanticsCallbacks) {
#if defined(OS_FUCHSIA) && (FLUTTER_RUNTIME_MODE != FLUTTER_RUNTIME_MODE_DEBUG)
  GTEST_SKIP() << "Dart_LoadELF is not implemented on Fuchsia.";
#else
  {
    auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
    EmbedderConfigBuilder builder(context);
    builder.SetSurface(DlISize(1, 1));
    builder.GetProjectArgs().update_semantics_callback =
        [](const FlutterSemanticsUpdate* update, void* user_data) {};
    builder.GetProjectArgs().update_semantics_callback2 =
        [](const FlutterSemanticsUpdate2* update, void* user_data) {};
    auto engine = builder.InitializeEngine();
    ASSERT_FALSE(engine.is_valid());
    engine.reset();
  }

  {
    auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
    EmbedderConfigBuilder builder(context);
    builder.SetSurface(DlISize(1, 1));
    builder.GetProjectArgs().update_semantics_callback2 =
        [](const FlutterSemanticsUpdate2* update, void* user_data) {};
    builder.GetProjectArgs().update_semantics_node_callback =
        [](const FlutterSemanticsNode* update, void* user_data) {};
    builder.GetProjectArgs().update_semantics_custom_action_callback =
        [](const FlutterSemanticsCustomAction* update, void* user_data) {};
    auto engine = builder.InitializeEngine();
    ASSERT_FALSE(engine.is_valid());
    engine.reset();
  }

  {
    auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
    EmbedderConfigBuilder builder(context);
    builder.SetSurface(DlISize(1, 1));
    builder.GetProjectArgs().update_semantics_callback =
        [](const FlutterSemanticsUpdate* update, void* user_data) {};
    builder.GetProjectArgs().update_semantics_node_callback =
        [](const FlutterSemanticsNode* update, void* user_data) {};
    builder.GetProjectArgs().update_semantics_custom_action_callback =
        [](const FlutterSemanticsCustomAction* update, void* user_data) {};
    auto engine = builder.InitializeEngine();
    ASSERT_FALSE(engine.is_valid());
    engine.reset();
  }

  {
    auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();
    EmbedderConfigBuilder builder(context);
    builder.SetSurface(DlISize(1, 1));
    builder.GetProjectArgs().update_semantics_callback2 =
        [](const FlutterSemanticsUpdate2* update, void* user_data) {};
    builder.GetProjectArgs().update_semantics_callback =
        [](const FlutterSemanticsUpdate* update, void* user_data) {};
    builder.GetProjectArgs().update_semantics_node_callback =
        [](const FlutterSemanticsNode* update, void* user_data) {};
    builder.GetProjectArgs().update_semantics_custom_action_callback =
        [](const FlutterSemanticsCustomAction* update, void* user_data) {};
    auto engine = builder.InitializeEngine();
    ASSERT_FALSE(engine.is_valid());
    engine.reset();
  }
#endif
}

TEST_F(EmbedderA11yTest, A11yTreeIsConsistentUsingV3Callbacks) {
#if defined(OS_FUCHSIA) && (FLUTTER_RUNTIME_MODE != FLUTTER_RUNTIME_MODE_DEBUG)
  GTEST_SKIP() << "Dart_LoadELF is not implemented on Fuchsia.";
#else
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  fml::AutoResetWaitableEvent signal_native_latch;

  // Called by the Dart text fixture on the UI thread to signal that the C++
  // unittest should resume.
  context.AddFfiNativeCallback("SignalNativeTest",
                               CREATE_FFI_LAMBDA(([&signal_native_latch]() {
                                 signal_native_latch.Signal();
                               })));

  // Called by test fixture on UI thread to pass data back to this test.
  std::function<void(bool)> notify_semantics_enabled_callback;
  context.AddFfiNativeCallback(
      "NotifySemanticsEnabled",
      CREATE_FFI_LAMBDA(([&notify_semantics_enabled_callback](bool enabled) {
        ASSERT_NE(notify_semantics_enabled_callback, nullptr);
        notify_semantics_enabled_callback(enabled);
      })));

  std::function<void(bool)> notify_accessibility_features_callback;
  context.AddFfiNativeCallback(
      "NotifyAccessibilityFeatures",
      CREATE_FFI_LAMBDA(
          ([&notify_accessibility_features_callback](bool reduce_motion) {
            ASSERT_NE(notify_accessibility_features_callback, nullptr);
            notify_accessibility_features_callback(reduce_motion);
          })));

  std::function<void(int64_t, int64_t, std::vector<int64_t>)>
      notify_semantics_action_callback;
  context.AddFfiNativeCallback(
      "NotifySemanticsAction",
      CREATE_FFI_LAMBDA(([&notify_semantics_action_callback](
                             int64_t node_id, int64_t action,
                             Dart_Handle data_handle) {
        ASSERT_NE(notify_semantics_action_callback, nullptr);
        std::vector<int64_t> data =
            tonic::DartConverter<std::vector<int64_t>>::FromDart(data_handle);
        notify_semantics_action_callback(node_id, action, data);
      })));

  fml::AutoResetWaitableEvent semantics_update_latch;
  context.SetSemanticsUpdateCallback2(
      [&](const FlutterSemanticsUpdate2* update) {
        ASSERT_EQ(size_t(4), update->node_count);
        ASSERT_EQ(size_t(1), update->custom_action_count);

        for (size_t i = 0; i < update->node_count; i++) {
          const FlutterSemanticsNode2* node = update->nodes[i];

          ASSERT_EQ(1.0, node->transform.scaleX);
          ASSERT_EQ(2.0, node->transform.skewX);
          ASSERT_EQ(3.0, node->transform.transX);
          ASSERT_EQ(4.0, node->transform.skewY);
          ASSERT_EQ(5.0, node->transform.scaleY);
          ASSERT_EQ(6.0, node->transform.transY);
          ASSERT_EQ(7.0, node->transform.pers0);
          ASSERT_EQ(8.0, node->transform.pers1);
          ASSERT_EQ(9.0, node->transform.pers2);
          ASSERT_EQ(std::strncmp(kTooltip, node->tooltip, sizeof(kTooltip) - 1),
                    0);
          ASSERT_EQ(node->heading_level, 0);

          if (node->id == 128) {
            ASSERT_EQ(0x3f3, node->platform_view_id);
          } else {
            ASSERT_NE(kFlutterSemanticsNodeIdBatchEnd, node->id);
            ASSERT_EQ(0, node->platform_view_id);
          }
        }

        semantics_update_latch.Signal();
      });

  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("a11y_main");

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  // 1: Wait for initial notifySemanticsEnabled(false).
  fml::AutoResetWaitableEvent notify_semantics_enabled_latch;
  notify_semantics_enabled_callback = [&](bool enabled) {
    ASSERT_FALSE(enabled);
    notify_semantics_enabled_latch.Signal();
  };
  notify_semantics_enabled_latch.Wait();

  // Prepare notifyAccessibilityFeatures callback.
  fml::AutoResetWaitableEvent notify_features_latch;
  notify_accessibility_features_callback = [&](bool reduce_motion) {
    ASSERT_FALSE(reduce_motion);
    notify_features_latch.Signal();
  };

  // 2: Enable semantics. Wait for notifySemanticsEnabled(true).
  fml::AutoResetWaitableEvent notify_semantics_enabled_latch_2;
  notify_semantics_enabled_callback = [&](bool enabled) {
    ASSERT_TRUE(enabled);
    notify_semantics_enabled_latch_2.Signal();
  };
  auto result = FlutterEngineUpdateSemanticsEnabled(engine.get(), true);
  ASSERT_EQ(result, FlutterEngineResult::kSuccess);
  notify_semantics_enabled_latch_2.Wait();

  // 3: Wait for notifyAccessibilityFeatures (reduce_motion == false)
  notify_features_latch.Wait();

  // 4: Wait for notifyAccessibilityFeatures (reduce_motion == true)
  fml::AutoResetWaitableEvent notify_features_latch_2;
  notify_accessibility_features_callback = [&](bool reduce_motion) {
    ASSERT_TRUE(reduce_motion);
    notify_features_latch_2.Signal();
  };
  result = FlutterEngineUpdateAccessibilityFeatures(
      engine.get(), kFlutterAccessibilityFeatureReduceMotion);
  ASSERT_EQ(result, FlutterEngineResult::kSuccess);
  notify_features_latch_2.Wait();

  // 5: Wait for UpdateSemantics callback on platform (current) thread.
  signal_native_latch.Wait();
  fml::MessageLoop::GetCurrent().RunExpiredTasksNow();
  semantics_update_latch.Wait();

  // 6: Dispatch a tap to semantics node 42. Wait for NotifySemanticsAction.
  fml::AutoResetWaitableEvent notify_semantics_action_latch;
  notify_semantics_action_callback =
      [&](int64_t node_id, int64_t action_id,
          const std::vector<int64_t>& semantic_args) {
        ASSERT_EQ(42, node_id);
        ASSERT_EQ(static_cast<int32_t>(flutter::SemanticsAction::kTap),
                  action_id);
        ASSERT_THAT(semantic_args, ElementsAre(2, 1));
        notify_semantics_action_latch.Signal();
      };
  std::vector<uint8_t> bytes({2, 1});
  result = FlutterEngineDispatchSemanticsAction(
      engine.get(), 42, kFlutterSemanticsActionTap, &bytes[0], bytes.size());
  ASSERT_EQ(result, FlutterEngineResult::kSuccess);
  notify_semantics_action_latch.Wait();

  // 7: Disable semantics. Wait for NotifySemanticsEnabled(false).
  fml::AutoResetWaitableEvent notify_semantics_enabled_latch_3;
  notify_semantics_enabled_callback = [&](bool enabled) {
    ASSERT_FALSE(enabled);
    notify_semantics_enabled_latch_3.Signal();
  };
  result = FlutterEngineUpdateSemanticsEnabled(engine.get(), false);
  ASSERT_EQ(result, FlutterEngineResult::kSuccess);
  notify_semantics_enabled_latch_3.Wait();
#endif
}

TEST_F(EmbedderA11yTest, A11yStringAttributes) {
#if defined(OS_FUCHSIA) && (FLUTTER_RUNTIME_MODE != FLUTTER_RUNTIME_MODE_DEBUG)
  GTEST_SKIP() << "Dart_LoadELF is not implemented on Fuchsia.";
#else
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  fml::AutoResetWaitableEvent signal_native_latch;

  // Called by the Dart text fixture on the UI thread to signal that the C++
  // unittest should resume.
  context.AddFfiNativeCallback("SignalNativeTest",
                               CREATE_FFI_LAMBDA(([&signal_native_latch]() {
                                 signal_native_latch.Signal();
                               })));

  fml::AutoResetWaitableEvent semantics_update_latch;
  context.SetSemanticsUpdateCallback2(
      [&](const FlutterSemanticsUpdate2* update) {
        ASSERT_EQ(update->node_count, size_t(1));
        ASSERT_EQ(update->custom_action_count, size_t(0));

        auto node = update->nodes[0];

        // Verify identifier
        {
          ASSERT_EQ(std::string(node->identifier), "identifier");
        }

        // Verify label
        {
          ASSERT_EQ(std::string(node->label), "What is the meaning of life?");
          ASSERT_EQ(node->label_attribute_count, size_t(2));

          ASSERT_EQ(node->label_attributes[0]->start, size_t(0));
          ASSERT_EQ(node->label_attributes[0]->end, size_t(28));
          ASSERT_EQ(node->label_attributes[0]->type,
                    FlutterStringAttributeType::kLocale);
          ASSERT_EQ(std::string(node->label_attributes[0]->locale->locale),
                    "en");

          ASSERT_EQ(node->label_attributes[1]->start, size_t(0));
          ASSERT_EQ(node->label_attributes[1]->end, size_t(1));
          ASSERT_EQ(node->label_attributes[1]->type,
                    FlutterStringAttributeType::kSpellOut);
        }

        // Verify hint
        {
          ASSERT_EQ(std::string(node->hint), "It's a number");
          ASSERT_EQ(node->hint_attribute_count, size_t(2));

          ASSERT_EQ(node->hint_attributes[0]->start, size_t(0));
          ASSERT_EQ(node->hint_attributes[0]->end, size_t(1));
          ASSERT_EQ(node->hint_attributes[0]->type,
                    FlutterStringAttributeType::kLocale);
          ASSERT_EQ(std::string(node->hint_attributes[0]->locale->locale),
                    "en");

          ASSERT_EQ(node->hint_attributes[1]->start, size_t(2));
          ASSERT_EQ(node->hint_attributes[1]->end, size_t(3));
          ASSERT_EQ(node->hint_attributes[1]->type,
                    FlutterStringAttributeType::kLocale);
          ASSERT_EQ(std::string(node->hint_attributes[1]->locale->locale),
                    "fr");
        }

        // Verify value
        {
          ASSERT_EQ(std::string(node->value), "42");
          ASSERT_EQ(node->value_attribute_count, size_t(1));

          ASSERT_EQ(node->value_attributes[0]->start, size_t(0));
          ASSERT_EQ(node->value_attributes[0]->end, size_t(2));
          ASSERT_EQ(node->value_attributes[0]->type,
                    FlutterStringAttributeType::kLocale);
          ASSERT_EQ(std::string(node->value_attributes[0]->locale->locale),
                    "en-US");
        }

        // Verify increased value
        {
          ASSERT_EQ(std::string(node->increased_value), "43");
          ASSERT_EQ(node->increased_value_attribute_count, size_t(2));

          ASSERT_EQ(node->increased_value_attributes[0]->start, size_t(0));
          ASSERT_EQ(node->increased_value_attributes[0]->end, size_t(1));
          ASSERT_EQ(node->increased_value_attributes[0]->type,
                    FlutterStringAttributeType::kSpellOut);

          ASSERT_EQ(node->increased_value_attributes[1]->start, size_t(1));
          ASSERT_EQ(node->increased_value_attributes[1]->end, size_t(2));
          ASSERT_EQ(node->increased_value_attributes[1]->type,
                    FlutterStringAttributeType::kSpellOut);
        }

        // Verify decreased value
        {
          ASSERT_EQ(std::string(node->decreased_value), "41");
          ASSERT_EQ(node->decreased_value_attribute_count, size_t(0));
          ASSERT_EQ(node->decreased_value_attributes, nullptr);
        }

        semantics_update_latch.Signal();
      });

  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("a11y_string_attributes");

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  // 1: Enable semantics.
  auto result = FlutterEngineUpdateSemanticsEnabled(engine.get(), true);
  ASSERT_EQ(result, FlutterEngineResult::kSuccess);

  // 2: Wait for semantics update callback on platform (current) thread.
  signal_native_latch.Wait();
  fml::MessageLoop::GetCurrent().RunExpiredTasksNow();
  semantics_update_latch.Wait();
#endif
}

TEST_F(EmbedderA11yTest, A11yTreeIsConsistentUsingV2Callbacks) {
#if defined(OS_FUCHSIA) && (FLUTTER_RUNTIME_MODE != FLUTTER_RUNTIME_MODE_DEBUG)
  GTEST_SKIP() << "Dart_LoadELF is not implemented on Fuchsia.";
#else
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  fml::AutoResetWaitableEvent signal_native_latch;

  // Called by the Dart text fixture on the UI thread to signal that the C++
  // unittest should resume.
  context.AddFfiNativeCallback("SignalNativeTest",
                               CREATE_FFI_LAMBDA(([&signal_native_latch]() {
                                 signal_native_latch.Signal();
                               })));

  // Called by test fixture on UI thread to pass data back to this test.
  std::function<void(bool)> notify_semantics_enabled_callback;
  context.AddFfiNativeCallback(
      "NotifySemanticsEnabled",
      CREATE_FFI_LAMBDA(([&notify_semantics_enabled_callback](bool enabled) {
        ASSERT_NE(notify_semantics_enabled_callback, nullptr);
        notify_semantics_enabled_callback(enabled);
      })));

  std::function<void(bool)> notify_accessibility_features_callback;
  context.AddFfiNativeCallback(
      "NotifyAccessibilityFeatures",
      CREATE_FFI_LAMBDA(
          ([&notify_accessibility_features_callback](bool reduce_motion) {
            ASSERT_NE(notify_accessibility_features_callback, nullptr);
            notify_accessibility_features_callback(reduce_motion);
          })));

  std::function<void(int64_t, int64_t, std::vector<int64_t>)>
      notify_semantics_action_callback;
  context.AddFfiNativeCallback(
      "NotifySemanticsAction",
      CREATE_FFI_LAMBDA(([&notify_semantics_action_callback](
                             int64_t node_id, int64_t action,
                             Dart_Handle data_handle) {
        ASSERT_NE(notify_semantics_action_callback, nullptr);
        std::vector<int64_t> data =
            tonic::DartConverter<std::vector<int64_t>>::FromDart(data_handle);
        notify_semantics_action_callback(node_id, action, data);
      })));

  fml::AutoResetWaitableEvent semantics_update_latch;
  context.SetSemanticsUpdateCallback([&](const FlutterSemanticsUpdate* update) {
    ASSERT_EQ(size_t(4), update->nodes_count);
    ASSERT_EQ(size_t(1), update->custom_actions_count);

    for (size_t i = 0; i < update->nodes_count; i++) {
      const FlutterSemanticsNode* node = update->nodes + i;

      ASSERT_EQ(1.0, node->transform.scaleX);
      ASSERT_EQ(2.0, node->transform.skewX);
      ASSERT_EQ(3.0, node->transform.transX);
      ASSERT_EQ(4.0, node->transform.skewY);
      ASSERT_EQ(5.0, node->transform.scaleY);
      ASSERT_EQ(6.0, node->transform.transY);
      ASSERT_EQ(7.0, node->transform.pers0);
      ASSERT_EQ(8.0, node->transform.pers1);
      ASSERT_EQ(9.0, node->transform.pers2);
      ASSERT_EQ(std::strncmp(kTooltip, node->tooltip, sizeof(kTooltip) - 1), 0);
      ASSERT_EQ(node->heading_level, 0);

      if (node->id == 128) {
        ASSERT_EQ(0x3f3, node->platform_view_id);
      } else {
        ASSERT_NE(kFlutterSemanticsNodeIdBatchEnd, node->id);
        ASSERT_EQ(0, node->platform_view_id);
      }
    }

    semantics_update_latch.Signal();
  });

  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("a11y_main");

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  // 1: Wait for initial notifySemanticsEnabled(false).
  fml::AutoResetWaitableEvent notify_semantics_enabled_latch;
  notify_semantics_enabled_callback = [&](bool enabled) {
    ASSERT_FALSE(enabled);
    notify_semantics_enabled_latch.Signal();
  };
  notify_semantics_enabled_latch.Wait();

  // Prepare notifyAccessibilityFeatures callback.
  fml::AutoResetWaitableEvent notify_features_latch;
  notify_accessibility_features_callback = [&](bool reduce_motion) {
    ASSERT_FALSE(reduce_motion);
    notify_features_latch.Signal();
  };

  // 2: Enable semantics. Wait for notifySemanticsEnabled(true).
  fml::AutoResetWaitableEvent notify_semantics_enabled_latch_2;
  notify_semantics_enabled_callback = [&](bool enabled) {
    ASSERT_TRUE(enabled);
    notify_semantics_enabled_latch_2.Signal();
  };
  auto result = FlutterEngineUpdateSemanticsEnabled(engine.get(), true);
  ASSERT_EQ(result, FlutterEngineResult::kSuccess);
  notify_semantics_enabled_latch_2.Wait();

  // 3: Wait for notifyAccessibilityFeatures (reduce_motion == false)
  notify_features_latch.Wait();

  // 4: Wait for notifyAccessibilityFeatures (reduce_motion == true)
  fml::AutoResetWaitableEvent notify_features_latch_2;
  notify_accessibility_features_callback = [&](bool reduce_motion) {
    ASSERT_TRUE(reduce_motion);
    notify_features_latch_2.Signal();
  };
  result = FlutterEngineUpdateAccessibilityFeatures(
      engine.get(), kFlutterAccessibilityFeatureReduceMotion);
  ASSERT_EQ(result, FlutterEngineResult::kSuccess);
  notify_features_latch_2.Wait();

  // 5: Wait for UpdateSemantics callback on platform (current) thread.
  signal_native_latch.Wait();
  fml::MessageLoop::GetCurrent().RunExpiredTasksNow();
  semantics_update_latch.Wait();

  // 6: Dispatch a tap to semantics node 42. Wait for NotifySemanticsAction.
  fml::AutoResetWaitableEvent notify_semantics_action_latch;
  notify_semantics_action_callback =
      [&](int64_t node_id, int64_t action_id,
          const std::vector<int64_t>& semantic_args) {
        ASSERT_EQ(42, node_id);
        ASSERT_EQ(static_cast<int32_t>(flutter::SemanticsAction::kTap),
                  action_id);
        ASSERT_THAT(semantic_args, ElementsAre(2, 1));
        notify_semantics_action_latch.Signal();
      };
  std::vector<uint8_t> bytes({2, 1});
  result = FlutterEngineDispatchSemanticsAction(
      engine.get(), 42, kFlutterSemanticsActionTap, &bytes[0], bytes.size());
  ASSERT_EQ(result, FlutterEngineResult::kSuccess);
  notify_semantics_action_latch.Wait();

  // 7: Disable semantics. Wait for NotifySemanticsEnabled(false).
  fml::AutoResetWaitableEvent notify_semantics_enabled_latch_3;
  notify_semantics_enabled_callback = [&](bool enabled) {
    ASSERT_FALSE(enabled);
    notify_semantics_enabled_latch_3.Signal();
  };
  result = FlutterEngineUpdateSemanticsEnabled(engine.get(), false);
  ASSERT_EQ(result, FlutterEngineResult::kSuccess);
  notify_semantics_enabled_latch_3.Wait();
#endif
}

TEST_F(EmbedderA11yTest, A11yTreeIsConsistentUsingV1Callbacks) {
#if defined(OS_FUCHSIA) && (FLUTTER_RUNTIME_MODE != FLUTTER_RUNTIME_MODE_DEBUG)
  GTEST_SKIP() << "Dart_LoadELF is not implemented on Fuchsia.";
#else
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  fml::AutoResetWaitableEvent signal_native_latch;

  // Called by the Dart text fixture on the UI thread to signal that the C++
  // unittest should resume.
  context.AddFfiNativeCallback("SignalNativeTest",
                               CREATE_FFI_LAMBDA(([&signal_native_latch]() {
                                 signal_native_latch.Signal();
                               })));

  // Called by test fixture on UI thread to pass data back to this test.
  std::function<void(bool)> notify_semantics_enabled_callback;
  context.AddFfiNativeCallback(
      "NotifySemanticsEnabled",
      CREATE_FFI_LAMBDA(([&notify_semantics_enabled_callback](bool enabled) {
        ASSERT_NE(notify_semantics_enabled_callback, nullptr);
        notify_semantics_enabled_callback(enabled);
      })));

  std::function<void(bool)> notify_accessibility_features_callback;
  context.AddFfiNativeCallback(
      "NotifyAccessibilityFeatures",
      CREATE_FFI_LAMBDA(
          ([&notify_accessibility_features_callback](bool reduce_motion) {
            ASSERT_NE(notify_accessibility_features_callback, nullptr);
            notify_accessibility_features_callback(reduce_motion);
          })));

  std::function<void(int64_t, int64_t, std::vector<int64_t>)>
      notify_semantics_action_callback;
  context.AddFfiNativeCallback(
      "NotifySemanticsAction",
      CREATE_FFI_LAMBDA(([&notify_semantics_action_callback](
                             int64_t node_id, int64_t action,
                             Dart_Handle data_handle) {
        ASSERT_NE(notify_semantics_action_callback, nullptr);
        std::vector<int64_t> data =
            tonic::DartConverter<std::vector<int64_t>>::FromDart(data_handle);
        notify_semantics_action_callback(node_id, action, data);
      })));

  fml::AutoResetWaitableEvent semantics_node_latch;
  fml::AutoResetWaitableEvent semantics_action_latch;

  int node_batch_end_count = 0;
  int action_batch_end_count = 0;

  int node_count = 0;
  context.SetSemanticsNodeCallback([&](const FlutterSemanticsNode* node) {
    if (node->id == kFlutterSemanticsNodeIdBatchEnd) {
      ++node_batch_end_count;
      semantics_node_latch.Signal();
    } else {
      // Batches should be completed after all nodes are received.
      ASSERT_EQ(0, node_batch_end_count);
      ASSERT_EQ(0, action_batch_end_count);

      ++node_count;
      ASSERT_EQ(1.0, node->transform.scaleX);
      ASSERT_EQ(2.0, node->transform.skewX);
      ASSERT_EQ(3.0, node->transform.transX);
      ASSERT_EQ(4.0, node->transform.skewY);
      ASSERT_EQ(5.0, node->transform.scaleY);
      ASSERT_EQ(6.0, node->transform.transY);
      ASSERT_EQ(7.0, node->transform.pers0);
      ASSERT_EQ(8.0, node->transform.pers1);
      ASSERT_EQ(9.0, node->transform.pers2);
      ASSERT_EQ(std::strncmp(kTooltip, node->tooltip, sizeof(kTooltip) - 1), 0);

      if (node->id == 128) {
        ASSERT_EQ(0x3f3, node->platform_view_id);
      } else {
        ASSERT_EQ(0, node->platform_view_id);
      }
    }
  });

  int action_count = 0;
  context.SetSemanticsCustomActionCallback(
      [&](const FlutterSemanticsCustomAction* action) {
        if (action->id == kFlutterSemanticsCustomActionIdBatchEnd) {
          ++action_batch_end_count;
          semantics_action_latch.Signal();
        } else {
          // Batches should be completed after all actions are received.
          ASSERT_EQ(0, node_batch_end_count);
          ASSERT_EQ(0, action_batch_end_count);

          ++action_count;
        }
      });

  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("a11y_main");

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  // 1: Wait for initial notifySemanticsEnabled(false).
  fml::AutoResetWaitableEvent notify_semantics_enabled_latch;
  notify_semantics_enabled_callback = [&](bool enabled) {
    ASSERT_FALSE(enabled);
    notify_semantics_enabled_latch.Signal();
  };
  notify_semantics_enabled_latch.Wait();

  // Prepare notifyAccessibilityFeatures callback.
  fml::AutoResetWaitableEvent notify_features_latch;
  notify_accessibility_features_callback = [&](bool reduce_motion) {
    ASSERT_FALSE(reduce_motion);
    notify_features_latch.Signal();
  };

  // 2: Enable semantics. Wait for notifySemanticsEnabled(true).
  fml::AutoResetWaitableEvent notify_semantics_enabled_latch_2;
  notify_semantics_enabled_callback = [&](bool enabled) {
    ASSERT_TRUE(enabled);
    notify_semantics_enabled_latch_2.Signal();
  };
  auto result = FlutterEngineUpdateSemanticsEnabled(engine.get(), true);
  ASSERT_EQ(result, FlutterEngineResult::kSuccess);
  notify_semantics_enabled_latch_2.Wait();

  // 3: Wait for notifyAccessibilityFeatures (reduce_motion == false)
  notify_features_latch.Wait();

  // 4: Wait for notifyAccessibilityFeatures (reduce_motion == true)
  fml::AutoResetWaitableEvent notify_features_latch_2;
  notify_accessibility_features_callback = [&](bool reduce_motion) {
    ASSERT_TRUE(reduce_motion);
    notify_features_latch_2.Signal();
  };
  result = FlutterEngineUpdateAccessibilityFeatures(
      engine.get(), kFlutterAccessibilityFeatureReduceMotion);
  ASSERT_EQ(result, FlutterEngineResult::kSuccess);
  notify_features_latch_2.Wait();

  // 5: Wait for UpdateSemantics callback on platform (current) thread.
  signal_native_latch.Wait();
  fml::MessageLoop::GetCurrent().RunExpiredTasksNow();
  semantics_node_latch.Wait();
  semantics_action_latch.Wait();
  ASSERT_EQ(4, node_count);
  ASSERT_EQ(1, node_batch_end_count);
  ASSERT_EQ(1, action_count);
  ASSERT_EQ(1, action_batch_end_count);

  // 6: Dispatch a tap to semantics node 42. Wait for NotifySemanticsAction.
  fml::AutoResetWaitableEvent notify_semantics_action_latch;
  notify_semantics_action_callback =
      [&](int64_t node_id, int64_t action_id,
          const std::vector<int64_t>& semantic_args) {
        ASSERT_EQ(42, node_id);
        ASSERT_EQ(static_cast<int32_t>(flutter::SemanticsAction::kTap),
                  action_id);
        ASSERT_THAT(semantic_args, ElementsAre(2, 1));
        notify_semantics_action_latch.Signal();
      };
  std::vector<uint8_t> bytes({2, 1});
  result = FlutterEngineDispatchSemanticsAction(
      engine.get(), 42, kFlutterSemanticsActionTap, &bytes[0], bytes.size());
  ASSERT_EQ(result, FlutterEngineResult::kSuccess);
  notify_semantics_action_latch.Wait();

  // 7: Disable semantics. Wait for NotifySemanticsEnabled(false).
  fml::AutoResetWaitableEvent notify_semantics_enabled_latch_3;
  notify_semantics_enabled_callback = [&](bool enabled) {
    ASSERT_FALSE(enabled);
    notify_semantics_enabled_latch_3.Signal();
  };
  result = FlutterEngineUpdateSemanticsEnabled(engine.get(), false);
  ASSERT_EQ(result, FlutterEngineResult::kSuccess);
  notify_semantics_enabled_latch_3.Wait();
#endif
}

TEST_F(EmbedderA11yTest, A11yTreesAreConsistentWithMultipleViews) {
#if defined(OS_FUCHSIA) && (FLUTTER_RUNTIME_MODE != FLUTTER_RUNTIME_MODE_DEBUG)
  GTEST_SKIP() << "Dart_LoadELF is not implemented on Fuchsia.";
#else
  auto& context = GetEmbedderContext<EmbedderTestContextSoftware>();

  fml::AutoResetWaitableEvent signal_native_latch;

  // Called by the Dart text fixture on the UI thread to signal that the C++
  // unittest should resume.
  context.AddFfiNativeCallback("SignalNativeTest",
                               CREATE_FFI_LAMBDA(([&signal_native_latch]() {
                                 signal_native_latch.Signal();
                               })));

  // Called by test fixture on UI thread to pass data back to this test.
  std::function<void(bool)> notify_semantics_enabled_callback;
  context.AddFfiNativeCallback(
      "NotifySemanticsEnabled",
      CREATE_FFI_LAMBDA(([&notify_semantics_enabled_callback](bool enabled) {
        ASSERT_NE(notify_semantics_enabled_callback, nullptr);
        notify_semantics_enabled_callback(enabled);
      })));

  std::function<void(bool)> notify_accessibility_features_callback;
  context.AddFfiNativeCallback(
      "NotifyAccessibilityFeatures",
      CREATE_FFI_LAMBDA(
          ([&notify_accessibility_features_callback](bool reduce_motion) {
            ASSERT_NE(notify_accessibility_features_callback, nullptr);
            notify_accessibility_features_callback(reduce_motion);
          })));

  int num_times_set_semantics_update_callback2_called = 0;
  fml::AutoResetWaitableEvent semantics_update_latch;
  context.SetSemanticsUpdateCallback2(
      [&](const FlutterSemanticsUpdate2* update) {
        num_times_set_semantics_update_callback2_called++;
        ASSERT_EQ(size_t(1), update->node_count);

        for (size_t i = 0; i < update->node_count; i++) {
          const FlutterSemanticsNode2* node = update->nodes[i];

          // The node ID should be the view_id + 1
          ASSERT_EQ(node->id, update->view_id + 1);
          ASSERT_EQ(1.0, node->transform.scaleX);
          ASSERT_EQ(2.0, node->transform.skewX);
          ASSERT_EQ(3.0, node->transform.transX);
          ASSERT_EQ(4.0, node->transform.skewY);
          ASSERT_EQ(5.0, node->transform.scaleY);
          ASSERT_EQ(6.0, node->transform.transY);
          ASSERT_EQ(7.0, node->transform.pers0);
          ASSERT_EQ(8.0, node->transform.pers1);
          ASSERT_EQ(9.0, node->transform.pers2);
          ASSERT_EQ(std::strncmp(kTooltip, node->tooltip, sizeof(kTooltip) - 1),
                    0);
        }

        if (num_times_set_semantics_update_callback2_called == 3) {
          semantics_update_latch.Signal();
        }
      });

  EmbedderConfigBuilder builder(context);
  builder.SetSurface(DlISize(1, 1));
  builder.SetDartEntrypoint("a11y_main_multi_view");

  auto engine = builder.LaunchEngine();
  ASSERT_TRUE(engine.is_valid());

  // 1: Wait for initial notifySemanticsEnabled(false).
  fml::AutoResetWaitableEvent notify_semantics_enabled_latch;
  notify_semantics_enabled_callback = [&](bool enabled) {
    ASSERT_FALSE(enabled);
    notify_semantics_enabled_latch.Signal();
  };
  notify_semantics_enabled_latch.Wait();

  const int64_t first_view_id = 1;
  const int64_t second_view_id = 2;

  // 2. Add the first view and wait for the add view callback.
  FlutterWindowMetricsEvent window_metrics_event;
  window_metrics_event.struct_size = sizeof(FlutterWindowMetricsEvent);
  window_metrics_event.width = 100;
  window_metrics_event.height = 100;
  window_metrics_event.pixel_ratio = 1.0;
  window_metrics_event.left = 0;
  window_metrics_event.top = 0;
  window_metrics_event.physical_view_inset_top = 0.0;
  window_metrics_event.physical_view_inset_right = 0.0;
  window_metrics_event.physical_view_inset_bottom = 0.0;
  window_metrics_event.physical_view_inset_left = 0.0;
  window_metrics_event.display_id = 0;
  window_metrics_event.view_id = first_view_id;
  window_metrics_event.has_constraints = false;

  FlutterAddViewInfo add_view_info;
  add_view_info.struct_size = sizeof(FlutterAddViewInfo);
  add_view_info.view_id = first_view_id;
  add_view_info.view_metrics = &window_metrics_event;
  fml::AutoResetWaitableEvent notify_add_view_latch;
  add_view_info.user_data = &notify_add_view_latch;
  add_view_info.add_view_callback = [](const FlutterAddViewResult* result) {
    EXPECT_TRUE(result->added);
    auto latch =
        reinterpret_cast<fml::AutoResetWaitableEvent*>(result->user_data);
    latch->Signal();
  };
  FlutterEngineAddView(engine.get(), &add_view_info);
  notify_add_view_latch.Wait();

  // 3. Add the second view and wait for the add view callback.
  add_view_info.view_id = second_view_id;
  window_metrics_event.view_id = second_view_id;
  add_view_info.add_view_callback = [](const FlutterAddViewResult* result) {
    EXPECT_TRUE(result->added);
    auto latch =
        reinterpret_cast<fml::AutoResetWaitableEvent*>(result->user_data);
    latch->Signal();
  };
  FlutterEngineAddView(engine.get(), &add_view_info);
  notify_add_view_latch.Wait();

  // Prepare notifyAccessibilityFeatures callback.
  fml::AutoResetWaitableEvent notify_features_latch;
  notify_accessibility_features_callback = [&](bool reduce_motion) {
    ASSERT_FALSE(reduce_motion);
    notify_features_latch.Signal();
  };

  // 4: Enable semantics. Wait for notifySemanticsEnabled(true).
  fml::AutoResetWaitableEvent notify_semantics_enabled_latch_2;
  notify_semantics_enabled_callback = [&](bool enabled) {
    ASSERT_TRUE(enabled);
    notify_semantics_enabled_latch_2.Signal();
  };
  auto result = FlutterEngineUpdateSemanticsEnabled(engine.get(), true);
  ASSERT_EQ(result, FlutterEngineResult::kSuccess);
  notify_semantics_enabled_latch_2.Wait();

  // 5: Wait for notifyAccessibilityFeatures (reduce_motion == false)
  notify_features_latch.Wait();

  // 6: Wait for UpdateSemantics callback on platform (current) thread.
  // for all pending updates. Expect that it is called 3 times (once for
  // the implicit view and two more times for the views that were manually
  // added).
  signal_native_latch.Wait();
  fml::MessageLoop::GetCurrent().RunExpiredTasksNow();
  semantics_update_latch.Wait();
  EXPECT_EQ(num_times_set_semantics_update_callback2_called, 3);

  // 7: Disable semantics. Wait for NotifySemanticsEnabled(false).
  fml::AutoResetWaitableEvent notify_semantics_enabled_latch_3;
  notify_semantics_enabled_callback = [&](bool enabled) {
    ASSERT_FALSE(enabled);
    notify_semantics_enabled_latch_3.Signal();
  };
  result = FlutterEngineUpdateSemanticsEnabled(engine.get(), false);
  ASSERT_EQ(result, FlutterEngineResult::kSuccess);
  notify_semantics_enabled_latch_3.Wait();
#endif
}

TEST_F(EmbedderA11yTest, ExtendedSemanticsNodeParity) {
  // Arbitrary node ID for testing semantics update translation.
  constexpr int32_t kTestNodeId = 42;
  // Maximum value length of 250 characters for editable text fields.
  constexpr int32_t kTestMaxValueLength = 250;
  // Current value length of 15 characters for editable text fields.
  constexpr int32_t kTestCurrentValueLength = 15;
  // Traversal parent node ID of 7.
  constexpr int32_t kTestTraversalParentId = 7;
  // Test view ID of 1.
  constexpr int64_t kTestViewId = 1;

  SemanticsNode node;
  node.id = kTestNodeId;
  node.maxValueLength = kTestMaxValueLength;
  node.currentValueLength = kTestCurrentValueLength;
  node.traversalParent = kTestTraversalParentId;
  node.minValue = "10.5";
  node.maxValue = "99.5";
  // Create 4x4 matrix with distinct components for testing:
  // scaleX=1.0, skewX=2.0, transX=10.0,
  // skewY=4.0, scaleY=5.0, transY=20.0,
  // pers0=7.0, pers1=8.0, pers2=9.0
  node.hitTestTransform =
      SkM44(1.0f, 2.0f, 0.0f, 10.0f, 4.0f, 5.0f, 0.0f, 20.0f, 0.0f, 0.0f, 1.0f,
            0.0f, 7.0f, 8.0f, 0.0f, 9.0f);
  node.linkUrl = "https://flutter.dev/docs";
  node.role = SemanticsRole::kProgressBar;
  node.validationResult = SemanticsValidationResult::kValid;
  node.locale = "en-US";

  SemanticsNodeUpdates node_updates;
  node_updates[node.id] = node;
  CustomAccessibilityActionUpdates action_updates;

  EmbedderSemanticsUpdate2 update(kTestViewId, node_updates, action_updates);
  const FlutterSemanticsUpdate2* result = update.get();
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->struct_size, sizeof(FlutterSemanticsUpdate2));
  ASSERT_EQ(result->node_count, size_t(1));
  ASSERT_EQ(result->view_id, kTestViewId);

  const FlutterSemanticsNode2* embedder_node = result->nodes[0];
  ASSERT_NE(embedder_node, nullptr);
  ASSERT_EQ(embedder_node->struct_size, sizeof(FlutterSemanticsNode2));
  ASSERT_EQ(embedder_node->id, kTestNodeId);
  ASSERT_EQ(embedder_node->max_value_length, kTestMaxValueLength);
  ASSERT_EQ(embedder_node->current_value_length, kTestCurrentValueLength);
  ASSERT_EQ(embedder_node->traversal_parent, kTestTraversalParentId);
  ASSERT_STREQ(embedder_node->min_value, "10.5");
  ASSERT_STREQ(embedder_node->max_value, "99.5");
  ASSERT_EQ(embedder_node->hit_test_transform.scaleX, 1.0);
  ASSERT_EQ(embedder_node->hit_test_transform.skewX, 2.0);
  ASSERT_EQ(embedder_node->hit_test_transform.transX, 10.0);
  ASSERT_EQ(embedder_node->hit_test_transform.skewY, 4.0);
  ASSERT_EQ(embedder_node->hit_test_transform.scaleY, 5.0);
  ASSERT_EQ(embedder_node->hit_test_transform.transY, 20.0);
  ASSERT_EQ(embedder_node->hit_test_transform.pers0, 7.0);
  ASSERT_EQ(embedder_node->hit_test_transform.pers1, 8.0);
  ASSERT_EQ(embedder_node->hit_test_transform.pers2, 9.0);
  ASSERT_STREQ(embedder_node->link_url, "https://flutter.dev/docs");
  ASSERT_EQ(embedder_node->role, kFlutterSemanticsRoleProgressBar);
  ASSERT_EQ(embedder_node->validation_result,
            kFlutterSemanticsValidationResultValid);
  ASSERT_STREQ(embedder_node->locale, "en-US");
}

TEST_F(EmbedderA11yTest, ExtendedSemanticsDefaultValues) {
  // Arbitrary node ID for testing default semantics translation.
  constexpr int32_t kDefaultNodeId = 100;
  // Test view ID of 0 for the default/implicit view.
  constexpr int64_t kDefaultViewId = 0;

  SemanticsNode default_node;
  default_node.id = kDefaultNodeId;

  SemanticsNodeUpdates node_updates;
  node_updates[default_node.id] = default_node;
  CustomAccessibilityActionUpdates action_updates;

  EmbedderSemanticsUpdate2 update(kDefaultViewId, node_updates, action_updates);
  const FlutterSemanticsUpdate2* result = update.get();
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(result->node_count, size_t(1));

  const FlutterSemanticsNode2* embedder_node = result->nodes[0];
  ASSERT_NE(embedder_node, nullptr);
  ASSERT_EQ(embedder_node->struct_size, sizeof(FlutterSemanticsNode2));
  // Default values should match SemanticsNode defaults:
  // Unconstrained max and current value length (-1).
  ASSERT_EQ(embedder_node->max_value_length, -1);
  ASSERT_EQ(embedder_node->current_value_length, -1);
  // Root traversal parent (0).
  ASSERT_EQ(embedder_node->traversal_parent, 0);
  // Default hit test transform should be identity.
  ASSERT_EQ(embedder_node->hit_test_transform.scaleX, 1.0);
  ASSERT_EQ(embedder_node->hit_test_transform.skewX, 0.0);
  ASSERT_EQ(embedder_node->hit_test_transform.transX, 0.0);
  ASSERT_EQ(embedder_node->hit_test_transform.skewY, 0.0);
  ASSERT_EQ(embedder_node->hit_test_transform.scaleY, 1.0);
  ASSERT_EQ(embedder_node->hit_test_transform.transY, 0.0);
  ASSERT_EQ(embedder_node->hit_test_transform.pers0, 0.0);
  ASSERT_EQ(embedder_node->hit_test_transform.pers1, 0.0);
  ASSERT_EQ(embedder_node->hit_test_transform.pers2, 1.0);
  // Empty strings should return valid non-null empty strings.
  ASSERT_NE(embedder_node->min_value, nullptr);
  ASSERT_STREQ(embedder_node->min_value, "");
  ASSERT_NE(embedder_node->max_value, nullptr);
  ASSERT_STREQ(embedder_node->max_value, "");
  ASSERT_NE(embedder_node->link_url, nullptr);
  ASSERT_STREQ(embedder_node->link_url, "");
  ASSERT_NE(embedder_node->locale, nullptr);
  ASSERT_STREQ(embedder_node->locale, "");
  // Default role and validation result.
  ASSERT_EQ(embedder_node->role, kFlutterSemanticsRoleNone);
  ASSERT_EQ(embedder_node->validation_result,
            kFlutterSemanticsValidationResultNone);
}

TEST_F(EmbedderA11yTest, FlutterSemanticsNode2SafeAccessCompatibility) {
  // Arbitrary test node ID.
  constexpr int32_t kTestId = 1;
  // Default unconstrained value length.
  constexpr int32_t kDefaultUnconstrained = -1;
  // Test value length of 50 characters.
  constexpr int32_t kTestLength = 50;

  FlutterSemanticsNode2 node = {};
  node.struct_size = sizeof(FlutterSemanticsNode2);
  node.id = kTestId;
  node.max_value_length = kTestLength;
  node.role = kFlutterSemanticsRoleSpinButton;

  const FlutterSemanticsNode2* node_ptr = &node;

  // With full struct_size, SAFE_ACCESS retrieves actual values.
  EXPECT_EQ(SAFE_ACCESS(node_ptr, max_value_length, kDefaultUnconstrained),
            kTestLength);
  EXPECT_EQ(SAFE_ACCESS(node_ptr, role, kFlutterSemanticsRoleNone),
            kFlutterSemanticsRoleSpinButton);

  // Simulate a legacy embedder compiled when struct ended at identifier.
  node.struct_size = offsetof(FlutterSemanticsNode2, max_value_length);
  EXPECT_EQ(SAFE_ACCESS(node_ptr, max_value_length, kDefaultUnconstrained),
            kDefaultUnconstrained);
  EXPECT_EQ(SAFE_ACCESS(node_ptr, role, kFlutterSemanticsRoleNone),
            kFlutterSemanticsRoleNone);
  EXPECT_EQ(SAFE_ACCESS(node_ptr, link_url, nullptr), nullptr);
}

TEST_F(EmbedderA11yTest, AllSemanticsRolesAndValidationEnumParity) {
  // Validate that every single SemanticsRole value matches its
  // FlutterSemanticsRole counterpart.
  static_assert(static_cast<int32_t>(SemanticsRole::kNone) ==
                static_cast<int32_t>(kFlutterSemanticsRoleNone));
  static_assert(static_cast<int32_t>(SemanticsRole::kTab) ==
                static_cast<int32_t>(kFlutterSemanticsRoleTab));
  static_assert(static_cast<int32_t>(SemanticsRole::kTabBar) ==
                static_cast<int32_t>(kFlutterSemanticsRoleTabBar));
  static_assert(static_cast<int32_t>(SemanticsRole::kTabPanel) ==
                static_cast<int32_t>(kFlutterSemanticsRoleTabPanel));
  static_assert(static_cast<int32_t>(SemanticsRole::kDialog) ==
                static_cast<int32_t>(kFlutterSemanticsRoleDialog));
  static_assert(static_cast<int32_t>(SemanticsRole::kAlertDialog) ==
                static_cast<int32_t>(kFlutterSemanticsRoleAlertDialog));
  static_assert(static_cast<int32_t>(SemanticsRole::kTable) ==
                static_cast<int32_t>(kFlutterSemanticsRoleTable));
  static_assert(static_cast<int32_t>(SemanticsRole::kCell) ==
                static_cast<int32_t>(kFlutterSemanticsRoleCell));
  static_assert(static_cast<int32_t>(SemanticsRole::kRow) ==
                static_cast<int32_t>(kFlutterSemanticsRoleRow));
  static_assert(static_cast<int32_t>(SemanticsRole::kColumnHeader) ==
                static_cast<int32_t>(kFlutterSemanticsRoleColumnHeader));
  static_assert(static_cast<int32_t>(SemanticsRole::kDragHandle) ==
                static_cast<int32_t>(kFlutterSemanticsRoleDragHandle));
  static_assert(static_cast<int32_t>(SemanticsRole::kSpinButton) ==
                static_cast<int32_t>(kFlutterSemanticsRoleSpinButton));
  static_assert(static_cast<int32_t>(SemanticsRole::kComboBox) ==
                static_cast<int32_t>(kFlutterSemanticsRoleComboBox));
  static_assert(static_cast<int32_t>(SemanticsRole::kMenuBar) ==
                static_cast<int32_t>(kFlutterSemanticsRoleMenuBar));
  static_assert(static_cast<int32_t>(SemanticsRole::kMenu) ==
                static_cast<int32_t>(kFlutterSemanticsRoleMenu));
  static_assert(static_cast<int32_t>(SemanticsRole::kMenuItem) ==
                static_cast<int32_t>(kFlutterSemanticsRoleMenuItem));
  static_assert(static_cast<int32_t>(SemanticsRole::kMenuItemCheckbox) ==
                static_cast<int32_t>(kFlutterSemanticsRoleMenuItemCheckbox));
  static_assert(static_cast<int32_t>(SemanticsRole::kMenuItemRadio) ==
                static_cast<int32_t>(kFlutterSemanticsRoleMenuItemRadio));
  static_assert(static_cast<int32_t>(SemanticsRole::kList) ==
                static_cast<int32_t>(kFlutterSemanticsRoleList));
  static_assert(static_cast<int32_t>(SemanticsRole::kListItem) ==
                static_cast<int32_t>(kFlutterSemanticsRoleListItem));
  static_assert(static_cast<int32_t>(SemanticsRole::kForm) ==
                static_cast<int32_t>(kFlutterSemanticsRoleForm));
  static_assert(static_cast<int32_t>(SemanticsRole::kTooltip) ==
                static_cast<int32_t>(kFlutterSemanticsRoleTooltip));
  static_assert(static_cast<int32_t>(SemanticsRole::kLoadingSpinner) ==
                static_cast<int32_t>(kFlutterSemanticsRoleLoadingSpinner));
  static_assert(static_cast<int32_t>(SemanticsRole::kProgressBar) ==
                static_cast<int32_t>(kFlutterSemanticsRoleProgressBar));
  static_assert(static_cast<int32_t>(SemanticsRole::kHotKey) ==
                static_cast<int32_t>(kFlutterSemanticsRoleHotKey));
  static_assert(static_cast<int32_t>(SemanticsRole::kRadioGroup) ==
                static_cast<int32_t>(kFlutterSemanticsRoleRadioGroup));
  static_assert(static_cast<int32_t>(SemanticsRole::kStatus) ==
                static_cast<int32_t>(kFlutterSemanticsRoleStatus));
  static_assert(static_cast<int32_t>(SemanticsRole::kAlert) ==
                static_cast<int32_t>(kFlutterSemanticsRoleAlert));
  static_assert(static_cast<int32_t>(SemanticsRole::kComplementary) ==
                static_cast<int32_t>(kFlutterSemanticsRoleComplementary));
  static_assert(static_cast<int32_t>(SemanticsRole::kContentInfo) ==
                static_cast<int32_t>(kFlutterSemanticsRoleContentInfo));
  static_assert(static_cast<int32_t>(SemanticsRole::kMain) ==
                static_cast<int32_t>(kFlutterSemanticsRoleMain));
  static_assert(static_cast<int32_t>(SemanticsRole::kNavigation) ==
                static_cast<int32_t>(kFlutterSemanticsRoleNavigation));
  static_assert(static_cast<int32_t>(SemanticsRole::kRegion) ==
                static_cast<int32_t>(kFlutterSemanticsRoleRegion));

  // Validate that every SemanticsValidationResult matches its
  // FlutterSemanticsValidationResult counterpart.
  static_assert(static_cast<int32_t>(SemanticsValidationResult::kNone) ==
                static_cast<int32_t>(kFlutterSemanticsValidationResultNone));
  static_assert(static_cast<int32_t>(SemanticsValidationResult::kValid) ==
                static_cast<int32_t>(kFlutterSemanticsValidationResultValid));
  static_assert(static_cast<int32_t>(SemanticsValidationResult::kInvalid) ==
                static_cast<int32_t>(kFlutterSemanticsValidationResultInvalid));
}

}  // namespace testing
}  // namespace flutter

// NOLINTEND(clang-analyzer-core.StackAddressEscape)
