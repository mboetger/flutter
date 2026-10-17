// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "flutter/fml/platform/android/jni_util.h"
#include "flutter/fml/platform/android/jni_weak_ref.h"
#include "flutter/fml/platform/android/scoped_java_ref.h"
#include "flutter/lib/ui/window/platform_configuration.h"
#include "flutter/runtime/platform_data.h"
#include "flutter/runtime/runtime_controller.h"
#include "flutter/shell/common/engine.h"
#include "flutter/shell/common/shell.h"
#include "flutter/shell/platform/android/android_shell_holder.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "flutter/shell/platform/android/jni/mock_jni_env.h"
#include "flutter/shell/platform/android/ndk/ndk_proc_table.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "flutter/shell/platform/android/platform_view_android_jni_impl.h"
#include "flutter/shell/platform/embedder/embedder.h"
#include "flutter/shell/platform/embedder/test_utils/proc_table_replacement.h"

namespace flutter {
namespace testing {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Return;
using ::testing::ReturnArg;

class PlatformViewAndroidJNIImplTest : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    static std::once_flag jvm_init_flag;
    std::call_once(jvm_init_flag, SetUpJVM);
  }

 private:
  friend class MockJNIEnvProvider;
  static MockJavaVM jvm_;
  static void SetUpJVM();
};

MockJavaVM PlatformViewAndroidJNIImplTest::jvm_;

class MockJNIEnvProvider {
 public:
  MockJNIEnvProvider() {
    PlatformViewAndroidJNIImplTest::jvm_.SetJNIEnv(&env_);
  }
  ~MockJNIEnvProvider() {
    PlatformViewAndroidJNIImplTest::jvm_.SetJNIEnv(nullptr);
  }
  MockJNIEnv& env() { return env_; }

 private:
  MockJNIEnv env_;
};

void PlatformViewAndroidJNIImplTest::SetUpJVM() {
  fml::jni::InitJavaVM(&jvm_);

  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  const jclass kPlaceholderClass = reinterpret_cast<jclass>(100);
  const jfieldID kPlaceholderFieldID = reinterpret_cast<jfieldID>(200);
  const jmethodID kPlaceholderMethodID = reinterpret_cast<jmethodID>(300);

  EXPECT_CALL(mock_env, GetObjectRefType(_))
      .WillRepeatedly(Return(JNILocalRefType));
  EXPECT_CALL(mock_env, NewLocalRef(_)).WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteLocalRef(_)).WillRepeatedly(Return());
  EXPECT_CALL(mock_env, NewGlobalRef(_)).WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteGlobalRef(_)).WillRepeatedly(Return());
  EXPECT_CALL(mock_env, FindClass(_)).WillRepeatedly(Return(kPlaceholderClass));
  EXPECT_CALL(mock_env, GetFieldID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderFieldID));
  EXPECT_CALL(mock_env, GetMethodID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderMethodID));
  EXPECT_CALL(mock_env, GetStaticFieldID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderFieldID));
  EXPECT_CALL(mock_env, GetStaticMethodID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderMethodID));
  EXPECT_CALL(mock_env, ExceptionCheck()).WillRepeatedly(Return(JNI_FALSE));
  EXPECT_CALL(mock_env, RegisterNatives(_, _, _)).WillRepeatedly(Return(0));

  PlatformViewAndroid::Register(&mock_env);
}

TEST_F(PlatformViewAndroidJNIImplTest, ImageGetHardwareBufferException) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  // Call ImageGetHardwareBuffer and simulate throwing an exception.
  // Verify that it clears the exception and does not abort the process.
  EXPECT_CALL(mock_env, GetObjectRefType(_))
      .WillRepeatedly(Return(JNILocalRefType));
  EXPECT_CALL(mock_env, NewLocalRef(_)).WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteLocalRef(_)).WillRepeatedly(Return());
  EXPECT_CALL(mock_env, CallObjectMethodV(_, _, _))
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(mock_env, ExceptionCheck()).WillOnce(Return(JNI_TRUE));
  EXPECT_CALL(mock_env, ExceptionDescribe()).WillOnce(Return());
  EXPECT_CALL(mock_env, ExceptionClear()).Times(1).WillOnce(Return());

  fml::jni::JavaObjectWeakGlobalRef flutter_jni_object;
  PlatformViewAndroidJNIImpl android_jni(flutter_jni_object);

  fml::jni::ScopedJavaLocalRef<jobject> image(&mock_env,
                                              reinterpret_cast<jobject>(123));
  android_jni.ImageGetHardwareBuffer(image);
}

TEST_F(PlatformViewAndroidJNIImplTest,
       CreateTransactionRetainsGlobalRefUntilPublished) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  const jobject kFlutterJniObj = reinterpret_cast<jobject>(0x1001);
  const jobject kJavaTxObj = reinterpret_cast<jobject>(0x2002);
  ASurfaceTransaction* const kNativeTx =
      reinterpret_cast<ASurfaceTransaction*>(0x3003);

  // Stub the NDK call so the fake Java object is never dereferenced.
  auto& from_java_proc =
      GetMutableNDKProcTableForTesting().ASurfaceTransaction_fromJava;
  auto* real_from_java = from_java_proc;
  from_java_proc = [](JNIEnv*, jobject) -> ASurfaceTransaction* {
    return reinterpret_cast<ASurfaceTransaction*>(0x3003);
  };
  struct RestoreProc {
    decltype(real_from_java)* slot;
    decltype(real_from_java) prev;
    ~RestoreProc() { *slot = prev; }
  } restore{&from_java_proc, real_from_java};

  EXPECT_CALL(mock_env, GetObjectRefType(_))
      .WillRepeatedly(Return(JNILocalRefType));
  EXPECT_CALL(mock_env, NewLocalRef(_)).WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteLocalRef(_)).WillRepeatedly(Return());
  EXPECT_CALL(mock_env, NewWeakGlobalRef(_)).WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteWeakGlobalRef(_)).WillRepeatedly(Return());
  EXPECT_CALL(mock_env, ExceptionCheck()).WillRepeatedly(Return(JNI_FALSE));
  EXPECT_CALL(mock_env, CallObjectMethodV(kFlutterJniObj, _, _))
      .WillOnce(Return(kJavaTxObj));

  bool global_ref_alive = false;
  EXPECT_CALL(mock_env, NewGlobalRef(kJavaTxObj))
      .WillOnce([&](jobject obj) -> jobject {
        global_ref_alive = true;
        return obj;
      });
  EXPECT_CALL(mock_env, DeleteGlobalRef(kJavaTxObj)).WillOnce([&](jobject) {
    global_ref_alive = false;
  });

  bool publish_called = false;
  EXPECT_CALL(mock_env, CallVoidMethodV(kFlutterJniObj, _, _))
      .WillOnce([&](jobject, jmethodID, va_list args) {
        // publishTransaction must receive the same transaction, while the
        // global ref is still keeping it alive.
        EXPECT_EQ(va_arg(args, jobject), kJavaTxObj);
        EXPECT_TRUE(global_ref_alive);
        publish_called = true;
      });

  fml::jni::JavaObjectWeakGlobalRef flutter_jni_ref(&mock_env, kFlutterJniObj);
  PlatformViewAndroidJNIImpl android_jni(flutter_jni_ref);

  std::function<void()> publish_cb;
  ASurfaceTransaction* tx = android_jni.createTransaction(publish_cb);
  EXPECT_EQ(tx, kNativeTx);
  ASSERT_NE(publish_cb, nullptr);
  EXPECT_TRUE(global_ref_alive);
  EXPECT_FALSE(publish_called);

  publish_cb();
  EXPECT_TRUE(publish_called);
  EXPECT_FALSE(global_ref_alive);
}

