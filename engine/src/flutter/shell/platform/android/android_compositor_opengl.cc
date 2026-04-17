// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_compositor_opengl.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "flutter/fml/logging.h"
#include "flutter/impeller/toolkit/android/hardware_buffer.h"
#include "flutter/impeller/toolkit/android/proc_table.h"
#include "flutter/impeller/toolkit/android/surface_transaction.h"
#include "flutter/impeller/toolkit/egl/image.h"
#include "flutter/impeller/toolkit/gles/texture.h"
#include "flutter/shell/gpu/gpu_surface_gl_delegate.h"
#include "flutter/shell/platform/android/android_surface_gl_impeller.h"
#include "flutter/shell/platform/android/android_surface_gl_skia.h"
#include "flutter/shell/platform/android/surface/android_surface.h"

namespace flutter {

struct BackingStoreRecord {
  std::unique_ptr<impeller::android::HardwareBuffer> hardware_buffer;
  EGLImageKHR egl_image;
  GLuint texture;
  GLuint framebuffer;
};

AndroidCompositorOpenGL::AndroidCompositorOpenGL(
    AndroidSurface* android_surface,
    AndroidRenderingAPI rendering_api)
    : android_surface_(android_surface), rendering_api_(rendering_api) {}
AndroidCompositorOpenGL::~AndroidCompositorOpenGL() = default;

void AndroidCompositorOpenGL::SetNativeWindow(ANativeWindow* window) {
  window_ = window;
  if (window) {
    root_surface_control_ = std::make_unique<impeller::android::SurfaceControl>(
        window, "Flutter Root");
  } else {
    root_surface_control_.reset();
  }
}

bool AndroidCompositorOpenGL::CreateBackingStoreCallback(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out,
    void* user_data) {
  auto* self = static_cast<AndroidCompositorOpenGL*>(user_data);
  return self->CreateBackingStore(config, backing_store_out);
}

bool AndroidCompositorOpenGL::CollectBackingStoreCallback(
    const FlutterBackingStore* backing_store,
    void* user_data) {
  auto* self = static_cast<AndroidCompositorOpenGL*>(user_data);
  return self->CollectBackingStore(backing_store);
}

bool AndroidCompositorOpenGL::PresentViewCallback(
    const FlutterPresentViewInfo* info) {
  auto* self = static_cast<AndroidCompositorOpenGL*>(info->user_data);
  return self->PresentView(info);
}

bool AndroidCompositorOpenGL::CreateBackingStore(
    const FlutterBackingStoreConfig* config,
    FlutterBackingStore* backing_store_out) {
  FML_LOG(INFO) << "AndroidCompositorOpenGL::CreateBackingStore called.";
  impeller::ISize size(config->size.width, config->size.height);
  auto desc =
      impeller::android::HardwareBufferDescriptor::MakeForSwapchainImage(size);

  auto hardware_buffer =
      std::make_unique<impeller::android::HardwareBuffer>(desc);
  if (!hardware_buffer->IsValid()) {
    FML_LOG(ERROR) << "Failed to allocate AHardwareBuffer.";
    return false;
  }

  EGLDisplay display = eglGetCurrentDisplay();
  if (display == EGL_NO_DISPLAY) {
    FML_LOG(ERROR) << "No current EGL display.";
    return false;
  }

  EGLClientBuffer client_buffer =
      impeller::android::GetProcTable().eglGetNativeClientBufferANDROID(
          hardware_buffer->GetHandle());
  if (client_buffer == nullptr) {
    FML_LOG(ERROR) << "eglGetNativeClientBufferAndroid returned null.";
    return false;
  }

  EGLImageKHR egl_image = eglCreateImageKHR(
      display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, client_buffer, 0);
  if (egl_image == EGL_NO_IMAGE_KHR) {
    FML_LOG(ERROR) << "Failed to create EGLImage.";
    return false;
  }

  GLuint texture_name;
  glGenTextures(1, &texture_name);
  glBindTexture(GL_TEXTURE_2D, texture_name);
  typedef void (*PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)(GLenum target,
                                                      GLeglImageOES image);
  static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOESPtr =
      nullptr;
  if (!glEGLImageTargetTexture2DOESPtr) {
    glEGLImageTargetTexture2DOESPtr =
        reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(
            eglGetProcAddress("glEGLImageTargetTexture2DOES"));
  }

  if (!glEGLImageTargetTexture2DOESPtr) {
    FML_LOG(ERROR) << "glEGLImageTargetTexture2DOES not available.";
    return false;
  }

  glEGLImageTargetTexture2DOESPtr(GL_TEXTURE_2D, egl_image);

  GLuint framebuffer_name;
  glGenFramebuffers(1, &framebuffer_name);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_name);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         texture_name, 0);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    FML_LOG(ERROR) << "Framebuffer is not complete.";
    glDeleteFramebuffers(1, &framebuffer_name);
    glDeleteTextures(1, &texture_name);
    eglDestroyImageKHR(display, egl_image);
    return false;
  }

  auto record = std::make_unique<BackingStoreRecord>();
  record->hardware_buffer = std::move(hardware_buffer);
  record->egl_image = egl_image;
  record->texture = texture_name;
  record->framebuffer = framebuffer_name;

  backing_store_out->type = kFlutterBackingStoreTypeOpenGL;
  backing_store_out->open_gl.type = kFlutterOpenGLTargetTypeFramebuffer;
  backing_store_out->open_gl.framebuffer.name = framebuffer_name;
  backing_store_out->open_gl.framebuffer.target =
      GL_RGBA8;  // Assuming format is RGBA8
  backing_store_out->open_gl.framebuffer.user_data = record.release();
  backing_store_out->open_gl.framebuffer.destruction_callback = [](void* p) {
    auto* r = static_cast<BackingStoreRecord*>(p);
    EGLDisplay display = eglGetCurrentDisplay();
    glDeleteFramebuffers(1, &r->framebuffer);
    glDeleteTextures(1, &r->texture);
    if (display != EGL_NO_DISPLAY && r->egl_image != EGL_NO_IMAGE_KHR) {
      eglDestroyImageKHR(display, r->egl_image);
    }
    delete r;
  };

  return true;
}

