// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "flutter/fml/platform/android/jni_util.h"
#include "flutter/fml/platform/android/jni_weak_ref.h"
#include "flutter/fml/platform/android/scoped_java_ref.h"
#include "flutter/shell/platform/android/android_engine.h"
#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "flutter/shell/platform/android/jni/mock_jni_env.h"
#include "flutter/shell/platform/android/platform_view_android.h"
#include "flutter/shell/platform/android/platform_view_android_jni_impl.h"

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

  void TearDown() override { FlutterMain::ResetEmbedderAPIEnabledForTesting(); }

  static void* GetNativeMethod(const std::string& name) {
    auto it = native_methods_.find(name);
    return it != native_methods_.end() ? it->second : nullptr;
  }

 private:
  friend class MockJNIEnvProvider;
  static MockJavaVM jvm_;
  static std::map<std::string, void*> native_methods_;
  static void SetUpJVM();
};

MockJavaVM PlatformViewAndroidJNIImplTest::jvm_;
std::map<std::string, void*> PlatformViewAndroidJNIImplTest::native_methods_;

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
  EXPECT_CALL(mock_env, NewWeakGlobalRef(_)).WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteWeakGlobalRef(_)).WillRepeatedly(Return());
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
              native_methods_[methods[i].name] = methods[i].fnPtr;
            }
            return 0;
          });

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
  EXPECT_CALL(mock_env, NewWeakGlobalRef(_)).WillRepeatedly(ReturnArg<0>());
  EXPECT_CALL(mock_env, DeleteWeakGlobalRef(_)).WillRepeatedly(Return());
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

  FlutterMain::SetEmbedderAPIEnabledForTesting(false);
  Settings settings;
  settings.enable_software_rendering = false;
  auto jni = std::make_shared<JNIMock>();
  auto engine = std::make_unique<AndroidEngine>(
      settings, jni, AndroidRenderingAPI::kImpellerOpenGLES);

  jobject jcaller = reinterpret_cast<jobject>(123);
  jintArray bounds = reinterpret_cast<jintArray>(456);
  jintArray type = reinterpret_cast<jintArray>(789);
  jintArray state = reinterpret_cast<jintArray>(1011);

  set_viewport_metrics(&mock_env, jcaller,
                       reinterpret_cast<jlong>(engine.get()), 1.0f, 100, 100, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, bounds, type, state,
                       0, 0, 0, 0, 0, 0, 0, 0);
}

TEST_F(PlatformViewAndroidJNIImplTest, DualPathAttachAndDestroy) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  using AttachFn = jlong (*)(JNIEnv*, jclass, jobject);
  using DestroyFn = void (*)(JNIEnv*, jobject, jlong);

  auto native_attach =
      reinterpret_cast<AttachFn>(GetNativeMethod("nativeAttach"));
  auto native_destroy =
      reinterpret_cast<DestroyFn>(GetNativeMethod("nativeDestroy"));

  ASSERT_NE(native_attach, nullptr);
  ASSERT_NE(native_destroy, nullptr);

  // Synthetic placeholder for Java FlutterJNI instance.
  jobject dummy_flutter_jni = reinterpret_cast<jobject>(0x1234);

  // 1. Test Legacy Path (flag = false)
  FlutterMain::SetEmbedderAPIEnabledForTesting(false);
  jlong legacy_handle = native_attach(&mock_env, nullptr, dummy_flutter_jni);
  EXPECT_NE(legacy_handle, 0);
  native_destroy(&mock_env, nullptr, legacy_handle);

  // 2. Test Embedder API Path (flag = true)
  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  jlong embedder_handle = native_attach(&mock_env, nullptr, dummy_flutter_jni);
  EXPECT_NE(embedder_handle, 0);
  native_destroy(&mock_env, nullptr, embedder_handle);
}