TEST_F(PlatformViewAndroidJNIImplTest,
       PublishAppliesNativelyIfFlutterJNIWasCollected) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  const jobject kFlutterJniObj = reinterpret_cast<jobject>(0x1001);
  const jobject kJavaTxObj = reinterpret_cast<jobject>(0x2002);

  // Stub the NDK calls so the fake objects are never dereferenced.
  auto& proc_table = GetMutableNDKProcTableForTesting();
  auto* real_from_java = proc_table.ASurfaceTransaction_fromJava;
  auto* real_apply = proc_table.ASurfaceTransaction_apply;
  proc_table.ASurfaceTransaction_fromJava =
      [](JNIEnv*, jobject) -> ASurfaceTransaction* {
    return reinterpret_cast<ASurfaceTransaction*>(0x3003);
  };
  static ASurfaceTransaction* applied_tx;
  applied_tx = nullptr;
  proc_table.ASurfaceTransaction_apply = [](ASurfaceTransaction* tx) {
    applied_tx = tx;
  };
  struct RestoreProcs {
    NDKProcTable& table;
    decltype(real_from_java) from_java;
    decltype(real_apply) apply;
    ~RestoreProcs() {
      table.ASurfaceTransaction_fromJava = from_java;
      table.ASurfaceTransaction_apply = apply;
    }
  } restore{proc_table, real_from_java, real_apply};

  // Resolving the weak FlutterJNI ref goes through NewLocalRef. Once
  // `flutter_jni_collected` is set, model the object having been collected.
  bool flutter_jni_collected = false;
  EXPECT_CALL(mock_env, GetObjectRefType(_))
      .WillRepeatedly(Return(JNILocalRefType));
  EXPECT_CALL(mock_env, NewLocalRef(_))
      .WillRepeatedly([&](jobject obj) -> jobject {
        return (obj == kFlutterJniObj && flutter_jni_collected) ? nullptr : obj;
      });
  EXPECT_CALL(mock_env, DeleteLocalRef(_)).WillRepeatedly(Return());
  EXPECT_CALL(mock_env, NewWeakGlobalRef(_)).WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteWeakGlobalRef(_)).WillRepeatedly(Return());
  EXPECT_CALL(mock_env, ExceptionCheck()).WillRepeatedly(Return(JNI_FALSE));
  EXPECT_CALL(mock_env, CallObjectMethodV(kFlutterJniObj, _, _))
      .WillOnce(Return(kJavaTxObj));

  bool global_ref_alive = false;
  EXPECT_CALL(mock_env, NewGlobalRef(kJavaTxObj))
      .WillOnce([&](jobject obj) -> jobject {
        global_ref_alive = true;
        return obj;
      });
  EXPECT_CALL(mock_env, DeleteGlobalRef(kJavaTxObj)).WillOnce([&](jobject) {
    // The transaction must be applied before the global ref stops keeping the
    // Java object (and therefore the native handle) alive.
    EXPECT_NE(applied_tx, nullptr);
    global_ref_alive = false;
  });

  // There is no FlutterJNI to publish to.
  EXPECT_CALL(mock_env, CallVoidMethodV(_, _, _)).Times(0);

  fml::jni::JavaObjectWeakGlobalRef flutter_jni_ref(&mock_env, kFlutterJniObj);
  PlatformViewAndroidJNIImpl android_jni(flutter_jni_ref);

  std::function<void()> publish_cb;
  ASurfaceTransaction* tx = android_jni.createTransaction(publish_cb);
  ASSERT_NE(tx, nullptr);
  ASSERT_NE(publish_cb, nullptr);

  flutter_jni_collected = true;
  publish_cb();
  EXPECT_EQ(applied_tx, tx);
  EXPECT_FALSE(global_ref_alive);

  // A second invocation must not touch the transaction again.
  applied_tx = nullptr;
  publish_cb();
  EXPECT_EQ(applied_tx, nullptr);
}

TEST_F(PlatformViewAndroidJNIImplTest, SetViewportMetricsEmptyArrays) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  typedef void (*SetViewportMetricsFn)(
      JNIEnv*, jobject, jlong, jfloat, jint, jint, jint, jint, jint, jint, jint,
      jint, jint, jint, jint, jint, jint, jint, jint, jintArray, jintArray,
      jintArray, jint, jint, jint, jint, jint, jint, jint, jint);

  SetViewportMetricsFn set_viewport_metrics = nullptr;

  const jclass kPlaceholderClass = reinterpret_cast<jclass>(100);
  const jfieldID kPlaceholderFieldID = reinterpret_cast<jfieldID>(200);
  const jmethodID kPlaceholderMethodID = reinterpret_cast<jmethodID>(300);

  EXPECT_CALL(mock_env, GetObjectRefType(_))
      .WillRepeatedly(Return(JNILocalRefType));
  EXPECT_CALL(mock_env, NewLocalRef(_)).WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteLocalRef(_)).WillRepeatedly(Return());
  EXPECT_CALL(mock_env, NewGlobalRef(_)).WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteGlobalRef(_)).WillRepeatedly(Return());
  EXPECT_CALL(mock_env, FindClass(_)).WillRepeatedly(Return(kPlaceholderClass));
  EXPECT_CALL(mock_env, GetFieldID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderFieldID));
  EXPECT_CALL(mock_env, GetMethodID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderMethodID));
  EXPECT_CALL(mock_env, GetStaticFieldID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderFieldID));
  EXPECT_CALL(mock_env, GetStaticMethodID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderMethodID));
  EXPECT_CALL(mock_env, ExceptionCheck()).WillRepeatedly(Return(JNI_FALSE));

  EXPECT_CALL(mock_env, RegisterNatives(_, _, _))
      .WillRepeatedly(
          [&](jclass clazz, const JNINativeMethod* methods, jint nMethods) {
            for (jint i = 0; i < nMethods; ++i) {
              if (strcmp(methods[i].name, "nativeSetViewportMetrics") == 0) {
                set_viewport_metrics =
                    reinterpret_cast<SetViewportMetricsFn>(methods[i].fnPtr);
              }
            }
            return 0;
          });

  PlatformViewAndroid::Register(&mock_env);

  ASSERT_NE(set_viewport_metrics, nullptr);

  EXPECT_CALL(mock_env, GetArrayLength(_)).WillRepeatedly(Return(0));
  EXPECT_CALL(mock_env, GetIntArrayRegion(_, _, _, _)).Times(0);

  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<JNIMock>();
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  jobject jcaller = reinterpret_cast<jobject>(123);
  jintArray bounds = reinterpret_cast<jintArray>(456);
  jintArray type = reinterpret_cast<jintArray>(789);
  jintArray state = reinterpret_cast<jintArray>(1011);

  set_viewport_metrics(&mock_env, jcaller,
                       reinterpret_cast<jlong>(holder.get()), 1.0f, 100, 100, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, bounds, type, state,
                       0, 0, 0, 0, 0, 0, 0, 0);
}

