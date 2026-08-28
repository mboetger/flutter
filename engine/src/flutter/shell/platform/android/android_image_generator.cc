// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_image_generator.h"

#include <android/bitmap.h>
#include <cstring>
#include <memory>
#include <utility>

#include "flutter/fml/platform/android/jni_util.h"

namespace flutter {

static fml::jni::ScopedJavaGlobalRef<jclass>* g_flutter_jni_class = nullptr;
static jmethodID g_decode_image_method = nullptr;

AndroidImageGenerator::~AndroidImageGenerator() = default;

AndroidImageGenerator::AndroidImageGenerator(std::vector<uint8_t> data)
    : data_(std::move(data)) {}

bool AndroidImageGenerator::GetImageInfo(FlutterImageInfo* info_out) {
  header_decoded_latch_.Wait();
  if (info_out == nullptr || width_ <= 0 || height_ <= 0) {
    return false;
  }
  info_out->struct_size = sizeof(FlutterImageInfo);
  info_out->width = width_;
  info_out->height = height_;
  info_out->frame_count = 1;
  info_out->repetition_count = 1;
  return true;
}

bool AndroidImageGenerator::GetPixels(const FlutterImageInfo* info,
                                      void* pixels,
                                      size_t row_bytes) {
  fully_decoded_latch_.Wait();
  if (software_decoded_data_.empty() || pixels == nullptr) {
    return false;
  }
  size_t required_bytes = static_cast<size_t>(width_) * height_ * 4;
  if (software_decoded_data_.size() < required_bytes) {
    return false;
  }
  std::memcpy(pixels, software_decoded_data_.data(), required_bytes);
  return true;
}

void AndroidImageGenerator::DecodeImage() {
  DoDecodeImage();
  header_decoded_latch_.Signal();
  fully_decoded_latch_.Signal();
}

void AndroidImageGenerator::DoDecodeImage() {
  FML_DCHECK(g_flutter_jni_class);
  FML_DCHECK(g_decode_image_method);

  JNIEnv* env = fml::jni::AttachCurrentThread();
  fml::jni::ScopedJavaLocalFrame scoped_local_reference_frame(env);

  jobject direct_buffer = env->NewDirectByteBuffer(
      const_cast<void*>(reinterpret_cast<const void*>(data_.data())),
      data_.size());

  auto bitmap = std::make_unique<fml::jni::ScopedJavaGlobalRef<jobject>>(
      env, env->CallStaticObjectMethod(g_flutter_jni_class->obj(),
                                       g_decode_image_method, direct_buffer,
                                       reinterpret_cast<jlong>(this)));
  FML_CHECK(fml::jni::CheckException(env));

  if (bitmap->is_null()) {
    return;
  }

  AndroidBitmapInfo info;
  [[maybe_unused]] int status;
  if ((status = AndroidBitmap_getInfo(env, bitmap->obj(), &info)) < 0) {
    FML_DLOG(ERROR) << "Failed to get bitmap info, status=" << status;
    return;
  }
  FML_DCHECK(info.format == ANDROID_BITMAP_FORMAT_RGBA_8888);

  void* pixel_lock = nullptr;
  if ((status = AndroidBitmap_lockPixels(env, bitmap->obj(), &pixel_lock)) <
      0) {
    FML_DLOG(ERROR) << "Failed to lock pixels, error=" << status;
    return;
  }

  size_t byte_count = info.width * info.height * 4;
  software_decoded_data_.resize(byte_count);
  std::memcpy(software_decoded_data_.data(), pixel_lock, byte_count);
  AndroidBitmap_unlockPixels(env, bitmap->obj());
}

bool AndroidImageGenerator::Register(JNIEnv* env) {
  g_flutter_jni_class = new fml::jni::ScopedJavaGlobalRef<jclass>(
      env, env->FindClass("io/flutter/embedding/engine/FlutterJNI"));
  FML_DCHECK(!g_flutter_jni_class->is_null());

  g_decode_image_method = env->GetStaticMethodID(
      g_flutter_jni_class->obj(), "decodeImage",
      "(Ljava/nio/ByteBuffer;J)Landroid/graphics/Bitmap;");
  FML_DCHECK(g_decode_image_method);

  static const JNINativeMethod header_decoded_method = {
      .name = "nativeImageHeaderCallback",
      .signature = "(JII)V",
      .fnPtr = reinterpret_cast<void*>(
          &AndroidImageGenerator::NativeImageHeaderCallback),
  };
  if (env->RegisterNatives(g_flutter_jni_class->obj(), &header_decoded_method,
                           1) != 0) {
    FML_LOG(ERROR)
        << "Failed to register FlutterJNI.nativeImageHeaderCallback method";
    return false;
  }

  return true;
}

void AndroidImageGenerator::NativeImageHeaderCallback(JNIEnv* env,
                                                      jclass jcaller,
                                                      jlong generator_address,
                                                      int width,
                                                      int height) {
  auto* generator = reinterpret_cast<AndroidImageGenerator*>(generator_address);
  generator->width_ = width;
  generator->height_ = height;
  generator->header_decoded_latch_.Signal();
}

}  // namespace flutter