TEST_F(PlatformViewAndroidJNIImplTest, DualPathSurfaceLifecycle) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  using AttachFn = jlong (*)(JNIEnv*, jclass, jobject);
  using DestroyFn = void (*)(JNIEnv*, jobject, jlong);
  using SurfaceDestroyedFn = void (*)(JNIEnv*, jobject, jlong);
  using SurfaceChangedFn = void (*)(JNIEnv*, jobject, jlong, jint, jint);

  auto native_attach =
      reinterpret_cast<AttachFn>(GetNativeMethod("nativeAttach"));
  auto native_destroy =
      reinterpret_cast<DestroyFn>(GetNativeMethod("nativeDestroy"));
  auto native_surface_destroyed = reinterpret_cast<SurfaceDestroyedFn>(
      GetNativeMethod("nativeSurfaceDestroyed"));
  auto native_surface_changed = reinterpret_cast<SurfaceChangedFn>(
      GetNativeMethod("nativeSurfaceChanged"));

  ASSERT_NE(native_attach, nullptr);
  ASSERT_NE(native_destroy, nullptr);
  ASSERT_NE(native_surface_destroyed, nullptr);
  ASSERT_NE(native_surface_changed, nullptr);

  // Standard surface viewport dimensions for test.
  constexpr jint kTestWidth = 800;
  constexpr jint kTestHeight = 600;

  // Synthetic placeholder for Java FlutterJNI instance.
  jobject dummy_flutter_jni = reinterpret_cast<jobject>(0x1234);

  // 1. Legacy path
  FlutterMain::SetEmbedderAPIEnabledForTesting(false);
  jlong legacy_handle = native_attach(&mock_env, nullptr, dummy_flutter_jni);
  ASSERT_NE(legacy_handle, 0);
  native_surface_changed(&mock_env, nullptr, legacy_handle, kTestWidth,
                         kTestHeight);
  native_surface_destroyed(&mock_env, nullptr, legacy_handle);
  native_destroy(&mock_env, nullptr, legacy_handle);

  // 2. Embedder API path
  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  jlong embedder_handle = native_attach(&mock_env, nullptr, dummy_flutter_jni);
  ASSERT_NE(embedder_handle, 0);
  native_surface_changed(&mock_env, nullptr, embedder_handle, kTestWidth,
                         kTestHeight);
  native_surface_destroyed(&mock_env, nullptr, embedder_handle);
  native_destroy(&mock_env, nullptr, embedder_handle);
}

TEST_F(PlatformViewAndroidJNIImplTest, DualPathSetViewportMetrics) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  using AttachFn = jlong (*)(JNIEnv*, jclass, jobject);
  using DestroyFn = void (*)(JNIEnv*, jobject, jlong);
  using SetViewportMetricsFn = void (*)(
      JNIEnv*, jobject, jlong, jfloat, jint, jint, jint, jint, jint, jint, jint,
      jint, jint, jint, jint, jint, jint, jint, jint, jintArray, jintArray,
      jintArray, jint, jint, jint, jint, jint, jint, jint, jint);
  using UpdateDisplayMetricsFn = void (*)(JNIEnv*, jobject, jlong);
  using IsSurfaceControlEnabledFn = bool (*)(JNIEnv*, jobject, jlong);

  auto native_attach =
      reinterpret_cast<AttachFn>(GetNativeMethod("nativeAttach"));
  auto native_destroy =
      reinterpret_cast<DestroyFn>(GetNativeMethod("nativeDestroy"));
  auto set_viewport_metrics = reinterpret_cast<SetViewportMetricsFn>(
      GetNativeMethod("nativeSetViewportMetrics"));
  auto update_display_metrics = reinterpret_cast<UpdateDisplayMetricsFn>(
      GetNativeMethod("nativeUpdateDisplayMetrics"));
  auto is_surface_control = reinterpret_cast<IsSurfaceControlEnabledFn>(
      GetNativeMethod("nativeIsSurfaceControlEnabled"));

  ASSERT_NE(native_attach, nullptr);
  ASSERT_NE(native_destroy, nullptr);
  ASSERT_NE(set_viewport_metrics, nullptr);
  ASSERT_NE(update_display_metrics, nullptr);
  ASSERT_NE(is_surface_control, nullptr);

  EXPECT_CALL(mock_env, GetArrayLength(_)).WillRepeatedly(Return(0));

  // Synthetic placeholder for Java FlutterJNI instance.
  jobject dummy_flutter_jni = reinterpret_cast<jobject>(0x1234);

  // 1. Legacy path
  FlutterMain::SetEmbedderAPIEnabledForTesting(false);
  jlong legacy_handle = native_attach(&mock_env, nullptr, dummy_flutter_jni);
  ASSERT_NE(legacy_handle, 0);
  set_viewport_metrics(&mock_env, nullptr, legacy_handle, 2.0f, 1080, 1920, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, nullptr, nullptr,
                       nullptr, 0, 0, 0, 0, 0, 0, 0, 0);
  update_display_metrics(&mock_env, nullptr, legacy_handle);
  EXPECT_FALSE(is_surface_control(&mock_env, nullptr, legacy_handle));
  native_destroy(&mock_env, nullptr, legacy_handle);

  // 2. Embedder API path
  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  jlong embedder_handle = native_attach(&mock_env, nullptr, dummy_flutter_jni);
  ASSERT_NE(embedder_handle, 0);
  set_viewport_metrics(&mock_env, nullptr, embedder_handle, 2.0f, 1080, 1920, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, nullptr, nullptr,
                       nullptr, 0, 0, 0, 0, 0, 0, 0, 0);
  update_display_metrics(&mock_env, nullptr, embedder_handle);
  EXPECT_FALSE(is_surface_control(&mock_env, nullptr, embedder_handle));
  native_destroy(&mock_env, nullptr, embedder_handle);
}