namespace {

// Signatures of FlutterJNI natives registered by PlatformViewAndroid::Register.
using SetSemanticsEnabledFn = void (*)(JNIEnv*, jobject, jlong, jboolean);
using SetAccessibilityFeaturesFn = void (*)(JNIEnv*, jobject, jlong, jint);
using ScheduleFrameFn = void (*)(JNIEnv*, jobject, jlong);
using UpdateDisplayMetricsFn = void (*)(JNIEnv*, jobject, jlong);
using SetViewportMetricsFn = void (*)(JNIEnv*,
                                      jobject,
                                      jlong,
                                      jfloat,
                                      jint,
                                      jint,
                                      jint,
                                      jint,
                                      jint,
                                      jint,
                                      jint,
                                      jint,
                                      jint,
                                      jint,
                                      jint,
                                      jint,
                                      jint,
                                      jint,
                                      jint,
                                      jintArray,
                                      jintArray,
                                      jintArray,
                                      jint,
                                      jint,
                                      jint,
                                      jint,
                                      jint,
                                      jint,
                                      jint,
                                      jint);
using DispatchPlatformMessageFn =
    void (*)(JNIEnv*, jobject, jlong, jstring, jobject, jint, jint);
using DispatchEmptyPlatformMessageFn =
    void (*)(JNIEnv*, jobject, jlong, jstring, jint);
using DispatchPointerDataPacketFn =
    void (*)(JNIEnv*, jobject, jlong, jobject, jint);
using DispatchSemanticsActionFn =
    void (*)(JNIEnv*, jobject, jlong, jint, jint, jobject, jint);
using CleanupMessageDataFn = void (*)(JNIEnv*, jobject, jlong);
using InvokePlatformMessageResponseCallbackFn =
    void (*)(JNIEnv*, jobject, jlong, jint, jobject, jint);
using InvokePlatformMessageEmptyResponseCallbackFn = void (*)(JNIEnv*,
                                                              jobject,
                                                              jlong,
                                                              jint);
using RegisterTextureFn = void (*)(JNIEnv*, jobject, jlong, jlong, jobject);
using RegisterImageTextureFn =
    void (*)(JNIEnv*, jobject, jlong, jlong, jobject, jboolean);
using UnregisterTextureFn = void (*)(JNIEnv*, jobject, jlong, jlong);
using MarkTextureFrameAvailableFn = void (*)(JNIEnv*, jobject, jlong, jlong);

// Runs PlatformViewAndroid::Register against |mock_env| and returns the
// function registered for the FlutterJNI native called |name|, or nullptr.
void* RegisterNativesAndFind(MockJNIEnv& mock_env, const char* name) {
  // Arbitrary non-null handles. The mock env never dereferences them.
  const jclass kPlaceholderClass = reinterpret_cast<jclass>(100);
  const jfieldID kPlaceholderFieldID = reinterpret_cast<jfieldID>(200);
  const jmethodID kPlaceholderMethodID = reinterpret_cast<jmethodID>(300);

  EXPECT_CALL(mock_env, GetObjectRefType(_))
      .WillRepeatedly(Return(JNILocalRefType));
  EXPECT_CALL(mock_env, NewLocalRef(_)).WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteLocalRef(_)).WillRepeatedly(Return());
  EXPECT_CALL(mock_env, NewGlobalRef(_)).WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteGlobalRef(_)).WillRepeatedly(Return());
  EXPECT_CALL(mock_env, FindClass(_)).WillRepeatedly(Return(kPlaceholderClass));
  EXPECT_CALL(mock_env, GetFieldID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderFieldID));
  EXPECT_CALL(mock_env, GetMethodID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderMethodID));
  EXPECT_CALL(mock_env, GetStaticFieldID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderFieldID));
  EXPECT_CALL(mock_env, GetStaticMethodID(_, _, _))
      .WillRepeatedly(Return(kPlaceholderMethodID));
  EXPECT_CALL(mock_env, ExceptionCheck()).WillRepeatedly(Return(JNI_FALSE));

  void* found = nullptr;
  EXPECT_CALL(mock_env, RegisterNatives(_, _, _))
      .WillRepeatedly(
          [&](jclass clazz, const JNINativeMethod* methods, jint nMethods) {
            for (jint i = 0; i < nMethods; ++i) {
              if (strcmp(methods[i].name, name) == 0) {
                found = methods[i].fnPtr;
              }
            }
            return 0;
          });

  PlatformViewAndroid::Register(&mock_env);
  return found;
}

// The PlatformData held by the holder's runtime controller.
//
// Only valid on the UI thread. AndroidShellHolder merges the platform and UI
// threads by default (Settings::merged_platform_ui_thread), and the platform
// thread is the thread that created the holder, so in these tests the UI
// thread is the test thread. Shell applies semantics and accessibility updates
// with RunNowAndFlushMessages, so they are visible as soon as the native
// returns.
const PlatformData& RuntimePlatformData(const AndroidShellHolder& holder) {
  return holder.GetShellForTesting()
      ->GetEngine()
      ->GetRuntimeController()
      ->GetPlatformData();
}

std::unique_ptr<AndroidShellHolder> CreateHolderWithMergedThreads() {
  Settings settings;
  settings.enable_software_rendering = false;
  EXPECT_EQ(settings.merged_platform_ui_thread,
            Settings::MergedPlatformUIThread::kEnabled);
  return std::make_unique<AndroidShellHolder>(
      settings, std::make_shared<JNIMock>(),
      AndroidRenderingAPI::kImpellerOpenGLES);
}

}  // namespace

TEST_F(PlatformViewAndroidJNIImplTest,
       SetSemanticsEnabledUpdatesRuntimePlatformData) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();
  auto set_semantics_enabled = reinterpret_cast<SetSemanticsEnabledFn>(
      RegisterNativesAndFind(mock_env, "nativeSetSemanticsEnabled"));
  ASSERT_NE(set_semantics_enabled, nullptr);

  std::unique_ptr<AndroidShellHolder> holder = CreateHolderWithMergedThreads();
  ASSERT_TRUE(holder->IsValid());
  const jlong shell_holder = reinterpret_cast<jlong>(holder.get());
  jobject jcaller = reinterpret_cast<jobject>(123);

  EXPECT_FALSE(RuntimePlatformData(*holder).semantics_enabled);

  set_semantics_enabled(&mock_env, jcaller, shell_holder, JNI_TRUE);
  EXPECT_TRUE(RuntimePlatformData(*holder).semantics_enabled);

  set_semantics_enabled(&mock_env, jcaller, shell_holder, JNI_FALSE);
  EXPECT_FALSE(RuntimePlatformData(*holder).semantics_enabled);
}

TEST_F(PlatformViewAndroidJNIImplTest,
       SetAccessibilityFeaturesUpdatesRuntimePlatformData) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();
  auto set_accessibility_features =
      reinterpret_cast<SetAccessibilityFeaturesFn>(
          RegisterNativesAndFind(mock_env, "nativeSetAccessibilityFeatures"));
  ASSERT_NE(set_accessibility_features, nullptr);

  std::unique_ptr<AndroidShellHolder> holder = CreateHolderWithMergedThreads();
  ASSERT_TRUE(holder->IsValid());
  const jlong shell_holder = reinterpret_cast<jlong>(holder.get());
  jobject jcaller = reinterpret_cast<jobject>(123);

  EXPECT_EQ(RuntimePlatformData(*holder).accessibility_feature_flags_, 0);

  // The flags AccessibilityBridge sends with animations off, bold text and
  // high contrast, the same values Java packs in
  // AccessibilityBridge.AccessibilityFeature.
  const int32_t flags =
      static_cast<int32_t>(AccessibilityFeatureFlag::kDisableAnimations) |
      static_cast<int32_t>(AccessibilityFeatureFlag::kBoldText) |
      static_cast<int32_t>(AccessibilityFeatureFlag::kHighContrast);
  set_accessibility_features(&mock_env, jcaller, shell_holder, flags);
  EXPECT_EQ(RuntimePlatformData(*holder).accessibility_feature_flags_, flags);

  set_accessibility_features(&mock_env, jcaller, shell_holder, 0);
  EXPECT_EQ(RuntimePlatformData(*holder).accessibility_feature_flags_, 0);
}

// The natives below must reach the engine only through the holder's embedder
// API proc table. Each test replaces the proc with a mock that does not
// forward, so the runtime state must stay unchanged while the mock records
// the call. The tests above cover the same natives with the real procs.

