// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_image_generator.h"

#include <cstring>
#include <memory>
#include <utility>

#include <android/bitmap.h>
#include <android/hardware_buffer.h>

#include "flutter/fml/logging.h"
#include "flutter/fml/platform/android/jni_util.h"
#include "flutter/fml/platform/android/scoped_java_ref.h"

namespace flutter {

static fml::jni::ScopedJavaGlobalRef<jclass>* g_flutter_jni_class = nullptr;
static jmethodID g_decode_image_method = nullptr;

AndroidImageGenerator::~AndroidImageGenerator() = default;

AndroidImageGenerator::AndroidImageGenerator(std::vector<uint8_t> data)
    : data_(std::move(data)) {}

bool AndroidImageGenerator::GetInfo(FlutterImageGeneratorInfo* info_out) {
  if (info_out == nullptr) {
    return false;
  }
  header_decoded_latch_.Wait();
  if (width_ <= 0 || height_ <= 0) {
    return false;
  }
  info_out->struct_size = sizeof(FlutterImageGeneratorInfo);
  info_out->width = static_cast<uint32_t>(width_);
  info_out->height = static_cast<uint32_t>(height_);
  info_out->color_type = kFlutterImageGeneratorColorTypeRGBA8888;
  info_out->alpha_type = kFlutterImageGeneratorAlphaTypePremul;
  return true;
}

bool AndroidImageGenerator::GetPixels(const FlutterImageGeneratorInfo* info,
                                      void* pixels,
                                      size_t row_bytes) {
  fully_decoded_latch_.Wait();

  if (!software_decoded_data_ || info == nullptr || pixels == nullptr) {
    return false;
  }

  if (info->color_type != kFlutterImageGeneratorColorTypeRGBA8888) {
    return false;
  }

  if (info->alpha_type != kFlutterImageGeneratorAlphaTypePremul) {
    return false;
  }

  if (info->width == 0 || info->height == 0) {
    return false;
  }
  const size_t min_row_bytes =
      static_cast<size_t>(info->width) * sizeof(uint32_t);
  if (row_bytes < min_row_bytes) {
    return false;
  }

  // Compute byte size matching SkImageInfo::computeByteSize(row_bytes):
  // (height - 1) * row_bytes + width * sizeof(uint32_t), guarding against
  // size_t overflow.
  const size_t last_row_offset =
      static_cast<size_t>(info->height - 1) * row_bytes;
  if (info->height > 1 &&
      last_row_offset / static_cast<size_t>(info->height - 1) != row_bytes) {
    return false;
  }
  const size_t pixels_size = last_row_offset + min_row_bytes;
  if (pixels_size < last_row_offset ||
      pixels_size < software_decoded_data_->GetSize()) {
    return false;
  }
  memcpy(pixels, software_decoded_data_->GetMapping(),
         software_decoded_data_->GetSize());
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

  // Call FlutterJNI.decodeImage

  JNIEnv* env = fml::jni::AttachCurrentThread();

  // This task is run on the IO thread.  Create a frame to ensure that all
  // local JNI references used here are freed.
  fml::jni::ScopedJavaLocalFrame scoped_local_reference_frame(env);

  jobject direct_buffer =
      env->NewDirectByteBuffer(data_.data(), static_cast<jlong>(data_.size()));

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

  // Lock the android buffer in a shared pointer

  void* pixel_lock;
  if ((status = AndroidBitmap_lockPixels(env, bitmap->obj(), &pixel_lock)) <
      0) {
    FML_DLOG(ERROR) << "Failed to lock pixels, error=" << status;
    return;
  }

  auto* raw_bitmap = bitmap.release();
  software_decoded_data_ = std::make_unique<fml::NonOwnedMapping>(
      static_cast<const uint8_t*>(pixel_lock),
      static_cast<size_t>(info.width) * static_cast<size_t>(info.height) *
          sizeof(uint32_t),
      [raw_bitmap](const uint8_t* /*data*/, size_t /*size*/) {
        auto env = fml::jni::AttachCurrentThread();
        AndroidBitmap_unlockPixels(env, raw_bitmap->obj());
        delete raw_bitmap;
      });
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

bool AndroidImageGenerator::CreateGenerator(
    const uint8_t* data,
    size_t data_size,
    const fml::RefPtr<fml::TaskRunner>& task_runner,
    FlutterImageGenerator* generator_out) {
  if (data == nullptr || data_size == 0 || generator_out == nullptr) {
    return false;
  }
  std::vector<uint8_t> buffer(data, data + data_size);
  std::shared_ptr<AndroidImageGenerator> generator(
      new AndroidImageGenerator(std::move(buffer)));

  fml::TaskRunner::RunNowOrPostTask(
      task_runner, [generator]() { generator->DecodeImage(); });

  if (!generator->IsValidImageData()) {
    return false;
  }

  auto* handle =
      new std::shared_ptr<AndroidImageGenerator>(std::move(generator));
  generator_out->struct_size = sizeof(FlutterImageGenerator);
  generator_out->user_data = handle;
  generator_out->get_info = [](void* user_data,
                               FlutterImageGeneratorInfo* info_out) -> bool {
    return (*static_cast<std::shared_ptr<AndroidImageGenerator>*>(user_data))
        ->GetInfo(info_out);
  };
  generator_out->get_pixels = [](void* user_data,
                                 const FlutterImageGeneratorInfo* info,
                                 void* pixels, size_t row_bytes) -> bool {
    return (*static_cast<std::shared_ptr<AndroidImageGenerator>*>(user_data))
        ->GetPixels(info, pixels, row_bytes);
  };
  generator_out->destruction_callback = [](void* user_data) {
    delete static_cast<std::shared_ptr<AndroidImageGenerator>*>(user_data);
  };
  return true;
}

std::shared_ptr<AndroidImageGenerator> AndroidImageGenerator::MakeForTesting(
    int width,
    int height,
    std::unique_ptr<fml::Mapping> decoded_data) {
  std::shared_ptr<AndroidImageGenerator> generator(
      new AndroidImageGenerator({}));
  generator->width_ = width;
  generator->height_ = height;
  generator->software_decoded_data_ = std::move(decoded_data);
  generator->header_decoded_latch_.Signal();
  generator->fully_decoded_latch_.Signal();
  return generator;
}

void AndroidImageGenerator::NativeImageHeaderCallback(JNIEnv* env,
                                                      jclass jcaller,
                                                      jlong generator_address,
                                                      int width,
                                                      int height) {
  AndroidImageGenerator* generator =
      reinterpret_cast<AndroidImageGenerator*>(generator_address);

  generator->width_ = width;
  generator->height_ = height;
  generator->header_decoded_latch_.Signal();
}

bool AndroidImageGenerator::IsValidImageData() {
  // The generator kicks off an IO task to decode everything, and waiting on
  // header_decoded_latch_ blocks until either the header has been decoded or
  // decoding has failed, whichever is sooner. The decoder is initialized with
  // a width and height of -1 and will update the dimensions if the image is
  // able to be decoded.
  header_decoded_latch_.Wait();
  return width_ > 0 && height_ > 0;
}

}  // namespace flutter
