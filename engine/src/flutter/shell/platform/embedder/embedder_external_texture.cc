// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/embedder/embedder_external_texture.h"

#include "flutter/display_list/image/dl_image_skia.h"
#include "flutter/fml/closure.h"
#include "flutter/fml/logging.h"

#if defined(SHELL_ENABLE_GL)
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
#endif

#if defined(SHELL_ENABLE_GL) && defined(IMPELLER_SUPPORTS_RENDERING)
#include "impeller/core/texture_descriptor.h"
#include "impeller/display_list/aiks_context.h"
#include "impeller/display_list/dl_image_impeller.h"
#include "impeller/geometry/size.h"
#include "impeller/renderer/backend/gles/context_gles.h"
#include "impeller/renderer/backend/gles/handle_gles.h"
#include "impeller/renderer/backend/gles/texture_gles.h"
#endif

namespace flutter {

namespace {

void CleanupFrame(const FlutterExternalTextureFrame& frame) {
  if (frame.type == kFlutterExternalTextureFrameTypeOpenGL) {
    if (frame.open_gl.destruction_callback) {
      frame.open_gl.destruction_callback(frame.open_gl.user_data);
    }
  } else if (frame.type == kFlutterExternalTextureFrameTypeHardwareBuffer) {
    if (frame.hardware_buffer.destruction_callback) {
      frame.hardware_buffer.destruction_callback(
          frame.hardware_buffer.user_data);
    }
  }
}

}  // namespace

EmbedderExternalTexture::EmbedderExternalTexture(
    int64_t texture_identifier,
    ExternalTextureFrameCallback callback)
    : Texture(texture_identifier), callback_(std::move(callback)) {
  FML_DCHECK(callback_);
}

EmbedderExternalTexture::~EmbedderExternalTexture() = default;

void EmbedderExternalTexture::Paint(PaintContext& context,
                                    const DlRect& bounds,
                                    bool freeze,
                                    const DlImageSampling sampling) {
  if (last_image_ == nullptr) {
    last_image_ =
        ResolveTexture(Id(), context.gr_context, context.aiks_context,
                       SkISize::Make(bounds.GetWidth(), bounds.GetHeight()));
  }

  DlCanvas* canvas = context.canvas;
  const DlPaint* paint = context.paint;

  if (last_image_) {
    DlRect image_bounds = DlRect::Make(last_image_->GetBounds());
    if (bounds != image_bounds) {
      canvas->DrawImageRect(last_image_, image_bounds, bounds, sampling, paint);
    } else {
      canvas->DrawImage(last_image_, bounds.GetOrigin(), sampling, paint);
    }
  }
}

sk_sp<DlImage> EmbedderExternalTexture::ResolveTexture(
    int64_t texture_id,
    GrDirectContext* context,
    impeller::AiksContext* aiks_context,
    const SkISize& size) {
  if (!callback_) {
    return nullptr;
  }

  FlutterExternalTextureFrame frame = {};
  frame.struct_size = sizeof(FlutterExternalTextureFrame);
  if (!callback_(texture_id, size.width(), size.height(), &frame)) {
    return nullptr;
  }

  // Ensure destruction callback is invoked if frame resolution fails or is unsupported.
  fml::ScopedCleanupClosure frame_cleanup([&frame]() { CleanupFrame(frame); });

  sk_sp<DlImage> resolved_image;
  if (aiks_context) {
    resolved_image = ResolveTextureImpeller(frame, aiks_context, size);
  } else if (context) {
    resolved_image = ResolveTextureSkia(frame, context, size);
  }

  if (resolved_image != nullptr) {
    frame_cleanup.Release();
  }

  return resolved_image;
}

sk_sp<DlImage> EmbedderExternalTexture::ResolveTextureSkia(
    const FlutterExternalTextureFrame& frame,
    GrDirectContext* context,
    const SkISize& size) {
  if (!context) {
    return nullptr;
  }

  if (frame.type == kFlutterExternalTextureFrameTypeOpenGL) {
#if defined(SHELL_ENABLE_GL)
    const FlutterOpenGLTexture& texture = frame.open_gl;
    context->flushAndSubmit();
    context->resetContext(kAll_GrBackendState);

    GrGLTextureInfo gr_texture_info = {texture.target, texture.name,
                                       texture.format};
    size_t width = texture.width != 0 ? texture.width : size.width();
    size_t height = texture.height != 0 ? texture.height : size.height();

    auto gr_backend_texture = GrBackendTextures::MakeGL(
        width, height, skgpu::Mipmapped::kNo, gr_texture_info);
    SkImages::TextureReleaseProc release_proc = texture.destruction_callback;
    auto image =
        SkImages::BorrowTextureFrom(context, gr_backend_texture,
                                    kTopLeft_GrSurfaceOrigin,
                                    kRGBA_8888_SkColorType, kPremul_SkAlphaType,
                                    nullptr, release_proc, texture.user_data);
    if (!image) {
      FML_LOG(ERROR) << "Could not create external OpenGL texture with Skia.";
      return nullptr;
    }
    return DlImageSkia::Make(std::move(image));
#endif
  }

  return nullptr;
}

sk_sp<DlImage> EmbedderExternalTexture::ResolveTextureImpeller(
    const FlutterExternalTextureFrame& frame,
    impeller::AiksContext* aiks_context,
    const SkISize& size) {
  if (!aiks_context) {
    return nullptr;
  }

  if (frame.type == kFlutterExternalTextureFrameTypeOpenGL) {
#if defined(SHELL_ENABLE_GL) && defined(IMPELLER_SUPPORTS_RENDERING)
    const FlutterOpenGLTexture& texture = frame.open_gl;

    size_t width = texture.width != 0 ? texture.width : size.width();
    size_t height = texture.height != 0 ? texture.height : size.height();

    impeller::TextureDescriptor desc;
    desc.size = impeller::ISize(width, height);
    desc.format = impeller::PixelFormat::kR8G8B8A8UNormInt;
    desc.type = (texture.target == 0x8D65 /* GL_TEXTURE_EXTERNAL_OES */)
                    ? impeller::TextureType::kTextureExternalOES
                    : impeller::TextureType::kTexture2D;

    impeller::ContextGLES& context =
        impeller::ContextGLES::Cast(*aiks_context->GetContext());
    impeller::HandleGLES handle = context.GetReactor()->CreateHandle(
        impeller::HandleType::kTexture, texture.name);
    std::shared_ptr<impeller::TextureGLES> image =
        impeller::TextureGLES::WrapTexture(context.GetReactor(), desc, handle);

    if (!image) {
      FML_LOG(ERROR) << "Could not create external OpenGL texture with Impeller";
      return nullptr;
    }

    VoidCallback destruction_callback = texture.destruction_callback;
    if (!destruction_callback) {
      destruction_callback = [](void*) {};
    }
    auto cleanup_callback = [callback = destruction_callback,
                             user_data = texture.user_data]() {
      callback(user_data);
    };
    if (!context.GetReactor()->RegisterCleanupCallback(handle,
                                                       cleanup_callback)) {
      FML_LOG(ERROR) << "Could not register destruction callback";
      return nullptr;
    }

    return impeller::DlImageImpeller::Make(image);
#endif
  }

  return nullptr;
}

void EmbedderExternalTexture::OnGrContextCreated() {}

void EmbedderExternalTexture::OnGrContextDestroyed() {
  last_image_ = nullptr;
}

void EmbedderExternalTexture::MarkNewFrameAvailable() {
  last_image_ = nullptr;
}

void EmbedderExternalTexture::OnTextureUnregistered() {}

}  // namespace flutter