TEST_F(PlatformViewAndroidJNIImplTest, SetSemanticsEnabledCallsEmbedderApi) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();
  auto set_semantics_enabled = reinterpret_cast<SetSemanticsEnabledFn>(
      RegisterNativesAndFind(mock_env, "nativeSetSemanticsEnabled"));
  ASSERT_NE(set_semantics_enabled, nullptr);

  std::unique_ptr<AndroidShellHolder> holder = CreateHolderWithMergedThreads();
  ASSERT_TRUE(holder->IsValid());
  std::vector<bool> calls;
  holder->GetEmbedderApiForTesting().UpdateSemanticsEnabled = MOCK_ENGINE_PROC(
      UpdateSemanticsEnabled,
      ([&calls](FLUTTER_API_SYMBOL(FlutterEngine) engine, bool enabled) {
        EXPECT_NE(engine, nullptr);
        calls.push_back(enabled);
        return kSuccess;
      }));

  set_semantics_enabled(&mock_env, reinterpret_cast<jobject>(123),
                        reinterpret_cast<jlong>(holder.get()), JNI_TRUE);

  EXPECT_THAT(calls, ElementsAre(true));
  EXPECT_FALSE(RuntimePlatformData(*holder).semantics_enabled);
}

TEST_F(PlatformViewAndroidJNIImplTest,
       SetAccessibilityFeaturesCallsEmbedderApi) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();
  auto set_accessibility_features =
      reinterpret_cast<SetAccessibilityFeaturesFn>(
          RegisterNativesAndFind(mock_env, "nativeSetAccessibilityFeatures"));
  ASSERT_NE(set_accessibility_features, nullptr);

  std::unique_ptr<AndroidShellHolder> holder = CreateHolderWithMergedThreads();
  ASSERT_TRUE(holder->IsValid());
  std::vector<int32_t> calls;
  holder->GetEmbedderApiForTesting().UpdateAccessibilityFeatures =
      MOCK_ENGINE_PROC(UpdateAccessibilityFeatures,
                       ([&calls](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                                 FlutterAccessibilityFeature features) {
                         EXPECT_NE(engine, nullptr);
                         calls.push_back(static_cast<int32_t>(features));
                         return kSuccess;
                       }));

  const int32_t flags =
      static_cast<int32_t>(AccessibilityFeatureFlag::kDisableAnimations) |
      static_cast<int32_t>(AccessibilityFeatureFlag::kBoldText);
  set_accessibility_features(&mock_env, reinterpret_cast<jobject>(123),
                             reinterpret_cast<jlong>(holder.get()), flags);

  EXPECT_THAT(calls, ElementsAre(flags));
  EXPECT_EQ(RuntimePlatformData(*holder).accessibility_feature_flags_, 0);
}

TEST_F(PlatformViewAndroidJNIImplTest, ScheduleFrameCallsEmbedderApi) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();
  auto schedule_frame = reinterpret_cast<ScheduleFrameFn>(
      RegisterNativesAndFind(mock_env, "nativeScheduleFrame"));
  ASSERT_NE(schedule_frame, nullptr);

  std::unique_ptr<AndroidShellHolder> holder = CreateHolderWithMergedThreads();
  ASSERT_TRUE(holder->IsValid());
  int calls = 0;
  holder->GetEmbedderApiForTesting().ScheduleFrame = MOCK_ENGINE_PROC(
      ScheduleFrame, ([&calls](FLUTTER_API_SYMBOL(FlutterEngine) engine) {
        EXPECT_NE(engine, nullptr);
        calls++;
        return kSuccess;
      }));

  schedule_frame(&mock_env, reinterpret_cast<jobject>(123),
                 reinterpret_cast<jlong>(holder.get()));

  EXPECT_EQ(calls, 1);
}

TEST_F(PlatformViewAndroidJNIImplTest, UpdateDisplayMetricsCallsEmbedderApi) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();
  auto update_display_metrics = reinterpret_cast<UpdateDisplayMetricsFn>(
      RegisterNativesAndFind(mock_env, "nativeUpdateDisplayMetrics"));
  ASSERT_NE(update_display_metrics, nullptr);

  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<JNIMock>();
  constexpr double kExpectedRefreshRate = 120.0;
  constexpr double kExpectedWidth = 1080.0;
  constexpr double kExpectedHeight = 2400.0;
  constexpr double kExpectedDensity = 3.0;
  EXPECT_CALL(*jni, GetDisplayRefreshRate())
      .WillOnce(Return(kExpectedRefreshRate));
  EXPECT_CALL(*jni, GetDisplayWidth()).WillOnce(Return(kExpectedWidth));
  EXPECT_CALL(*jni, GetDisplayHeight()).WillOnce(Return(kExpectedHeight));
  EXPECT_CALL(*jni, GetDisplayDensity()).WillOnce(Return(kExpectedDensity));

  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_TRUE(holder->IsValid());

  std::vector<FlutterEngineDisplay> displays;
  holder->GetEmbedderApiForTesting().NotifyDisplayUpdate = MOCK_ENGINE_PROC(
      NotifyDisplayUpdate,
      ([&displays](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                   FlutterEngineDisplaysUpdateType update_type,
                   const FlutterEngineDisplay* engine_displays,
                   size_t display_count) {
        EXPECT_NE(engine, nullptr);
        EXPECT_EQ(update_type, kFlutterEngineDisplaysUpdateTypeStartup);
        for (size_t i = 0; i < display_count; ++i) {
          displays.push_back(engine_displays[i]);
        }
        return kSuccess;
      }));

  update_display_metrics(&mock_env, reinterpret_cast<jobject>(123),
                         reinterpret_cast<jlong>(holder.get()));

  ASSERT_EQ(displays.size(), 1u);
  EXPECT_EQ(displays[0].display_id, 0u);
  EXPECT_TRUE(displays[0].single_display);
  EXPECT_DOUBLE_EQ(displays[0].refresh_rate, kExpectedRefreshRate);
  EXPECT_EQ(displays[0].width, static_cast<size_t>(kExpectedWidth));
  EXPECT_EQ(displays[0].height, static_cast<size_t>(kExpectedHeight));
  EXPECT_DOUBLE_EQ(displays[0].device_pixel_ratio, kExpectedDensity);
}

