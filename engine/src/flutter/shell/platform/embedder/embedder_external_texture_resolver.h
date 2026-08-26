// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_EXTERNAL_TEXTURE_RESOLVER_H_
#define FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_EXTERNAL_TEXTURE_RESOLVER_H_

#include <functional>
#include <memory>

#include "flutter/common/graphics/texture.h"

#ifdef SHELL_ENABLE_GL
#include "flutter/shell/platform/embedder/embedder_external_texture_gl.h"
#endif

#ifdef SHELL_ENABLE_METAL
#include "flutter/shell/platform/embedder/embedder_external_texture_metal.h"
#endif

#include "flutter/shell/platform/embedder/embedder_external_texture.h"

namespace flutter {
class EmbedderExternalTextureResolver {
 public:
  using CustomExternalTextureCallback =
      std::function<std::shared_ptr<Texture>(int64_t)>;
  using ExternalTextureFrameCallback = std::function<
      bool(int64_t, size_t, size_t, FlutterExternalTextureFrame*)>;

  EmbedderExternalTextureResolver() = default;

  ~EmbedderExternalTextureResolver() = default;

  explicit EmbedderExternalTextureResolver(
      ExternalTextureFrameCallback frame_callback);

  explicit EmbedderExternalTextureResolver(
      CustomExternalTextureCallback custom_callback);

#ifdef SHELL_ENABLE_GL
  explicit EmbedderExternalTextureResolver(
      EmbedderExternalTextureGL::ExternalTextureCallback gl_callback);
#endif

#ifdef SHELL_ENABLE_METAL
  explicit EmbedderExternalTextureResolver(
      EmbedderExternalTextureMetal::ExternalTextureCallback metal_callback);
#endif

  EmbedderExternalTextureResolver(
      const EmbedderExternalTextureResolver& other) = default;
  EmbedderExternalTextureResolver& operator=(
      const EmbedderExternalTextureResolver& other) = default;
  EmbedderExternalTextureResolver(EmbedderExternalTextureResolver&& other) =
      default;
  EmbedderExternalTextureResolver& operator=(
      EmbedderExternalTextureResolver&& other) = default;

  std::shared_ptr<Texture> ResolveExternalTexture(int64_t texture_id);

  bool SupportsExternalTextures() const;

 private:
  ExternalTextureFrameCallback frame_callback_;
  CustomExternalTextureCallback custom_callback_;

#ifdef SHELL_ENABLE_GL
  EmbedderExternalTextureGL::ExternalTextureCallback gl_callback_;
#endif

#ifdef SHELL_ENABLE_METAL
  EmbedderExternalTextureMetal::ExternalTextureCallback metal_callback_;
#endif
};
}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_EXTERNAL_TEXTURE_RESOLVER_H_
