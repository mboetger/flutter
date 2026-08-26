// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_EXTERNAL_TEXTURE_H_
#define FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_EXTERNAL_TEXTURE_H_

#include <functional>
#include <memory>

#include "flutter/common/graphics/texture.h"
#include "flutter/display_list/image/dl_image.h"
#include "flutter/fml/macros.h"
#include "flutter/shell/platform/embedder/embedder.h"
#include "third_party/skia/include/core/SkSize.h"

namespace flutter {

class EmbedderExternalTexture : public flutter::Texture {
 public:
  using ExternalTextureFrameCallback = std::function<bool(
      int64_t /* texture_id */,
      size_t /* width */,
      size_t /* height */,
      FlutterExternalTextureFrame* /* frame_out */)>;

  EmbedderExternalTexture(int64_t texture_identifier,
                          ExternalTextureFrameCallback callback);

  ~EmbedderExternalTexture() override;

  // |flutter::Texture|
  void Paint(PaintContext& context,
             const DlRect& bounds,
             bool freeze,
             const DlImageSampling sampling) override;

  // |flutter::Texture|
  void OnGrContextCreated() override;

  // |flutter::Texture|
  void OnGrContextDestroyed() override;

  // |flutter::Texture|
  void MarkNewFrameAvailable() override;

  // |flutter::Texture|
  void OnTextureUnregistered() override;

 private:
  ExternalTextureFrameCallback callback_;
  sk_sp<DlImage> last_image_;

  sk_sp<DlImage> ResolveTexture(int64_t texture_id,
                                GrDirectContext* context,
                                impeller::AiksContext* aiks_context,
                                const SkISize& size);

  sk_sp<DlImage> ResolveTextureSkia(const FlutterExternalTextureFrame& frame,
                                    GrDirectContext* context,
                                    const SkISize& size);

  sk_sp<DlImage> ResolveTextureImpeller(
      const FlutterExternalTextureFrame& frame,
      impeller::AiksContext* aiks_context,
      const SkISize& size);

  FML_DISALLOW_COPY_AND_ASSIGN(EmbedderExternalTexture);
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_EXTERNAL_TEXTURE_H_