TEST_F(PlatformViewAndroidJNIImplTest,
       SetViewportMetricsCallsEmbedderApiWithDisplayFeaturesAndCornerRadii) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();
  auto set_viewport_metrics = reinterpret_cast<SetViewportMetricsFn>(
      RegisterNativesAndFind(mock_env, "nativeSetViewportMetrics"));
  ASSERT_NE(set_viewport_metrics, nullptr);

  const jintArray kBoundsArray = reinterpret_cast<jintArray>(0x401);
  const jintArray kTypeArray = reinterpret_cast<jintArray>(0x402);
  const jintArray kStateArray = reinterpret_cast<jintArray>(0x403);
  EXPECT_CALL(mock_env, GetArrayLength(kBoundsArray)).WillOnce(Return(4));
  EXPECT_CALL(mock_env, GetArrayLength(kTypeArray)).WillOnce(Return(1));
  EXPECT_CALL(mock_env, GetArrayLength(kStateArray)).WillOnce(Return(1));
  EXPECT_CALL(mock_env, GetIntArrayRegion(kBoundsArray, 0, 4, _))
      .WillOnce([](jintArray, jsize, jsize, jint* buf) {
        buf[0] = 10;
        buf[1] = 20;
        buf[2] = 30;
        buf[3] = 40;
      });
  EXPECT_CALL(mock_env, GetIntArrayRegion(kTypeArray, 0, 1, _))
      .WillOnce([](jintArray, jsize, jsize, jint* buf) {
        buf[0] = static_cast<jint>(kFlutterDisplayFeatureTypeHinge);
      });
  EXPECT_CALL(mock_env, GetIntArrayRegion(kStateArray, 0, 1, _))
      .WillOnce([](jintArray, jsize, jsize, jint* buf) {
        buf[0] =
            static_cast<jint>(kFlutterDisplayFeatureStatePostureHalfOpened);
      });

  std::unique_ptr<AndroidShellHolder> holder = CreateHolderWithMergedThreads();
  ASSERT_TRUE(holder->IsValid());

  bool called = false;
  FlutterWindowMetricsEvent captured = {};
  FlutterDisplayFeature captured_feature = {};
  FlutterDisplayCornerRadii captured_radii = {};
  bool had_corner_radii = false;
  holder->GetEmbedderApiForTesting().SendWindowMetricsEvent = MOCK_ENGINE_PROC(
      SendWindowMetricsEvent,
      ([&](FLUTTER_API_SYMBOL(FlutterEngine) engine,
           const FlutterWindowMetricsEvent* event) -> FlutterEngineResult {
        EXPECT_NE(engine, nullptr);
        EXPECT_NE(event, nullptr);
        if (event == nullptr) {
          return kInvalidArguments;
        }
        called = true;
        captured = *event;
        if (event->display_features_count > 0 &&
            event->display_features != nullptr &&
            event->display_features[0] != nullptr) {
          captured_feature = *event->display_features[0];
        }
        if (event->display_corner_radii != nullptr) {
          had_corner_radii = true;
          captured_radii = *event->display_corner_radii;
        }
        return kSuccess;
      }));

  set_viewport_metrics(&mock_env, reinterpret_cast<jobject>(123),
                       reinterpret_cast<jlong>(holder.get()),
                       /*devicePixelRatio=*/2.5f,
                       /*physicalWidth=*/1080,
                       /*physicalHeight=*/2400,
                       /*physicalPaddingTop=*/50,
                       /*physicalPaddingRight=*/10,
                       /*physicalPaddingBottom=*/80,
                       /*physicalPaddingLeft=*/15,
                       /*physicalViewInsetTop=*/0,
                       /*physicalViewInsetRight=*/0,
                       /*physicalViewInsetBottom=*/600,
                       /*physicalViewInsetLeft=*/0,
                       /*systemGestureInsetTop=*/12,
                       /*systemGestureInsetRight=*/24,
                       /*systemGestureInsetBottom=*/36,
                       /*systemGestureInsetLeft=*/48,
                       /*physicalTouchSlop=*/18, kBoundsArray, kTypeArray,
                       kStateArray,
                       /*physicalMinWidth=*/500,
                       /*physicalMaxWidth=*/1080,
                       /*physicalMinHeight=*/800,
                       /*physicalMaxHeight=*/2400,
                       /*physicalDisplayCornerRadiusTopLeft=*/32,
                       /*physicalDisplayCornerRadiusTopRight=*/33,
                       /*physicalDisplayCornerRadiusBottomRight=*/34,
                       /*physicalDisplayCornerRadiusBottomLeft=*/35);

  ASSERT_TRUE(called);
  EXPECT_EQ(captured.width, 1080u);
  EXPECT_EQ(captured.height, 2400u);
  EXPECT_DOUBLE_EQ(captured.pixel_ratio, 2.5);
  EXPECT_DOUBLE_EQ(captured.physical_padding.top, 50.0);
  EXPECT_DOUBLE_EQ(captured.physical_padding.right, 10.0);
  EXPECT_DOUBLE_EQ(captured.physical_padding.bottom, 80.0);
  EXPECT_DOUBLE_EQ(captured.physical_padding.left, 15.0);
  EXPECT_DOUBLE_EQ(captured.physical_view_inset_bottom, 600.0);
  EXPECT_DOUBLE_EQ(captured.physical_system_gesture_insets.top, 12.0);
  EXPECT_DOUBLE_EQ(captured.physical_system_gesture_insets.right, 24.0);
  EXPECT_DOUBLE_EQ(captured.physical_system_gesture_insets.bottom, 36.0);
  EXPECT_DOUBLE_EQ(captured.physical_system_gesture_insets.left, 48.0);
  EXPECT_DOUBLE_EQ(captured.physical_touch_slop, 18.0);
  EXPECT_TRUE(captured.has_constraints);
  EXPECT_EQ(captured.min_width_constraint, 500u);
  EXPECT_EQ(captured.max_width_constraint, 1080u);
  EXPECT_EQ(captured.min_height_constraint, 800u);
  EXPECT_EQ(captured.max_height_constraint, 2400u);
  ASSERT_TRUE(had_corner_radii);
  EXPECT_DOUBLE_EQ(captured_radii.top_left, 32.0);
  EXPECT_DOUBLE_EQ(captured_radii.top_right, 33.0);
  EXPECT_DOUBLE_EQ(captured_radii.bottom_right, 34.0);
  EXPECT_DOUBLE_EQ(captured_radii.bottom_left, 35.0);
  ASSERT_EQ(captured.display_features_count, 1u);
  EXPECT_DOUBLE_EQ(captured_feature.bounds.left, 10.0);
  EXPECT_DOUBLE_EQ(captured_feature.bounds.top, 20.0);
  EXPECT_DOUBLE_EQ(captured_feature.bounds.right, 30.0);
  EXPECT_DOUBLE_EQ(captured_feature.bounds.bottom, 40.0);
  EXPECT_EQ(captured_feature.type, kFlutterDisplayFeatureTypeHinge);
  EXPECT_EQ(captured_feature.state,
            kFlutterDisplayFeatureStatePostureHalfOpened);
}

TEST_F(PlatformViewAndroidJNIImplTest,
       SetViewportMetricsRejectsInvalidPixelRatioWithoutCallingEmbedderApi) {
  std::unique_ptr<AndroidShellHolder> holder = CreateHolderWithMergedThreads();
  ASSERT_TRUE(holder->IsValid());

  int calls = 0;
  holder->GetEmbedderApiForTesting().SendWindowMetricsEvent = MOCK_ENGINE_PROC(
      SendWindowMetricsEvent, ([&](FLUTTER_API_SYMBOL(FlutterEngine),
                                   const FlutterWindowMetricsEvent*) {
        calls++;
        return kSuccess;
      }));

  AndroidViewportMetrics invalid_metrics;
  invalid_metrics.device_pixel_ratio = 0.0;
  invalid_metrics.physical_width = 100;
  invalid_metrics.physical_height = 100;
  holder->SetViewportMetrics(0, invalid_metrics);
  EXPECT_EQ(calls, 0);
}