bool AndroidCompositorOpenGL::CollectBackingStore(
    const FlutterBackingStore* backing_store) {
  if (backing_store->type == kFlutterBackingStoreTypeOpenGL) {
    if (backing_store->open_gl.framebuffer.destruction_callback) {
      backing_store->open_gl.framebuffer.destruction_callback(
          backing_store->open_gl.framebuffer.user_data);
    }
  }
  return true;
}

bool AndroidCompositorOpenGL::PresentView(const FlutterPresentViewInfo* info) {
  FML_LOG(INFO) << "AndroidCompositorOpenGL::PresentView called.";
  // Try SurfaceControl first if available.
  if (root_surface_control_ && root_surface_control_->IsValid()) {
    impeller::android::SurfaceTransaction transaction;
    if (transaction.IsValid()) {
      bool success = true;
      for (size_t i = 0; i < info->layers_count; ++i) {
        const FlutterLayer* layer = info->layers[i];
        if (layer->type == kFlutterLayerContentTypeBackingStore) {
          const FlutterBackingStore* backing_store = layer->backing_store;
          if (backing_store->type == kFlutterBackingStoreTypeOpenGL) {
            auto* record = static_cast<BackingStoreRecord*>(
                backing_store->open_gl.framebuffer.user_data);
            const impeller::android::HardwareBuffer* buffer =
                record->hardware_buffer.get();

            if (!transaction.SetContents(root_surface_control_.get(), buffer)) {
              FML_LOG(ERROR) << "Failed to set contents of surface control.";
              success = false;
              break;
            }
          }
        }
      }
      if (success && transaction.Apply()) {
        return true;
      }
    }
  }

  // Fallback: Blit to default framebuffer and swap buffers.
  FML_LOG(INFO) << "Falling back to EGL swap buffers.";

  if (!android_surface_) {
    FML_LOG(ERROR) << "No AndroidSurface available for fallback presentation.";
    return false;
  }

  GPUSurfaceGLDelegate* gl_delegate = nullptr;
  if (rendering_api_ == AndroidRenderingAPI::kSkiaOpenGLES) {
    auto* gl_skia = static_cast<AndroidSurfaceGLSkia*>(android_surface_);
    gl_delegate = static_cast<GPUSurfaceGLDelegate*>(gl_skia);
  } else if (rendering_api_ == AndroidRenderingAPI::kImpellerOpenGLES) {
    auto* gl_impeller =
        static_cast<AndroidSurfaceGLImpeller*>(android_surface_);
    gl_delegate = static_cast<GPUSurfaceGLDelegate*>(gl_impeller);
  }

  if (!gl_delegate) {
    FML_LOG(ERROR) << "AndroidSurface is not a GPUSurfaceGLDelegate.";
    return false;
  }

  auto context_result = gl_delegate->GLContextMakeCurrent();
  if (!context_result || !context_result->GetResult()) {
    FML_LOG(ERROR) << "Failed to make onscreen context current.";
    return false;
  }

  for (size_t i = 0; i < info->layers_count; ++i) {
    const FlutterLayer* layer = info->layers[i];
    if (layer->type == kFlutterLayerContentTypeBackingStore) {
      const FlutterBackingStore* backing_store = layer->backing_store;
      if (backing_store->type == kFlutterBackingStoreTypeOpenGL) {
        auto* record = static_cast<BackingStoreRecord*>(
            backing_store->open_gl.framebuffer.user_data);

        typedef void (*GLBlitFramebufferProc)(
            GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0,
            GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask,
            GLenum filter);
        static GLBlitFramebufferProc glBlitFramebufferPtr = nullptr;
        if (!glBlitFramebufferPtr) {
          glBlitFramebufferPtr = reinterpret_cast<GLBlitFramebufferProc>(
              eglGetProcAddress("glBlitFramebuffer"));
        }
        if (!glBlitFramebufferPtr) {
          glBlitFramebufferPtr = reinterpret_cast<GLBlitFramebufferProc>(
              eglGetProcAddress("glBlitFramebufferANGLE"));
        }

        if (!glBlitFramebufferPtr) {
          FML_LOG(ERROR) << "glBlitFramebuffer not available.";
          return false;
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, record->framebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        int width = record->hardware_buffer->GetDescriptor().size.width;
        int height = record->hardware_buffer->GetDescriptor().size.height;

        // Flip vertically but keep horizontal orientation normal.
        glBlitFramebufferPtr(0, 0, width, height, 0, height, width, 0,
                             GL_COLOR_BUFFER_BIT, GL_NEAREST);
      }
    }
  }

  std::optional<DlIRect> empty_damage = std::nullopt;
  GLPresentInfo present_info = {
      .fbo_id = 0,
      .frame_damage = empty_damage,
      .buffer_damage = empty_damage,
  };
  return gl_delegate->GLContextPresent(present_info);
}

}  // namespace flutter