TEST_F(PlatformViewAndroidJNIImplTest, DualPathPlatformMessaging) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  using AttachFn = jlong (*)(JNIEnv*, jclass, jobject);
  using DestroyFn = void (*)(JNIEnv*, jobject, jlong);
  using DispatchPlatformMessageFn =
      void (*)(JNIEnv*, jobject, jlong, jstring, jobject, jint, jint);
  using DispatchEmptyPlatformMessageFn =
      void (*)(JNIEnv*, jobject, jlong, jstring, jint);
  using InvokePlatformMessageResponseCallbackFn =
      void (*)(JNIEnv*, jobject, jlong, jint, jobject, jint);
  using InvokePlatformMessageEmptyResponseCallbackFn =
      void (*)(JNIEnv*, jobject, jlong, jint);

  auto native_attach =
      reinterpret_cast<AttachFn>(GetNativeMethod("nativeAttach"));
  auto native_destroy =
      reinterpret_cast<DestroyFn>(GetNativeMethod("nativeDestroy"));
  auto native_dispatch_msg = reinterpret_cast<DispatchPlatformMessageFn>(
      GetNativeMethod("nativeDispatchPlatformMessage"));
  auto native_dispatch_empty_msg =
      reinterpret_cast<DispatchEmptyPlatformMessageFn>(
          GetNativeMethod("nativeDispatchEmptyPlatformMessage"));
  auto native_invoke_response =
      reinterpret_cast<InvokePlatformMessageResponseCallbackFn>(
          GetNativeMethod("nativeInvokePlatformMessageResponseCallback"));
  auto native_invoke_empty_response =
      reinterpret_cast<InvokePlatformMessageEmptyResponseCallbackFn>(
          GetNativeMethod("nativeInvokePlatformMessageEmptyResponseCallback"));

  ASSERT_NE(native_attach, nullptr);
  ASSERT_NE(native_destroy, nullptr);
  ASSERT_NE(native_dispatch_msg, nullptr);
  ASSERT_NE(native_dispatch_empty_msg, nullptr);
  ASSERT_NE(native_invoke_response, nullptr);
  ASSERT_NE(native_invoke_empty_response, nullptr);

  // Synthetic placeholder for Java FlutterJNI instance.
  jobject dummy_flutter_jni = reinterpret_cast<jobject>(0x1234);

  EXPECT_CALL(mock_env, GetStringUTFChars(_, _))
      .WillRepeatedly(Return("test_channel"));
  EXPECT_CALL(mock_env, ReleaseStringUTFChars(_, _)).WillRepeatedly(Return());

  // Rationale: Test response ID and byte payload dimensions.
  constexpr jint kResponseId = 42;
  constexpr jint kPayloadSize = 4;
  uint8_t payload[kPayloadSize] = {1, 2, 3, 4};
  EXPECT_CALL(mock_env, GetDirectBufferAddress(_))
      .WillRepeatedly(Return(payload));

  // 1. Legacy path
  FlutterMain::SetEmbedderAPIEnabledForTesting(false);
  jlong legacy_handle = native_attach(&mock_env, nullptr, dummy_flutter_jni);
  ASSERT_NE(legacy_handle, 0);
  native_dispatch_empty_msg(&mock_env, nullptr, legacy_handle, nullptr,
                            kResponseId);
  native_dispatch_msg(&mock_env, nullptr, legacy_handle, nullptr, nullptr,
                      kPayloadSize, kResponseId);
  native_destroy(&mock_env, nullptr, legacy_handle);

  // 2. Embedder API path
  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  jlong embedder_handle = native_attach(&mock_env, nullptr, dummy_flutter_jni);
  ASSERT_NE(embedder_handle, 0);
  native_dispatch_empty_msg(&mock_env, nullptr, embedder_handle, nullptr,
                            kResponseId);
  native_dispatch_msg(&mock_env, nullptr, embedder_handle, nullptr, nullptr,
                      kPayloadSize, kResponseId);
  native_destroy(&mock_env, nullptr, embedder_handle);
}