TEST_F(PlatformViewAndroidJNIImplTest,
       DispatchPlatformMessageAndEmptyMessageCallEmbedderApi) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();
  auto dispatch_msg = reinterpret_cast<DispatchPlatformMessageFn>(
      RegisterNativesAndFind(mock_env, "nativeDispatchPlatformMessage"));
  auto dispatch_empty = reinterpret_cast<DispatchEmptyPlatformMessageFn>(
      RegisterNativesAndFind(mock_env, "nativeDispatchEmptyPlatformMessage"));
  ASSERT_NE(dispatch_msg, nullptr);
  ASSERT_NE(dispatch_empty, nullptr);

  static constexpr jchar kChannelChars[] = {'t', 'e', 's', 't'};
  const jstring kJavaChannel = reinterpret_cast<jstring>(0x501);
  const jobject kJavaBuffer = reinterpret_cast<jobject>(0x502);
  const uint8_t kPayload[3] = {7, 8, 9};
  EXPECT_CALL(mock_env, GetStringChars(kJavaChannel, _))
      .WillRepeatedly(Return(kChannelChars));
  EXPECT_CALL(mock_env, GetStringLength(kJavaChannel))
      .WillRepeatedly(Return(4));
  EXPECT_CALL(mock_env, ReleaseStringChars(kJavaChannel, kChannelChars))
      .WillRepeatedly(Return());
  EXPECT_CALL(mock_env, GetDirectBufferAddress(kJavaBuffer))
      .WillRepeatedly(Return(const_cast<uint8_t*>(kPayload)));

  auto jni = std::make_shared<JNIMock>();
  Settings settings;
  settings.enable_software_rendering = false;
  auto holder = std::make_unique<AndroidShellHolder>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);
  ASSERT_TRUE(holder->IsValid());

  struct SentMessage {
    std::string channel;
    bool non_null_ptr = false;
    std::vector<uint8_t> bytes;
    bool has_response_handle = false;
  };
  std::vector<SentMessage> sent;
  FlutterDataCallback saved_callback = nullptr;
  void* saved_user_data = nullptr;
  int released_handles = 0;

  auto& api = holder->GetEmbedderApiForTesting();
  api.PlatformMessageCreateResponseHandle = MOCK_ENGINE_PROC(
      PlatformMessageCreateResponseHandle,
      ([&](FLUTTER_API_SYMBOL(FlutterEngine) engine,
           FlutterDataCallback data_callback, void* user_data,
           FlutterPlatformMessageResponseHandle** response_out) {
        saved_callback = data_callback;
        saved_user_data = user_data;
        *response_out =
            reinterpret_cast<FlutterPlatformMessageResponseHandle*>(0x7777);
        return kSuccess;
      }));
  api.PlatformMessageReleaseResponseHandle = MOCK_ENGINE_PROC(
      PlatformMessageReleaseResponseHandle,
      ([&](FLUTTER_API_SYMBOL(FlutterEngine),
           FlutterPlatformMessageResponseHandle* handle) {
        EXPECT_EQ(
            handle,
            reinterpret_cast<FlutterPlatformMessageResponseHandle*>(0x7777));
        released_handles++;
        return kSuccess;
      }));
  api.SendPlatformMessage = MOCK_ENGINE_PROC(
      SendPlatformMessage, ([&](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                                const FlutterPlatformMessage* message) {
        SentMessage m;
        m.channel = message->channel;
        m.non_null_ptr = (message->message != nullptr);
        if (message->message != nullptr && message->message_size > 0) {
          m.bytes.assign(message->message,
                         message->message + message->message_size);
        }
        m.has_response_handle = (message->response_handle != nullptr);
        sent.push_back(std::move(m));
        return kSuccess;
      }));

  // 1. Non-empty platform message with responseId = 42.
  dispatch_msg(&mock_env, reinterpret_cast<jobject>(123),
               reinterpret_cast<jlong>(holder.get()), kJavaChannel, kJavaBuffer,
               3, 42);
  ASSERT_EQ(sent.size(), 1u);
  EXPECT_EQ(sent[0].channel, "test");
  EXPECT_TRUE(sent[0].non_null_ptr);
  EXPECT_THAT(sent[0].bytes, ElementsAre(7, 8, 9));
  EXPECT_TRUE(sent[0].has_response_handle);
  EXPECT_EQ(released_handles, 1);

  // Simulate Dart replying to responseId 42 through the C-API data callback.
  ASSERT_NE(saved_callback, nullptr);
  const uint8_t kReplyBytes[2] = {0x11, 0x22};
  EXPECT_CALL(*jni, FlutterViewHandlePlatformMessageResponse(42, _))
      .WillOnce([](int id, std::unique_ptr<fml::Mapping> data) {
        ASSERT_NE(data, nullptr);
        ASSERT_EQ(data->GetSize(), 2u);
        EXPECT_EQ(data->GetMapping()[0], 0x11);
        EXPECT_EQ(data->GetMapping()[1], 0x22);
      });
  saved_callback(kReplyBytes, sizeof(kReplyBytes), saved_user_data);

  // 2. Empty platform message (`message == nullptr, size == 0`) with no
  //    response callback (`responseId == 0`).
  dispatch_empty(&mock_env, reinterpret_cast<jobject>(123),
                 reinterpret_cast<jlong>(holder.get()), kJavaChannel, 0);
  ASSERT_EQ(sent.size(), 2u);
  EXPECT_EQ(sent[1].channel, "test");
  EXPECT_FALSE(sent[1].non_null_ptr);
  EXPECT_TRUE(sent[1].bytes.empty());
  EXPECT_FALSE(sent[1].has_response_handle);
}

TEST_F(PlatformViewAndroidJNIImplTest,
       DispatchPointerDataPacketUnpacksAndCallsSendPointerEvent) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();
  auto dispatch_pointer = reinterpret_cast<DispatchPointerDataPacketFn>(
      RegisterNativesAndFind(mock_env, "nativeDispatchPointerDataPacket"));
  ASSERT_NE(dispatch_pointer, nullptr);

  // Build a 36-field (288-byte) serialized PointerData record matching
  // `AndroidTouchProcessor.java`.
  struct RawRecord {
    int64_t embedder_id;
    int64_t time_stamp;
    int64_t change;
    int64_t kind;
    int64_t signal_kind;
    int64_t device;
    int64_t pointer_identifier;
    double physical_x;
    double physical_y;
    double physical_delta_x;
    double physical_delta_y;
    int64_t buttons;
    int64_t obscured;
    int64_t synthesized;
    double pressure;
    double pressure_min;
    double pressure_max;
    double distance;
    double distance_max;
    double size;
    double radius_major;
    double radius_minor;
    double radius_min;
    double radius_max;
    double orientation;
    double tilt;
    int64_t platform_data;
    double scroll_delta_x;
    double scroll_delta_y;
    double pan_x;
    double pan_y;
    double pan_delta_x;
    double pan_delta_y;
    double scale;
    double rotation;
    int64_t view_id;
  };
  static_assert(sizeof(RawRecord) == 36 * sizeof(int64_t));

  RawRecord raw = {};
  raw.embedder_id = 777;
  raw.time_stamp = 5000;
  raw.change = 4;       // kDown
  raw.kind = 2;         // kStylus
  raw.signal_kind = 0;  // kNone
  raw.device = 3;
  raw.physical_x = 120.5;
  raw.physical_y = 340.25;
  raw.buttons = 1;
  raw.pressure = 0.8;
  raw.pressure_min = 0.0;
  raw.pressure_max = 1.0;
  raw.distance = 1.5;
  raw.distance_max = 9.0;
  raw.size = 0.4;
  raw.radius_major = 11.0;
  raw.radius_minor = 7.0;
  raw.radius_min = 0.5;
  raw.radius_max = 20.0;
  raw.orientation = 0.3;
  raw.tilt = 0.6;
  raw.platform_data = 99;
  raw.scale = 1.0;
  raw.view_id = 0;

  const jobject kJavaBuffer = reinterpret_cast<jobject>(0x601);
  EXPECT_CALL(mock_env, GetDirectBufferAddress(kJavaBuffer))
      .WillOnce(Return(&raw));

  std::unique_ptr<AndroidShellHolder> holder = CreateHolderWithMergedThreads();
  ASSERT_TRUE(holder->IsValid());

  std::vector<FlutterPointerEvent> events;
  holder->GetEmbedderApiForTesting().SendPointerEvent = MOCK_ENGINE_PROC(
      SendPointerEvent,
      ([&](FLUTTER_API_SYMBOL(FlutterEngine) engine,
           const FlutterPointerEvent* pointers, size_t events_count) {
        EXPECT_NE(engine, nullptr);
        for (size_t i = 0; i < events_count; ++i) {
          events.push_back(pointers[i]);
        }
        return kSuccess;
      }));

  dispatch_pointer(&mock_env, reinterpret_cast<jobject>(123),
                   reinterpret_cast<jlong>(holder.get()), kJavaBuffer,
                   static_cast<jint>(sizeof(RawRecord)));

  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].phase, FlutterPointerPhase::kDown);
  EXPECT_EQ(events[0].timestamp, 5000u);
  EXPECT_DOUBLE_EQ(events[0].x, 120.5);
  EXPECT_DOUBLE_EQ(events[0].y, 340.25);
  EXPECT_EQ(events[0].device, 3);
  EXPECT_EQ(events[0].device_kind, kFlutterPointerDeviceKindStylus);
  EXPECT_EQ(events[0].buttons, 1);
  EXPECT_DOUBLE_EQ(events[0].pressure, 0.8);
  EXPECT_DOUBLE_EQ(events[0].pressure_max, 1.0);
  EXPECT_DOUBLE_EQ(events[0].distance, 1.5);
  EXPECT_DOUBLE_EQ(events[0].distance_max, 9.0);
  EXPECT_DOUBLE_EQ(events[0].size, 0.4);
  EXPECT_DOUBLE_EQ(events[0].radius_major, 11.0);
  EXPECT_DOUBLE_EQ(events[0].radius_minor, 7.0);
  EXPECT_DOUBLE_EQ(events[0].radius_min, 0.5);
  EXPECT_DOUBLE_EQ(events[0].radius_max, 20.0);
  EXPECT_DOUBLE_EQ(events[0].orientation, 0.3);
  EXPECT_DOUBLE_EQ(events[0].tilt, 0.6);
  EXPECT_EQ(events[0].platform_data, 99);
  EXPECT_EQ(events[0].embedder_id, 777);
}

