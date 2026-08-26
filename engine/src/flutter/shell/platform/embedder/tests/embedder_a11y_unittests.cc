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
  FlutterWindowMetricsEvent window_metrics_event = {};
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

  FlutterAddViewInfo add_view_info = {};
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

TEST_F(EmbedderA11yTest, CompleteSemanticsNode2FieldsPopulated) {
  SemanticsNodeUpdates updates;
  CustomAccessibilityActionUpdates actions;

  SemanticsNode node;
  node.id = 42;
  node.flags = SemanticsFlags{};
  node.actions = static_cast<int32_t>(SemanticsAction::kTap);
  node.maxValueLength = 100;
  node.currentValueLength = 25;
  node.traversalParent = 1;
  node.role = SemanticsRole::kTab;
  node.validationResult = SemanticsValidationResult::kValid;
  node.linkUrl = "https://flutter.dev";
  node.locale = "en-US";
  node.minValue = "0.0";
  node.maxValue = "100.0";
  node.label = "Sample Label";
  node.hint = "Sample Hint";
  node.value = "Sample Value";
  node.increasedValue = "Sample Inc";
  node.decreasedValue = "Sample Dec";
  node.tooltip = "Sample Tooltip";
  node.identifier = "sample_identifier";
  node.headingLevel = 3;
  node.textDirection = 2;
  node.rect = SkRect::MakeLTRB(10.0f, 20.0f, 30.0f, 40.0f);
  node.transform = SkM44(1, 2, 0, 3, 4, 5, 0, 6, 0, 0, 1, 0, 7, 8, 0, 9);
  node.hitTestTransform = SkM44(9, 8, 0, 7, 6, 5, 0, 4, 0, 0, 1, 0, 3, 2, 0, 1);
  node.childrenInTraversalOrder = {2, 3};
  node.childrenInHitTestOrder = {3, 2};
  node.customAccessibilityActions = {101};

  updates[42] = node;

  CustomAccessibilityAction action;
  action.id = 101;
  action.overrideId = static_cast<int32_t>(SemanticsAction::kTap);
  action.label = "Custom Tap";
  action.hint = "Custom Tap Hint";
  actions[101] = action;

  EmbedderSemanticsUpdate2 update(1, updates, actions);
  const FlutterSemanticsUpdate2* semantic_update = update.get();
  ASSERT_NE(semantic_update, nullptr);
  ASSERT_EQ(semantic_update->node_count, 1u);
  ASSERT_EQ(semantic_update->custom_action_count, 1u);
  ASSERT_EQ(semantic_update->view_id, 1);

  const FlutterSemanticsNode2* node2 = semantic_update->nodes[0];
  ASSERT_NE(node2, nullptr);
  EXPECT_EQ(node2->struct_size, sizeof(FlutterSemanticsNode2));
  EXPECT_EQ(node2->id, 42);
  EXPECT_EQ(node2->max_value_length, 100);
  EXPECT_EQ(node2->current_value_length, 25);
  EXPECT_EQ(node2->traversal_parent, 1);
  EXPECT_EQ(node2->role, kFlutterSemanticsRoleTab);
  EXPECT_EQ(node2->validation_result, kFlutterSemanticsValidationResultValid);
  EXPECT_STREQ(node2->link_url, "https://flutter.dev");
  EXPECT_STREQ(node2->locale, "en-US");
  EXPECT_STREQ(node2->min_value, "0.0");
  EXPECT_STREQ(node2->max_value, "100.0");
  EXPECT_STREQ(node2->label, "Sample Label");
  EXPECT_STREQ(node2->hint, "Sample Hint");
  EXPECT_STREQ(node2->value, "Sample Value");
  EXPECT_STREQ(node2->increased_value, "Sample Inc");
  EXPECT_STREQ(node2->decreased_value, "Sample Dec");
  EXPECT_STREQ(node2->tooltip, "Sample Tooltip");
  EXPECT_STREQ(node2->identifier, "sample_identifier");
  EXPECT_EQ(node2->heading_level, 3);
  EXPECT_EQ(node2->text_direction, kFlutterTextDirectionLTR);
  EXPECT_FLOAT_EQ(node2->rect.left, 10.0f);
  EXPECT_FLOAT_EQ(node2->rect.top, 20.0f);
  EXPECT_FLOAT_EQ(node2->rect.right, 30.0f);
  EXPECT_FLOAT_EQ(node2->rect.bottom, 40.0f);
  EXPECT_EQ(node2->hit_test_transform.scaleX, 9.0);
  EXPECT_EQ(node2->hit_test_transform.skewX, 8.0);
  EXPECT_EQ(node2->hit_test_transform.transX, 7.0);
  EXPECT_EQ(node2->hit_test_transform.skewY, 6.0);
  EXPECT_EQ(node2->hit_test_transform.scaleY, 5.0);
  EXPECT_EQ(node2->hit_test_transform.transY, 4.0);
  EXPECT_EQ(node2->hit_test_transform.pers0, 3.0);
  EXPECT_EQ(node2->hit_test_transform.pers1, 2.0);
  EXPECT_EQ(node2->hit_test_transform.pers2, 1.0);

  const FlutterSemanticsCustomAction2* action2 =
      semantic_update->custom_actions[0];
  ASSERT_NE(action2, nullptr);
  EXPECT_EQ(action2->struct_size, sizeof(FlutterSemanticsCustomAction2));
  EXPECT_EQ(action2->id, 101);
  EXPECT_EQ(action2->override_action, kFlutterSemanticsActionTap);
  EXPECT_STREQ(action2->label, "Custom Tap");
  EXPECT_STREQ(action2->hint, "Custom Tap Hint");
}