TEST_F(PlatformViewAndroidJNIImplTest, DualPathSemanticsAndAccessibility) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  using AttachFn = jlong (*)(JNIEnv*, jclass, jobject);
  using DestroyFn = void (*)(JNIEnv*, jobject, jlong);
  using DispatchSemanticsActionFn =
      void (*)(JNIEnv*, jobject, jlong, jint, jint, jobject, jint);
  using SetSemanticsEnabledFn = void (*)(JNIEnv*, jobject, jlong, jboolean);
  using SetAccessibilityFeaturesFn = void (*)(JNIEnv*, jobject, jlong, jint);

  auto native_attach =
      reinterpret_cast<AttachFn>(GetNativeMethod("nativeAttach"));
  auto native_destroy =
      reinterpret_cast<DestroyFn>(GetNativeMethod("nativeDestroy"));
  auto native_semantics_action = reinterpret_cast<DispatchSemanticsActionFn>(
      GetNativeMethod("nativeDispatchSemanticsAction"));
  auto native_set_semantics = reinterpret_cast<SetSemanticsEnabledFn>(
      GetNativeMethod("nativeSetSemanticsEnabled"));
  auto native_set_a11y = reinterpret_cast<SetAccessibilityFeaturesFn>(
      GetNativeMethod("nativeSetAccessibilityFeatures"));

  ASSERT_NE(native_attach, nullptr);
  ASSERT_NE(native_destroy, nullptr);
  ASSERT_NE(native_semantics_action, nullptr);
  ASSERT_NE(native_set_semantics, nullptr);
  ASSERT_NE(native_set_a11y, nullptr);

  // Synthetic placeholder for Java FlutterJNI instance.
  jobject dummy_flutter_jni = reinterpret_cast<jobject>(0x1234);

  // Rationale: Test semantics node ID, action ID (tap = 1), and flags bitmask
  // (1).
  constexpr jint kNodeId = 100;
  constexpr jint kActionId = 1;
  constexpr jint kFlags = 1;

  // 1. Legacy path
  FlutterMain::SetEmbedderAPIEnabledForTesting(false);
  jlong legacy_handle = native_attach(&mock_env, nullptr, dummy_flutter_jni);
  ASSERT_NE(legacy_handle, 0);
  native_set_semantics(&mock_env, nullptr, legacy_handle, JNI_TRUE);
  native_set_a11y(&mock_env, nullptr, legacy_handle, kFlags);
  native_semantics_action(&mock_env, nullptr, legacy_handle, kNodeId, kActionId,
                          nullptr, 0);
  native_destroy(&mock_env, nullptr, legacy_handle);

  // 2. Embedder API path
  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  jlong embedder_handle = native_attach(&mock_env, nullptr, dummy_flutter_jni);
  ASSERT_NE(embedder_handle, 0);
  native_set_semantics(&mock_env, nullptr, embedder_handle, JNI_TRUE);
  native_set_a11y(&mock_env, nullptr, embedder_handle, kFlags);
  native_semantics_action(&mock_env, nullptr, embedder_handle, kNodeId,
                          kActionId, nullptr, 0);
  native_destroy(&mock_env, nullptr, embedder_handle);
}