TEST_F(PlatformViewAndroidJNIImplTest,
       CleanupMessageDataInvokesPayloadDestructionCallback) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();
  auto cleanup_fn = reinterpret_cast<CleanupMessageDataFn>(
      RegisterNativesAndFind(mock_env, "nativeCleanupMessageData"));
  ASSERT_NE(cleanup_fn, nullptr);

  bool destroyed = false;
  auto* payload = new FlutterPlatformMessagePayload{
      .struct_size = sizeof(FlutterPlatformMessagePayload),
      .data = reinterpret_cast<const uint8_t*>(0x1),
      .data_size = 1,
      .user_data = &destroyed,
      .destruction_callback = [](void* ud) { *static_cast<bool*>(ud) = true; },
  };
  cleanup_fn(&mock_env, reinterpret_cast<jobject>(123),
             reinterpret_cast<jlong>(payload));
  EXPECT_TRUE(destroyed);
}

TEST_F(PlatformViewAndroidJNIImplTest,
       DispatchSemanticsActionCallsSendSemanticsAction) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();
  auto dispatch_semantics = reinterpret_cast<DispatchSemanticsActionFn>(
      RegisterNativesAndFind(mock_env, "nativeDispatchSemanticsAction"));
  ASSERT_NE(dispatch_semantics, nullptr);

  std::unique_ptr<AndroidShellHolder> holder = CreateHolderWithMergedThreads();
  ASSERT_TRUE(holder->IsValid());

  struct ActionCall {
    int32_t node_id;
    FlutterSemanticsAction action;
    std::vector<uint8_t> data;
    FlutterViewId view_id;
  };
  std::vector<ActionCall> calls;
  holder->GetEmbedderApiForTesting().SendSemanticsAction = MOCK_ENGINE_PROC(
      SendSemanticsAction, ([&](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                                const FlutterSendSemanticsActionInfo* info) {
        EXPECT_NE(engine, nullptr);
        EXPECT_NE(info, nullptr);
        ActionCall call;
        call.node_id = static_cast<int32_t>(info->node_id);
        call.action = info->action;
        if (info->data != nullptr && info->data_length > 0) {
          call.data.assign(info->data, info->data + info->data_length);
        }
        call.view_id = info->view_id;
        calls.push_back(std::move(call));
        return kSuccess;
      }));

  // 1. Action without arguments (args == nullptr).
  dispatch_semantics(&mock_env, reinterpret_cast<jobject>(123),
                     reinterpret_cast<jlong>(holder.get()), /*id=*/7,
                     /*action=*/static_cast<jint>(kFlutterSemanticsActionTap),
                     /*args=*/nullptr, /*args_position=*/0);
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].node_id, 7);
  EXPECT_EQ(calls[0].action, kFlutterSemanticsActionTap);
  EXPECT_TRUE(calls[0].data.empty());
  EXPECT_EQ(calls[0].view_id, 0);

  // 2. Action with direct ByteBuffer arguments.
  const uint8_t kArgsBytes[4] = {0xAA, 0xBB, 0xCC, 0xDD};
  const jobject kJavaArgs = reinterpret_cast<jobject>(0x701);
  EXPECT_CALL(mock_env, GetDirectBufferAddress(kJavaArgs))
      .WillOnce(Return(const_cast<uint8_t*>(kArgsBytes)));

  dispatch_semantics(
      &mock_env, reinterpret_cast<jobject>(123),
      reinterpret_cast<jlong>(holder.get()), /*id=*/12,
      /*action=*/static_cast<jint>(kFlutterSemanticsActionSetText),
      /*args=*/kJavaArgs, /*args_position=*/4);
  ASSERT_EQ(calls.size(), 2u);
  EXPECT_EQ(calls[1].node_id, 12);
  EXPECT_EQ(calls[1].action, kFlutterSemanticsActionSetText);
  EXPECT_THAT(calls[1].data, ElementsAre(0xAA, 0xBB, 0xCC, 0xDD));
  EXPECT_EQ(calls[1].view_id, 0);
}

TEST_F(PlatformViewAndroidJNIImplTest, NotifyLowMemoryWarningCallsEmbedderApi) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();
  auto notify_low_memory = reinterpret_cast<ScheduleFrameFn>(
      RegisterNativesAndFind(mock_env, "nativeNotifyLowMemoryWarning"));
  ASSERT_NE(notify_low_memory, nullptr);

  std::unique_ptr<AndroidShellHolder> holder = CreateHolderWithMergedThreads();
  ASSERT_TRUE(holder->IsValid());
  int calls = 0;
  holder->GetEmbedderApiForTesting().NotifyLowMemoryWarning =
      MOCK_ENGINE_PROC(NotifyLowMemoryWarning,
                       ([&calls](FLUTTER_API_SYMBOL(FlutterEngine) engine) {
                         EXPECT_NE(engine, nullptr);
                         calls++;
                         return kSuccess;
                       }));

  notify_low_memory(&mock_env, reinterpret_cast<jobject>(123),
                    reinterpret_cast<jlong>(holder.get()));
  EXPECT_EQ(calls, 1);
}

// The load order is exercised with an injected loader rather than real
// dlopen(): the property under test is purely the ordering (first-to-last,
// stop at the first that loads), and a fake loader makes that deterministic
// and free of any platform- or system-library-specific behavior. Whether a
// given path format is actually loadable is covered end-to-end by
// dev/integration_tests/deferred_components_test.
TEST(FindFirstLoadableLibraryTest, TriesInOrderAndStopsAtFirstSuccess) {
  std::vector<std::string> attempted;
  void* const handle = reinterpret_cast<void*>(0x1234);
  auto opener = [&](const std::string& path) -> void* {
    attempted.push_back(path);
    return path == "b" ? handle : nullptr;
  };
  EXPECT_EQ(FindFirstLoadableLibrary({"a", "b", "c"}, opener), handle);
  // "c" is never attempted because "b" already loaded.
  EXPECT_THAT(attempted, ElementsAre("a", "b"));
}

TEST(FindFirstLoadableLibraryTest, TriesAllAndReturnsNullWhenNoneLoad) {
  std::vector<std::string> attempted;
  auto opener = [&](const std::string& path) -> void* {
    attempted.push_back(path);
    return nullptr;
  };
  EXPECT_EQ(FindFirstLoadableLibrary({"a", "b"}, opener), nullptr);
  EXPECT_THAT(attempted, ElementsAre("a", "b"));
}

TEST(FindFirstLoadableLibraryTest, EmptyReturnsNull) {
  EXPECT_EQ(FindFirstLoadableLibrary(
                {}, [](const std::string&) -> void* { return nullptr; }),
            nullptr);
}