static_assert(static_cast<int>(SemanticsRole::kNone) ==
              static_cast<int>(kFlutterSemanticsRoleNone));
static_assert(static_cast<int>(SemanticsRole::kTab) ==
              static_cast<int>(kFlutterSemanticsRoleTab));
static_assert(static_cast<int>(SemanticsRole::kTabBar) ==
              static_cast<int>(kFlutterSemanticsRoleTabBar));
static_assert(static_cast<int>(SemanticsRole::kTabPanel) ==
              static_cast<int>(kFlutterSemanticsRoleTabPanel));
static_assert(static_cast<int>(SemanticsRole::kDialog) ==
              static_cast<int>(kFlutterSemanticsRoleDialog));
static_assert(static_cast<int>(SemanticsRole::kAlertDialog) ==
              static_cast<int>(kFlutterSemanticsRoleAlertDialog));
static_assert(static_cast<int>(SemanticsRole::kTable) ==
              static_cast<int>(kFlutterSemanticsRoleTable));
static_assert(static_cast<int>(SemanticsRole::kCell) ==
              static_cast<int>(kFlutterSemanticsRoleCell));
static_assert(static_cast<int>(SemanticsRole::kRow) ==
              static_cast<int>(kFlutterSemanticsRoleRow));
static_assert(static_cast<int>(SemanticsRole::kColumnHeader) ==
              static_cast<int>(kFlutterSemanticsRoleColumnHeader));
static_assert(static_cast<int>(SemanticsRole::kDragHandle) ==
              static_cast<int>(kFlutterSemanticsRoleDragHandle));
static_assert(static_cast<int>(SemanticsRole::kSpinButton) ==
              static_cast<int>(kFlutterSemanticsRoleSpinButton));
static_assert(static_cast<int>(SemanticsRole::kComboBox) ==
              static_cast<int>(kFlutterSemanticsRoleComboBox));
static_assert(static_cast<int>(SemanticsRole::kMenuBar) ==
              static_cast<int>(kFlutterSemanticsRoleMenuBar));
static_assert(static_cast<int>(SemanticsRole::kMenu) ==
              static_cast<int>(kFlutterSemanticsRoleMenu));
static_assert(static_cast<int>(SemanticsRole::kMenuItem) ==
              static_cast<int>(kFlutterSemanticsRoleMenuItem));
static_assert(static_cast<int>(SemanticsRole::kMenuItemCheckbox) ==
              static_cast<int>(kFlutterSemanticsRoleMenuItemCheckbox));
static_assert(static_cast<int>(SemanticsRole::kMenuItemRadio) ==
              static_cast<int>(kFlutterSemanticsRoleMenuItemRadio));