TEST_F(PlatformViewAndroidJNIImplTest, DualPathTextureAndMemoryManagement) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  using AttachFn = jlong (*)(JNIEnv*, jclass, jobject);
  using DestroyFn = void (*)(JNIEnv*, jobject, jlong);
  using RegisterTextureFn = void (*)(JNIEnv*, jobject, jlong, jlong, jobject);
  using MarkTextureFrameAvailableFn = void (*)(JNIEnv*, jobject, jlong, jlong);
  using UnregisterTextureFn = void (*)(JNIEnv*, jobject, jlong, jlong);
  using NotifyLowMemoryWarningFn = void (*)(JNIEnv*, jobject, jlong);

  auto native_attach =
      reinterpret_cast<AttachFn>(GetNativeMethod("nativeAttach"));
  auto native_destroy =
      reinterpret_cast<DestroyFn>(GetNativeMethod("nativeDestroy"));
  auto native_register_tex = reinterpret_cast<RegisterTextureFn>(
      GetNativeMethod("nativeRegisterTexture"));
  auto native_mark_frame = reinterpret_cast<MarkTextureFrameAvailableFn>(
      GetNativeMethod("nativeMarkTextureFrameAvailable"));
  auto native_unregister_tex = reinterpret_cast<UnregisterTextureFn>(
      GetNativeMethod("nativeUnregisterTexture"));
  auto native_low_mem = reinterpret_cast<NotifyLowMemoryWarningFn>(
      GetNativeMethod("nativeNotifyLowMemoryWarning"));

  ASSERT_NE(native_attach, nullptr);
  ASSERT_NE(native_destroy, nullptr);
  ASSERT_NE(native_register_tex, nullptr);
  ASSERT_NE(native_mark_frame, nullptr);
  ASSERT_NE(native_unregister_tex, nullptr);
  ASSERT_NE(native_low_mem, nullptr);

  // Synthetic placeholder for Java FlutterJNI instance.
  jobject dummy_flutter_jni = reinterpret_cast<jobject>(0x1234);
  // Rationale: Arbitrary texture ID.
  constexpr jlong kTextureId = 55;

  // 1. Legacy path
  FlutterMain::SetEmbedderAPIEnabledForTesting(false);
  jlong legacy_handle = native_attach(&mock_env, nullptr, dummy_flutter_jni);
  ASSERT_NE(legacy_handle, 0);
  native_register_tex(&mock_env, nullptr, legacy_handle, kTextureId,
                      dummy_flutter_jni);
  native_mark_frame(&mock_env, nullptr, legacy_handle, kTextureId);
  native_unregister_tex(&mock_env, nullptr, legacy_handle, kTextureId);
  native_low_mem(&mock_env, nullptr, legacy_handle);
  native_destroy(&mock_env, nullptr, legacy_handle);

  // 2. Embedder API path
  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  jlong embedder_handle = native_attach(&mock_env, nullptr, dummy_flutter_jni);
  ASSERT_NE(embedder_handle, 0);
  native_register_tex(&mock_env, nullptr, embedder_handle, kTextureId,
                      dummy_flutter_jni);
  native_mark_frame(&mock_env, nullptr, embedder_handle, kTextureId);
  native_unregister_tex(&mock_env, nullptr, embedder_handle, kTextureId);
  native_low_mem(&mock_env, nullptr, embedder_handle);
  native_destroy(&mock_env, nullptr, embedder_handle);
}

