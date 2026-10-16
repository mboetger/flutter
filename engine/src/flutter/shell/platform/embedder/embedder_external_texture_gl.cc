// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/embedder/embedder_external_texture_gl.h"

#include "flutter/display_list/effects/dl_color_source.h"
#include "flutter/display_list/image/dl_image_skia.h"
#include "flutter/fml/logging.h"
#include "flutter/shell/platform/embedder/embedder_struct_macros.h"
#include "impeller/core/texture_descriptor.h"
#include "impeller/display_list/aiks_context.h"
#include "impeller/display_list/dl_image_impeller.h"
#include "impeller/geometry/size.h"
#include "impeller/renderer/backend/gles/context_gles.h"
#include "impeller/renderer/backend/gles/handle_gles.h"
#include "impeller/renderer/backend/gles/texture_gles.h"

#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"

namespace flutter {

namespace {

#ifndef GL_TEXTURE_EXTERNAL_OES
#define GL_TEXTURE_EXTERNAL_OES 0x8D65
#endif

#ifndef GL_RGBA8_OES
#define GL_RGBA8_OES 0x8058
#endif

DlMatrix ToDlMatrix(const FlutterTransformation& tr) {
  if (tr.scaleX == 0.0 && tr.skewX == 0.0 && tr.transX == 0.0 &&
      tr.skewY == 0.0 && tr.scaleY == 0.0 && tr.transY == 0.0 &&
      tr.pers0 == 0.0 && tr.pers1 == 0.0 && tr.pers2 == 0.0) {
    return DlMatrix();
  }
  return DlMatrix::MakeColumn(
      static_cast<DlScalar>(tr.scaleX), static_cast<DlScalar>(tr.skewY), 0.0f,
      static_cast<DlScalar>(tr.pers0), static_cast<DlScalar>(tr.skewX),
      static_cast<DlScalar>(tr.scaleY), 0.0f, static_cast<DlScalar>(tr.pers1),
      0.0f, 0.0f, 1.0f, 0.0f, static_cast<DlScalar>(tr.transX),
      static_cast<DlScalar>(tr.transY), 0.0f, static_cast<DlScalar>(tr.pers2));
}

}  // namespace

EmbedderExternalTextureGL::EmbedderExternalTextureGL(
    int64_t texture_identifier,
    const ExternalTextureCallback& callback)
    : Texture(texture_identifier), external_texture_callback_(callback) {
  FML_DCHECK(external_texture_callback_);
}

EmbedderExternalTextureGL::~EmbedderExternalTextureGL() = default;

// |flutter::Texture|
void EmbedderExternalTextureGL::Paint(PaintContext& context,
                                      const DlRect& bounds,
                                      bool freeze,
                                      const DlImageSampling sampling) {
  if (last_image_ == nullptr) {
    last_image_ =
        ResolveTexture(Id(),                                                 //
                       context.gr_context,                                   //
                       context.aiks_context,                                 //
                       SkISize::Make(bounds.GetWidth(), bounds.GetHeight())  //
        );
  }

  DlCanvas* canvas = context.canvas;
  const DlPaint* paint = context.paint;

  if (last_image_) {
    if (!last_uv_transform_.IsIdentity()) {
      DlAutoCanvasRestore auto_restore(canvas, true);
      canvas->Translate(bounds.GetX(), bounds.GetY());
      canvas->Scale(bounds.GetWidth(), bounds.GetHeight());

      DlMatrix transform =
          (DlMatrix::MakeScale({static_cast<DlScalar>(last_image_->width()),
                                static_cast<DlScalar>(last_image_->height()),
                                1.0f}) *
           last_uv_transform_)
              .Invert();

      DlPaint paint_with_shader = paint ? *paint : DlPaint();
      auto image_source =
          DlColorSource::MakeImage(last_image_, DlTileMode::kClamp,
                                   DlTileMode::kClamp, sampling, &transform);
      paint_with_shader.setColorSource(image_source);
      canvas->DrawRect(DlRect::MakeWH(1, 1), paint_with_shader);
      return;
    }

    DlRect image_bounds = DlRect::Make(last_image_->GetBounds());
    if (bounds != image_bounds) {
      canvas->DrawImageRect(last_image_, image_bounds, bounds, sampling, paint);
    } else {
      canvas->DrawImage(last_image_, bounds.GetOrigin(), sampling, paint);
    }
  }
}

sk_sp<DlImage> EmbedderExternalTextureGL::ResolveTexture(
    int64_t texture_id,
    GrDirectContext* context,
    impeller::AiksContext* aiks_context,
    const SkISize& size) {
  if (!!aiks_context) {
    return ResolveTextureImpeller(texture_id, aiks_context, size);
  } else if (context) {
    return ResolveTextureSkia(texture_id, context, size);
  }
  return nullptr;
}

sk_sp<DlImage> EmbedderExternalTextureGL::ResolveTextureSkia(
    int64_t texture_id,
    GrDirectContext* context,
    const SkISize& size) {
  if (!context) {
    return nullptr;
  }
  context->flushAndSubmit();
  context->resetContext(kAll_GrBackendState);
  std::unique_ptr<FlutterOpenGLExternalTexture> texture =
      external_texture_callback_(texture_id, size.width(), size.height());

  if (!texture) {
    return nullptr;
  }

  last_uv_transform_ = STRUCT_HAS_MEMBER(texture.get(), uv_transform)
                           ? ToDlMatrix(texture->uv_transform)
                           : DlMatrix();

  GrGLTextureInfo gr_texture_info = {texture->target, texture->name,
                                     texture->format};

  size_t width = size.width();
  size_t height = size.height();

  if (texture->width != 0 && texture->height != 0) {
    width = texture->width;
    height = texture->height;
  }

  auto gr_backend_texture = GrBackendTextures::MakeGL(
      width, height, skgpu::Mipmapped::kNo, gr_texture_info);
  SkImages::TextureReleaseProc release_proc = texture->destruction_callback;
  auto image =
      SkImages::BorrowTextureFrom(context,                   // context
                                  gr_backend_texture,        // texture handle
                                  kTopLeft_GrSurfaceOrigin,  // origin
                                  kRGBA_8888_SkColorType,    // color type
                                  kPremul_SkAlphaType,       // alpha type
                                  nullptr,                   // colorspace
                                  release_proc,       // texture release proc
                                  texture->user_data  // texture release context
      );

  if (!image) {
    // In case Skia rejects the image, call the release proc so that
    // embedders can perform collection of intermediates.
    if (release_proc) {
      release_proc(texture->user_data);
    }
    FML_LOG(ERROR) << "Could not create external texture->";
    return nullptr;
  }

  // This image should not escape local use by EmbedderExternalTextureGL
  return DlImageSkia::Make(std::move(image));
}

sk_sp<DlImage> EmbedderExternalTextureGL::ResolveTextureImpeller(
    int64_t texture_id,
    impeller::AiksContext* aiks_context,
    const SkISize& size) {
  std::unique_ptr<FlutterOpenGLExternalTexture> texture =
      external_texture_callback_(texture_id, size.width(), size.height());

  if (!texture) {
    return nullptr;
  }

  last_uv_transform_ = STRUCT_HAS_MEMBER(texture.get(), uv_transform)
                           ? ToDlMatrix(texture->uv_transform)
                           : DlMatrix();

  // Call the destruction callback if an error occurs.
  fml::ScopedCleanupClosure scoped_cleanup([&texture]() {
    if (texture->destruction_callback) {
      texture->destruction_callback(texture->user_data);
    }
  });

  if (texture->format != GL_RGBA8 && texture->format != GL_RGBA8_OES) {
    FML_LOG(ERROR) << "Only support GL_RGBA8 format now";
    return nullptr;
  }

  size_t width = size.width();
  size_t height = size.height();
  if (texture->width != 0 && texture->height != 0) {
    width = texture->width;
    height = texture->height;
  }

  impeller::TextureDescriptor desc;
  desc.size = impeller::ISize(width, height);
  desc.format = impeller::PixelFormat::kR8G8B8A8UNormInt;
  if (texture->target == GL_TEXTURE_EXTERNAL_OES) {
    desc.type = impeller::TextureType::kTextureExternalOES;
  }

  impeller::ContextGLES& context =
      impeller::ContextGLES::Cast(*aiks_context->GetContext());
  impeller::HandleGLES handle = context.GetReactor()->CreateHandle(
      impeller::HandleType::kTexture, texture->name);
  std::shared_ptr<impeller::TextureGLES> image =
      impeller::TextureGLES::WrapTexture(context.GetReactor(), desc, handle);

  if (!image) {
    FML_LOG(ERROR) << "Could not create external texture";
    return nullptr;
  }

  VoidCallback destruction_callback = texture->destruction_callback;
  if (!destruction_callback) {
    // Set a no-op cleanup callback if the texture does not provide a callback.
    // The presence of a cleanup callback indicates that the embedder controls
    // the GL texture's lifetime and Impeller should not delete it.
    destruction_callback = [](void*) {};
  }
  auto cleanup_callback = [callback = destruction_callback,
                           user_data = texture->user_data]() {
    callback(user_data);
  };
  if (!context.GetReactor()->RegisterCleanupCallback(handle,
                                                     cleanup_callback)) {
    FML_LOG(ERROR) << "Could not register destruction callback";
    return nullptr;
  }

  scoped_cleanup.Release();

  return impeller::DlImageImpeller::Make(image);
}

// |flutter::Texture|
void EmbedderExternalTextureGL::OnGrContextCreated() {}

// |flutter::Texture|
void EmbedderExternalTextureGL::OnGrContextDestroyed() {}

// |flutter::Texture|
void EmbedderExternalTextureGL::MarkNewFrameAvailable() {
  last_image_ = nullptr;
}

// |flutter::Texture|
void EmbedderExternalTextureGL::OnTextureUnregistered() {}

}  // namespace flutter
