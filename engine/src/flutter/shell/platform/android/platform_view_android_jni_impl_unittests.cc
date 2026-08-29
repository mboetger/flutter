// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "flutter/fml/platform/android/jni_util.h"
#include "flutter/fml/platform/android/jni_weak_ref.h"
#include "flutter/fml/platform/android/scoped_java_ref.h"
#include "flutter/shell/platform/android/android_engine.h"
#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/android/jni/jni_mock.h"
#include "flutter/shell/platform/android/jni/mock_jni_env.h"
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

  PlatformViewAndroidJNIImpl::Register(&mock_env);
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

  PlatformViewAndroidJNIImpl::Register(&mock_env);

  ASSERT_NE(set_viewport_metrics, nullptr);

  EXPECT_CALL(mock_env, GetArrayLength(_)).WillRepeatedly(Return(0));
  EXPECT_CALL(mock_env, GetIntArrayRegion(_, _, _, _)).Times(0);

  AndroidSettings settings;
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

class PlatformViewAndroidJNIMultiBackendMatrixTest
    : public PlatformViewAndroidJNIImplTest,
      public ::testing::WithParamInterface<AndroidRenderingAPI> {
 protected:
  void SetUp() override { rendering_api_ = GetParam(); }

  AndroidRenderingAPI rendering_api_ = AndroidRenderingAPI::kImpellerOpenGLES;
};

static std::string JNIMatrixTestName(
    const ::testing::TestParamInfo<AndroidRenderingAPI>& info) {
  AndroidRenderingAPI api = info.param;
  switch (api) {
    case AndroidRenderingAPI::kSoftware:
      return "Software";
    case AndroidRenderingAPI::kSkiaOpenGLES:
      return "SkiaOpenGLES";
    case AndroidRenderingAPI::kImpellerOpenGLES:
      return "ImpellerOpenGLES";
    case AndroidRenderingAPI::kImpellerVulkan:
      return "ImpellerVulkan";
    case AndroidRenderingAPI::kImpellerAutoselect:
      return "ImpellerAutoselect";
  }
}