TEST_F(PlatformViewAndroidJNIImplTest, DualPathPointerDataPacket) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  using AttachFn = jlong (*)(JNIEnv*, jclass, jobject);
  using DestroyFn = void (*)(JNIEnv*, jobject, jlong);
  using DispatchPointerDataPacketFn =
      void (*)(JNIEnv*, jobject, jlong, jobject, jint);

  auto native_attach =
      reinterpret_cast<AttachFn>(GetNativeMethod("nativeAttach"));
  auto native_destroy =
      reinterpret_cast<DestroyFn>(GetNativeMethod("nativeDestroy"));
  auto native_pointer = reinterpret_cast<DispatchPointerDataPacketFn>(
      GetNativeMethod("nativeDispatchPointerDataPacket"));

  ASSERT_NE(native_attach, nullptr);
  ASSERT_NE(native_destroy, nullptr);
  ASSERT_NE(native_pointer, nullptr);

  // Synthetic placeholder for Java FlutterJNI instance.
  jobject dummy_flutter_jni = reinterpret_cast<jobject>(0x1234);

  // 288 bytes per PointerData record.
  constexpr size_t kPointerPacketSize = 288;
  std::vector<uint8_t> packet_data(kPointerPacketSize, 0);
  EXPECT_CALL(mock_env, GetDirectBufferAddress(_))
      .WillRepeatedly(Return(packet_data.data()));

  // 1. Legacy path
  FlutterMain::SetEmbedderAPIEnabledForTesting(false);
  jlong legacy_handle = native_attach(&mock_env, nullptr, dummy_flutter_jni);
  ASSERT_NE(legacy_handle, 0);
  native_pointer(&mock_env, nullptr, legacy_handle, nullptr,
                 kPointerPacketSize);
  native_destroy(&mock_env, nullptr, legacy_handle);

  // 2. Embedder API path
  FlutterMain::SetEmbedderAPIEnabledForTesting(true);
  jlong embedder_handle = native_attach(&mock_env, nullptr, dummy_flutter_jni);
  ASSERT_NE(embedder_handle, 0);
  native_pointer(&mock_env, nullptr, embedder_handle, nullptr,
                 kPointerPacketSize);
  native_destroy(&mock_env, nullptr, embedder_handle);
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

struct JNIMatrixConfig {
  AndroidRenderingAPI rendering_api;
  bool embedder_api_enabled;
};

class PlatformViewAndroidJNIMatrixTest
    : public ::testing::TestWithParam<JNIMatrixConfig> {
 public:
  static void SetUpTestSuite() {
    PlatformViewAndroidJNIImplTest::SetUpTestSuite();
  }

  static void* GetNativeMethod(const std::string& name) {
    return PlatformViewAndroidJNIImplTest::GetNativeMethod(name);
  }

 protected:
  void SetUp() override {
    FlutterMain::SetEmbedderAPIEnabledForTesting(
        GetParam().embedder_api_enabled);
  }

  void TearDown() override { FlutterMain::ResetEmbedderAPIEnabledForTesting(); }
};

TEST_P(PlatformViewAndroidJNIMatrixTest, MatrixAttachDestroyAndLifecycle) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  using AttachFn = jlong (*)(JNIEnv*, jclass, jobject);
  using DestroyFn = void (*)(JNIEnv*, jobject, jlong);
  using SurfaceChangedFn = void (*)(JNIEnv*, jobject, jlong, jint, jint);
  using SurfaceDestroyedFn = void (*)(JNIEnv*, jobject, jlong);

  auto native_attach =
      reinterpret_cast<AttachFn>(GetNativeMethod("nativeAttach"));
  auto native_destroy =
      reinterpret_cast<DestroyFn>(GetNativeMethod("nativeDestroy"));
  auto native_surface_changed = reinterpret_cast<SurfaceChangedFn>(
      GetNativeMethod("nativeSurfaceChanged"));
  auto native_surface_destroyed = reinterpret_cast<SurfaceDestroyedFn>(
      GetNativeMethod("nativeSurfaceDestroyed"));

  ASSERT_NE(native_attach, nullptr);
  ASSERT_NE(native_destroy, nullptr);
  ASSERT_NE(native_surface_changed, nullptr);
  ASSERT_NE(native_surface_destroyed, nullptr);

  // Rationale: Standard 1080x1920 test viewport dimensions.
  constexpr jint kWidth = 1080;
  constexpr jint kHeight = 1920;
  jobject dummy_flutter_jni = reinterpret_cast<jobject>(0x1234);

  jlong handle = native_attach(&mock_env, nullptr, dummy_flutter_jni);
  ASSERT_NE(handle, 0);

  native_surface_changed(&mock_env, nullptr, handle, kWidth, kHeight);
  native_surface_destroyed(&mock_env, nullptr, handle);
  native_destroy(&mock_env, nullptr, handle);
}