TEST_F(PlatformViewAndroidJNIImplTest,
       RegisterTextureCallsEmbedderApiIncludingZeroId) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();
  auto register_texture = reinterpret_cast<RegisterTextureFn>(
      RegisterNativesAndFind(mock_env, "nativeRegisterTexture"));
  ASSERT_NE(register_texture, nullptr);

  std::unique_ptr<AndroidShellHolder> holder = CreateHolderWithMergedThreads();
  ASSERT_TRUE(holder->IsValid());

  std::vector<int64_t> registered_ids;
  holder->GetEmbedderApiForTesting().RegisterExternalTexture = MOCK_ENGINE_PROC(
      RegisterExternalTexture,
      ([&registered_ids](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                         int64_t texture_identifier) {
        EXPECT_NE(engine, nullptr);
        registered_ids.push_back(texture_identifier);
        return kSuccess;
      }));

  const jlong shell_holder = reinterpret_cast<jlong>(holder.get());
  jobject jcaller = reinterpret_cast<jobject>(123);
  jobject jsurface_texture = reinterpret_cast<jobject>(4001);

  constexpr jlong kFirstTextureId = 0;
  constexpr jlong kSecondTextureId = 5;
  register_texture(&mock_env, jcaller, shell_holder, kFirstTextureId,
                   jsurface_texture);
  register_texture(&mock_env, jcaller, shell_holder, kSecondTextureId,
                   jsurface_texture);

  EXPECT_THAT(registered_ids, ElementsAre(kFirstTextureId, kSecondTextureId));
}

TEST_F(PlatformViewAndroidJNIImplTest, RegisterImageTextureCallsEmbedderApi) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();
  auto register_image_texture = reinterpret_cast<RegisterImageTextureFn>(
      RegisterNativesAndFind(mock_env, "nativeRegisterImageTexture"));
  ASSERT_NE(register_image_texture, nullptr);

  std::unique_ptr<AndroidShellHolder> holder = CreateHolderWithMergedThreads();
  ASSERT_TRUE(holder->IsValid());

  std::vector<int64_t> registered_ids;
  holder->GetEmbedderApiForTesting().RegisterExternalTexture = MOCK_ENGINE_PROC(
      RegisterExternalTexture,
      ([&registered_ids](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                         int64_t texture_identifier) {
        EXPECT_NE(engine, nullptr);
        registered_ids.push_back(texture_identifier);
        return kSuccess;
      }));

  const jlong shell_holder = reinterpret_cast<jlong>(holder.get());
  jobject jcaller = reinterpret_cast<jobject>(123);
  jobject jimage_entry = reinterpret_cast<jobject>(4002);

  constexpr jlong kKeepAliveTextureId = 0;
  constexpr jlong kResetTextureId = 1;
  register_image_texture(&mock_env, jcaller, shell_holder, kKeepAliveTextureId,
                         jimage_entry, JNI_FALSE);
  register_image_texture(&mock_env, jcaller, shell_holder, kResetTextureId,
                         jimage_entry, JNI_TRUE);

  EXPECT_THAT(registered_ids,
              ElementsAre(kKeepAliveTextureId, kResetTextureId));
}

TEST_F(PlatformViewAndroidJNIImplTest,
       UnregisterAndMarkTextureFrameAvailableCallEmbedderApi) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();
  auto mark_frame_available = reinterpret_cast<MarkTextureFrameAvailableFn>(
      RegisterNativesAndFind(mock_env, "nativeMarkTextureFrameAvailable"));
  auto unregister_texture = reinterpret_cast<UnregisterTextureFn>(
      RegisterNativesAndFind(mock_env, "nativeUnregisterTexture"));
  ASSERT_NE(mark_frame_available, nullptr);
  ASSERT_NE(unregister_texture, nullptr);

  std::unique_ptr<AndroidShellHolder> holder = CreateHolderWithMergedThreads();
  ASSERT_TRUE(holder->IsValid());

  std::vector<int64_t> marked_ids;
  std::vector<int64_t> unregistered_ids;
  holder->GetEmbedderApiForTesting().MarkExternalTextureFrameAvailable =
      MOCK_ENGINE_PROC(MarkExternalTextureFrameAvailable,
                       ([&marked_ids](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                                      int64_t texture_identifier) {
                         EXPECT_NE(engine, nullptr);
                         marked_ids.push_back(texture_identifier);
                         return kSuccess;
                       }));
  holder->GetEmbedderApiForTesting().UnregisterExternalTexture =
      MOCK_ENGINE_PROC(
          UnregisterExternalTexture,
          ([&unregistered_ids](FLUTTER_API_SYMBOL(FlutterEngine) engine,
                               int64_t texture_identifier) {
            EXPECT_NE(engine, nullptr);
            unregistered_ids.push_back(texture_identifier);
            return kSuccess;
          }));

  const jlong shell_holder = reinterpret_cast<jlong>(holder.get());
  jobject jcaller = reinterpret_cast<jobject>(123);

  constexpr jlong kZeroTextureId = 0;
  constexpr jlong kNonZeroTextureId = 9;
  mark_frame_available(&mock_env, jcaller, shell_holder, kZeroTextureId);
  mark_frame_available(&mock_env, jcaller, shell_holder, kNonZeroTextureId);
  unregister_texture(&mock_env, jcaller, shell_holder, kZeroTextureId);
  unregister_texture(&mock_env, jcaller, shell_holder, kNonZeroTextureId);

  EXPECT_THAT(marked_ids, ElementsAre(kZeroTextureId, kNonZeroTextureId));
  EXPECT_THAT(unregistered_ids, ElementsAre(kZeroTextureId, kNonZeroTextureId));
}

TEST_F(PlatformViewAndroidJNIImplTest,
       RegisterMarkAndUnregisterTextureRealProcsForwardToEngine) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();
  auto register_texture = reinterpret_cast<RegisterTextureFn>(
      RegisterNativesAndFind(mock_env, "nativeRegisterTexture"));
  auto register_image_texture = reinterpret_cast<RegisterImageTextureFn>(
      RegisterNativesAndFind(mock_env, "nativeRegisterImageTexture"));
  auto mark_frame_available = reinterpret_cast<MarkTextureFrameAvailableFn>(
      RegisterNativesAndFind(mock_env, "nativeMarkTextureFrameAvailable"));
  auto unregister_texture = reinterpret_cast<UnregisterTextureFn>(
      RegisterNativesAndFind(mock_env, "nativeUnregisterTexture"));
  ASSERT_NE(register_texture, nullptr);
  ASSERT_NE(register_image_texture, nullptr);
  ASSERT_NE(mark_frame_available, nullptr);
  ASSERT_NE(unregister_texture, nullptr);

  std::unique_ptr<AndroidShellHolder> holder = CreateHolderWithMergedThreads();
  ASSERT_TRUE(holder->IsValid());

  const jlong shell_holder = reinterpret_cast<jlong>(holder.get());
  jobject jcaller = reinterpret_cast<jobject>(123);
  jobject jsurface_texture = reinterpret_cast<jobject>(4003);
  jobject jimage_entry = reinterpret_cast<jobject>(4004);

  // Android's FlutterRenderer starts assigning texture IDs at 0.
  constexpr jlong kSurfaceTextureId = 0;
  constexpr jlong kImageTextureId = 1;
  register_texture(&mock_env, jcaller, shell_holder, kSurfaceTextureId,
                   jsurface_texture);
  register_image_texture(&mock_env, jcaller, shell_holder, kImageTextureId,
                         jimage_entry, JNI_FALSE);
  mark_frame_available(&mock_env, jcaller, shell_holder, kSurfaceTextureId);
  mark_frame_available(&mock_env, jcaller, shell_holder, kImageTextureId);
  unregister_texture(&mock_env, jcaller, shell_holder, kSurfaceTextureId);
  unregister_texture(&mock_env, jcaller, shell_holder, kImageTextureId);
}

}  // namespace testing
}  // namespace flutter