TEST_P(PlatformViewAndroidJNIMultiBackendMatrixTest, FullJNIDispatchSuite) {
  MockJNIEnvProvider env_provider;
  MockJNIEnv& mock_env = env_provider.env();

  typedef jlong (*AttachJNIFn)(JNIEnv*, jclass, jobject);
  typedef void (*DestroyJNIFn)(JNIEnv*, jobject, jlong);
  typedef void (*SurfaceCreatedFn)(JNIEnv*, jobject, jlong, jobject);
  typedef void (*SurfaceWindowChangedFn)(JNIEnv*, jobject, jlong, jobject);
  typedef void (*SurfaceChangedFn)(JNIEnv*, jobject, jlong, jint, jint);
  typedef void (*SurfaceDestroyedFn)(JNIEnv*, jobject, jlong);
  typedef void (*SetViewportMetricsFn)(
      JNIEnv*, jobject, jlong, jfloat, jint, jint, jint, jint, jint, jint, jint,
      jint, jint, jint, jint, jint, jint, jint, jint, jintArray, jintArray,
      jintArray, jint, jint, jint, jint, jint, jint, jint, jint);
  typedef void (*UpdateDisplayMetricsFn)(JNIEnv*, jobject, jlong);
  typedef jboolean (*IsSurfaceControlEnabledFn)(JNIEnv*, jobject, jlong);
  typedef jobject (*GetBitmapFn)(JNIEnv*, jobject, jlong);
  typedef void (*DispatchEmptyPlatformMessageFn)(JNIEnv*, jobject, jlong,
                                                 jstring, jint);
  typedef void (*InvokeEmptyResponseCallbackFn)(JNIEnv*, jobject, jlong, jint);
  typedef void (*SetSemanticsEnabledFn)(JNIEnv*, jobject, jlong, jboolean);
  typedef void (*SetAccessibilityFeaturesFn)(JNIEnv*, jobject, jlong, jint);
  typedef void (*RegisterTextureFn)(JNIEnv*, jobject, jlong, jlong, jobject);
  typedef void (*RegisterImageTextureFn)(JNIEnv*, jobject, jlong, jlong,
                                         jobject, jboolean);
  typedef void (*MarkTextureFrameAvailableFn)(JNIEnv*, jobject, jlong, jlong);
  typedef void (*UnregisterTextureFn)(JNIEnv*, jobject, jlong, jlong);
  typedef void (*ScheduleFrameFn)(JNIEnv*, jobject, jlong);
  typedef void (*NotifyLowMemoryWarningFn)(JNIEnv*, jobject, jlong);

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

  AttachJNIFn attach_jni = nullptr;
  DestroyJNIFn destroy_jni = nullptr;
  SurfaceCreatedFn surface_created = nullptr;
  SurfaceWindowChangedFn surface_window_changed = nullptr;
  SurfaceChangedFn surface_changed = nullptr;
  SurfaceDestroyedFn surface_destroyed = nullptr;
  SetViewportMetricsFn set_viewport_metrics = nullptr;
  UpdateDisplayMetricsFn update_display_metrics = nullptr;
  IsSurfaceControlEnabledFn is_surface_control_enabled = nullptr;
  GetBitmapFn get_bitmap = nullptr;
  DispatchEmptyPlatformMessageFn dispatch_empty_msg = nullptr;
  InvokeEmptyResponseCallbackFn invoke_empty_response = nullptr;
  SetSemanticsEnabledFn set_semantics_enabled = nullptr;
  SetAccessibilityFeaturesFn set_accessibility_features = nullptr;
  RegisterTextureFn register_texture = nullptr;
  RegisterImageTextureFn register_image_texture = nullptr;
  MarkTextureFrameAvailableFn mark_texture_available = nullptr;
  UnregisterTextureFn unregister_texture = nullptr;
  ScheduleFrameFn schedule_frame = nullptr;
  NotifyLowMemoryWarningFn notify_low_memory = nullptr;

  EXPECT_CALL(mock_env, RegisterNatives(_, _, _))
      .WillRepeatedly([&](jclass clazz, const JNINativeMethod* methods,
                          jint nMethods) {
        for (jint i = 0; i < nMethods; ++i) {
          if (strcmp(methods[i].name, "nativeAttach") == 0) {
            attach_jni = reinterpret_cast<AttachJNIFn>(methods[i].fnPtr);
          } else if (strcmp(methods[i].name, "nativeDestroy") == 0) {
            destroy_jni = reinterpret_cast<DestroyJNIFn>(methods[i].fnPtr);
          } else if (strcmp(methods[i].name, "nativeSurfaceCreated") == 0) {
            surface_created =
                reinterpret_cast<SurfaceCreatedFn>(methods[i].fnPtr);
          } else if (strcmp(methods[i].name, "nativeSurfaceWindowChanged") ==
                     0) {
            surface_window_changed =
                reinterpret_cast<SurfaceWindowChangedFn>(methods[i].fnPtr);
          } else if (strcmp(methods[i].name, "nativeSurfaceChanged") == 0) {
            surface_changed =
                reinterpret_cast<SurfaceChangedFn>(methods[i].fnPtr);
          } else if (strcmp(methods[i].name, "nativeSurfaceDestroyed") == 0) {
            surface_destroyed =
                reinterpret_cast<SurfaceDestroyedFn>(methods[i].fnPtr);
          } else if (strcmp(methods[i].name, "nativeSetViewportMetrics") == 0) {
            set_viewport_metrics =
                reinterpret_cast<SetViewportMetricsFn>(methods[i].fnPtr);
          } else if (strcmp(methods[i].name, "nativeUpdateDisplayMetrics") ==
                     0) {
            update_display_metrics =
                reinterpret_cast<UpdateDisplayMetricsFn>(methods[i].fnPtr);
          } else if (strcmp(methods[i].name, "nativeIsSurfaceControlEnabled") ==
                     0) {
            is_surface_control_enabled =
                reinterpret_cast<IsSurfaceControlEnabledFn>(methods[i].fnPtr);
          } else if (strcmp(methods[i].name, "nativeGetBitmap") == 0) {
            get_bitmap = reinterpret_cast<GetBitmapFn>(methods[i].fnPtr);
          } else if (strcmp(methods[i].name,
                            "nativeDispatchEmptyPlatformMessage") == 0) {
            dispatch_empty_msg =
                reinterpret_cast<DispatchEmptyPlatformMessageFn>(
                    methods[i].fnPtr);
          } else if (strcmp(
                         methods[i].name,
                         "nativeInvokePlatformMessageEmptyResponseCallback") ==
                     0) {
            invoke_empty_response =
                reinterpret_cast<InvokeEmptyResponseCallbackFn>(
                    methods[i].fnPtr);
          } else if (strcmp(methods[i].name, "nativeSetSemanticsEnabled") ==
                     0) {
            set_semantics_enabled =
                reinterpret_cast<SetSemanticsEnabledFn>(methods[i].fnPtr);
          } else if (strcmp(methods[i].name,
                            "nativeSetAccessibilityFeatures") == 0) {
            set_accessibility_features =
                reinterpret_cast<SetAccessibilityFeaturesFn>(methods[i].fnPtr);
          } else if (strcmp(methods[i].name, "nativeRegisterTexture") == 0) {
            register_texture =
                reinterpret_cast<RegisterTextureFn>(methods[i].fnPtr);
          } else if (strcmp(methods[i].name, "nativeRegisterImageTexture") ==
                     0) {
            register_image_texture =
                reinterpret_cast<RegisterImageTextureFn>(methods[i].fnPtr);
          } else if (strcmp(methods[i].name,
                            "nativeMarkTextureFrameAvailable") == 0) {
            mark_texture_available =
                reinterpret_cast<MarkTextureFrameAvailableFn>(methods[i].fnPtr);
          } else if (strcmp(methods[i].name, "nativeUnregisterTexture") == 0) {
            unregister_texture =
                reinterpret_cast<UnregisterTextureFn>(methods[i].fnPtr);
          } else if (strcmp(methods[i].name, "nativeScheduleFrame") == 0) {
            schedule_frame =
                reinterpret_cast<ScheduleFrameFn>(methods[i].fnPtr);
          } else if (strcmp(methods[i].name, "nativeNotifyLowMemoryWarning") ==
                     0) {
            notify_low_memory =
                reinterpret_cast<NotifyLowMemoryWarningFn>(methods[i].fnPtr);
          }
        }
        return 0;
      });

  PlatformViewAndroidJNIImpl::Register(&mock_env);

  ASSERT_NE(destroy_jni, nullptr);
  ASSERT_NE(set_viewport_metrics, nullptr);
  ASSERT_NE(set_semantics_enabled, nullptr);
  ASSERT_NE(set_accessibility_features, nullptr);
  ASSERT_NE(schedule_frame, nullptr);
  ASSERT_NE(notify_low_memory, nullptr);
  ASSERT_NE(surface_destroyed, nullptr);

  EXPECT_CALL(mock_env, GetArrayLength(_)).WillRepeatedly(Return(0));

  jobject jcaller = reinterpret_cast<jobject>(123);
  jintArray bounds = reinterpret_cast<jintArray>(456);
  jintArray type = reinterpret_cast<jintArray>(789);
  jintArray state = reinterpret_cast<jintArray>(1011);

  AndroidSettings settings;
  if (rendering_api_ == AndroidRenderingAPI::kSoftware) {
    settings.enable_software_rendering = true;
    settings.enable_impeller = false;
  } else if (rendering_api_ == AndroidRenderingAPI::kSkiaOpenGLES) {
    settings.enable_software_rendering = false;
    settings.enable_impeller = false;
  } else {
    settings.enable_software_rendering = false;
    settings.enable_impeller = true;
  }

  auto engine = std::make_unique<AndroidEngine>(
      settings, std::make_shared<JNIMock>(), rendering_api_);
  jlong shell_holder_ptr = reinterpret_cast<jlong>(engine.release());

  ASSERT_NE(shell_holder_ptr, 0);

  // Surface and frame lifecycle
  surface_changed(&mock_env, jcaller, shell_holder_ptr, 1080, 1920);
  surface_destroyed(&mock_env, jcaller, shell_holder_ptr);

  // Metrics and displays
  set_viewport_metrics(&mock_env, jcaller, shell_holder_ptr, 1.0f, 100, 100, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, bounds, type, state,
                       0, 0, 0, 0, 0, 0, 0, 0);
  update_display_metrics(&mock_env, jcaller, shell_holder_ptr);
  EXPECT_FALSE(
      is_surface_control_enabled(&mock_env, jcaller, shell_holder_ptr));

  // Semantics & Accessibility
  set_semantics_enabled(&mock_env, jcaller, shell_holder_ptr, JNI_TRUE);
  set_accessibility_features(&mock_env, jcaller, shell_holder_ptr, 0x7);
  set_semantics_enabled(&mock_env, jcaller, shell_holder_ptr, JNI_FALSE);

  // Textures and Frame Scheduling
  register_texture(&mock_env, jcaller, shell_holder_ptr, 1001, nullptr);
  mark_texture_available(&mock_env, jcaller, shell_holder_ptr, 1001);
  unregister_texture(&mock_env, jcaller, shell_holder_ptr, 1001);

  register_image_texture(&mock_env, jcaller, shell_holder_ptr, 1002, nullptr,
                         JNI_FALSE);
  mark_texture_available(&mock_env, jcaller, shell_holder_ptr, 1002);
  unregister_texture(&mock_env, jcaller, shell_holder_ptr, 1002);

  schedule_frame(&mock_env, jcaller, shell_holder_ptr);

  // Platform Messages & Memory
  dispatch_empty_msg(&mock_env, jcaller, shell_holder_ptr, nullptr, 0);
  invoke_empty_response(&mock_env, jcaller, shell_holder_ptr, 10);
  notify_low_memory(&mock_env, jcaller, shell_holder_ptr);

  // GetBitmap uninitialized returns nullptr safely
  EXPECT_EQ(get_bitmap(&mock_env, jcaller, shell_holder_ptr), nullptr);

  // Cleanup
  destroy_jni(&mock_env, jcaller, shell_holder_ptr);
}

INSTANTIATE_TEST_SUITE_P(
    Matrix,
    PlatformViewAndroidJNIMultiBackendMatrixTest,
    ::testing::Values(AndroidRenderingAPI::kSoftware,
                      AndroidRenderingAPI::kSkiaOpenGLES,
                      AndroidRenderingAPI::kImpellerOpenGLES,
                      AndroidRenderingAPI::kImpellerVulkan,
                      AndroidRenderingAPI::kImpellerAutoselect),
    JNIMatrixTestName);

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

}  // namespace testing
}  // namespace flutter