TEST_P(PlatformViewAndroidJNIMatrixTest, MatrixViewportAndInputPipeline) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  using AttachFn = jlong (*)(JNIEnv*, jclass, jobject);
  using DestroyFn = void (*)(JNIEnv*, jobject, jlong);
  using SetViewportMetricsFn = void (*)(
      JNIEnv*, jobject, jlong, jfloat, jint, jint, jint, jint, jint, jint, jint,
      jint, jint, jint, jint, jint, jint, jint, jint, jintArray, jintArray,
      jintArray, jint, jint, jint, jint, jint, jint, jint, jint);
  using DispatchPointerDataPacketFn =
      void (*)(JNIEnv*, jobject, jlong, jobject, jint);

  auto native_attach =
      reinterpret_cast<AttachFn>(GetNativeMethod("nativeAttach"));
  auto native_destroy =
      reinterpret_cast<DestroyFn>(GetNativeMethod("nativeDestroy"));
  auto set_viewport_metrics = reinterpret_cast<SetViewportMetricsFn>(
      GetNativeMethod("nativeSetViewportMetrics"));
  auto dispatch_pointer = reinterpret_cast<DispatchPointerDataPacketFn>(
      GetNativeMethod("nativeDispatchPointerDataPacket"));

  ASSERT_NE(native_attach, nullptr);
  ASSERT_NE(native_destroy, nullptr);
  ASSERT_NE(set_viewport_metrics, nullptr);
  ASSERT_NE(dispatch_pointer, nullptr);

  EXPECT_CALL(mock_env, GetArrayLength(_)).WillRepeatedly(Return(0));

  // Rationale: 288 bytes per PointerData record.
  constexpr size_t kPointerPacketSize = 288;
  std::vector<uint8_t> packet_data(kPointerPacketSize, 0);
  EXPECT_CALL(mock_env, GetDirectBufferAddress(_))
      .WillRepeatedly(Return(packet_data.data()));

  jobject dummy_flutter_jni = reinterpret_cast<jobject>(0x1234);
  jlong handle = native_attach(&mock_env, nullptr, dummy_flutter_jni);
  ASSERT_NE(handle, 0);

  // Rationale: 1080x1920 with 2.0x DPR, 48px top padding, 96px bottom padding.
  set_viewport_metrics(&mock_env, nullptr, handle, 2.0f, 1080, 1920, 48, 96, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, nullptr, nullptr, nullptr,
                       0, 0, 0, 0, 0, 0, 0, 0);

  dispatch_pointer(&mock_env, nullptr, handle, nullptr, kPointerPacketSize);
  native_destroy(&mock_env, nullptr, handle);
}