static_assert(static_cast<int>(SemanticsRole::kList) ==
              static_cast<int>(kFlutterSemanticsRoleList));
static_assert(static_cast<int>(SemanticsRole::kListItem) ==
              static_cast<int>(kFlutterSemanticsRoleListItem));
static_assert(static_cast<int>(SemanticsRole::kForm) ==
              static_cast<int>(kFlutterSemanticsRoleForm));
static_assert(static_cast<int>(SemanticsRole::kTooltip) ==
              static_cast<int>(kFlutterSemanticsRoleTooltip));
static_assert(static_cast<int>(SemanticsRole::kLoadingSpinner) ==
              static_cast<int>(kFlutterSemanticsRoleLoadingSpinner));
static_assert(static_cast<int>(SemanticsRole::kProgressBar) ==
              static_cast<int>(kFlutterSemanticsRoleProgressBar));
static_assert(static_cast<int>(SemanticsRole::kHotKey) ==
              static_cast<int>(kFlutterSemanticsRoleHotKey));
static_assert(static_cast<int>(SemanticsRole::kRadioGroup) ==
              static_cast<int>(kFlutterSemanticsRoleRadioGroup));
static_assert(static_cast<int>(SemanticsRole::kStatus) ==
              static_cast<int>(kFlutterSemanticsRoleStatus));
static_assert(static_cast<int>(SemanticsRole::kAlert) ==
              static_cast<int>(kFlutterSemanticsRoleAlert));
static_assert(static_cast<int>(SemanticsRole::kComplementary) ==
              static_cast<int>(kFlutterSemanticsRoleComplementary));
static_assert(static_cast<int>(SemanticsRole::kContentInfo) ==
              static_cast<int>(kFlutterSemanticsRoleContentInfo));
static_assert(static_cast<int>(SemanticsRole::kMain) ==
              static_cast<int>(kFlutterSemanticsRoleMain));
static_assert(static_cast<int>(SemanticsRole::kNavigation) ==
              static_cast<int>(kFlutterSemanticsRoleNavigation));
static_assert(static_cast<int>(SemanticsRole::kRegion) ==
              static_cast<int>(kFlutterSemanticsRoleRegion));

static_assert(static_cast<int>(SemanticsValidationResult::kNone) ==
              static_cast<int>(kFlutterSemanticsValidationResultNone));
static_assert(static_cast<int>(SemanticsValidationResult::kValid) ==
              static_cast<int>(kFlutterSemanticsValidationResultValid));
static_assert(static_cast<int>(SemanticsValidationResult::kInvalid) ==
              static_cast<int>(kFlutterSemanticsValidationResultInvalid));

TEST_F(EmbedderA11yTest, DefaultSemanticsNode2FieldsPopulated) {
  SemanticsNodeUpdates updates;
  CustomAccessibilityActionUpdates actions;

  SemanticsNode default_node;
  default_node.id = 0;
  updates[0] = default_node;

  EmbedderSemanticsUpdate2 update(0, updates, actions);
  const FlutterSemanticsUpdate2* semantic_update = update.get();
  ASSERT_NE(semantic_update, nullptr);
  ASSERT_EQ(semantic_update->node_count, 1u);

  const FlutterSemanticsNode2* node = semantic_update->nodes[0];
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->struct_size, sizeof(FlutterSemanticsNode2));
  EXPECT_EQ(node->id, 0);
  EXPECT_EQ(node->max_value_length, -1);
  EXPECT_EQ(node->current_value_length, -1);
  EXPECT_EQ(node->traversal_parent, 0);
  EXPECT_EQ(node->role, kFlutterSemanticsRoleNone);
  EXPECT_EQ(node->validation_result, kFlutterSemanticsValidationResultNone);
  EXPECT_STREQ(node->link_url, "");
  EXPECT_STREQ(node->locale, "");
  EXPECT_STREQ(node->min_value, "");
  EXPECT_STREQ(node->max_value, "");
}

}  // namespace testing
}  // namespace flutter

// NOLINTEND(clang-analyzer-core.StackAddressEscape)