TEST_P(PlatformViewAndroidJNIMatrixTest, MatrixMessagingAndAccessibility) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  using AttachFn = jlong (*)(JNIEnv*, jclass, jobject);
  using DestroyFn = void (*)(JNIEnv*, jobject, jlong);
  using DispatchPlatformMessageFn =
      void (*)(JNIEnv*, jobject, jlong, jstring, jobject, jint, jint);
  using SetSemanticsEnabledFn = void (*)(JNIEnv*, jobject, jlong, jboolean);
  using SetAccessibilityFeaturesFn = void (*)(JNIEnv*, jobject, jlong, jint);
  using DispatchSemanticsActionFn =
      void (*)(JNIEnv*, jobject, jlong, jint, jint, jobject, jint);

  auto native_attach =
      reinterpret_cast<AttachFn>(GetNativeMethod("nativeAttach"));
  auto native_destroy =
      reinterpret_cast<DestroyFn>(GetNativeMethod("nativeDestroy"));
  auto dispatch_msg = reinterpret_cast<DispatchPlatformMessageFn>(
      GetNativeMethod("nativeDispatchPlatformMessage"));
  auto set_semantics = reinterpret_cast<SetSemanticsEnabledFn>(
      GetNativeMethod("nativeSetSemanticsEnabled"));
  auto set_a11y = reinterpret_cast<SetAccessibilityFeaturesFn>(
      GetNativeMethod("nativeSetAccessibilityFeatures"));
  auto semantics_action = reinterpret_cast<DispatchSemanticsActionFn>(
      GetNativeMethod("nativeDispatchSemanticsAction"));

  ASSERT_NE(native_attach, nullptr);
  ASSERT_NE(native_destroy, nullptr);
  ASSERT_NE(dispatch_msg, nullptr);
  ASSERT_NE(set_semantics, nullptr);
  ASSERT_NE(set_a11y, nullptr);
  ASSERT_NE(semantics_action, nullptr);

  EXPECT_CALL(mock_env, GetStringUTFChars(_, _))
      .WillRepeatedly(Return("flutter/matrix_channel"));
  EXPECT_CALL(mock_env, ReleaseStringUTFChars(_, _)).WillRepeatedly(Return());

  // Rationale: 4-byte payload and response ID 99.
  constexpr jint kResponseId = 99;
  constexpr jint kPayloadSize = 4;
  uint8_t payload[kPayloadSize] = {0xAA, 0xBB, 0xCC, 0xDD};
  EXPECT_CALL(mock_env, GetDirectBufferAddress(_))
      .WillRepeatedly(Return(payload));

  jobject dummy_flutter_jni = reinterpret_cast<jobject>(0x1234);
  jlong handle = native_attach(&mock_env, nullptr, dummy_flutter_jni);
  ASSERT_NE(handle, 0);

  dispatch_msg(&mock_env, nullptr, handle, nullptr, nullptr, kPayloadSize,
               kResponseId);
  set_semantics(&mock_env, nullptr, handle, JNI_TRUE);
  // Rationale: Accessibility feature flag 1.
  set_a11y(&mock_env, nullptr, handle, 1);
  // Rationale: Node ID 0, tap action ID 1.
  semantics_action(&mock_env, nullptr, handle, 0, 1, nullptr, 0);

  native_destroy(&mock_env, nullptr, handle);
}

INSTANTIATE_TEST_SUITE_P(
    JNIDispatchMatrix,
    PlatformViewAndroidJNIMatrixTest,
    ::testing::Values(
        JNIMatrixConfig{AndroidRenderingAPI::kImpellerOpenGLES, true},
        JNIMatrixConfig{AndroidRenderingAPI::kImpellerOpenGLES, false},
        JNIMatrixConfig{AndroidRenderingAPI::kImpellerVulkan, true},
        JNIMatrixConfig{AndroidRenderingAPI::kImpellerVulkan, false},
        JNIMatrixConfig{AndroidRenderingAPI::kSoftware, true},
        JNIMatrixConfig{AndroidRenderingAPI::kSoftware, false}),
    [](const ::testing::TestParamInfo<JNIMatrixConfig>& info) {
      std::string api_name;
      switch (info.param.rendering_api) {
        case AndroidRenderingAPI::kImpellerOpenGLES:
          api_name = "OpenGLES";
          break;
        case AndroidRenderingAPI::kImpellerVulkan:
          api_name = "Vulkan";
          break;
        case AndroidRenderingAPI::kSoftware:
          api_name = "Software";
          break;
        case AndroidRenderingAPI::kSkiaOpenGLES:
          api_name = "SkiaOpenGLES";
          break;
        case AndroidRenderingAPI::kImpellerAutoselect:
          api_name = "ImpellerAutoselect";
          break;
      }
      return api_name + (info.param.embedder_api_enabled ? "_EmbedderAPI"
                                                         : "_LegacyShell");
    });

}  // namespace testing
}  // namespace flutter
