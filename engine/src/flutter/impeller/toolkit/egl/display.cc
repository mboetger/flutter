// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/toolkit/egl/display.h"

#include <vector>

#include "flutter/fml/logging.h"
#include "impeller/toolkit/egl/context.h"
#include "impeller/toolkit/egl/surface.h"

#ifndef EGL_PROTECTED_CONTENT_EXT
#define EGL_PROTECTED_CONTENT_EXT 0x32C0
#endif

namespace impeller {
namespace egl {

Display::Display() {
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

  if (::eglInitialize(display, nullptr, nullptr) != EGL_TRUE) {
    IMPELLER_LOG_EGL_ERROR;
    return;
  }
  display_ = display;
}

Display::~Display() {
  if (display_ != EGL_NO_DISPLAY) {
    if (::eglTerminate(display_) != EGL_TRUE) {
      IMPELLER_LOG_EGL_ERROR;
    }
  }
}

bool Display::IsValid() const {
  return display_ != EGL_NO_DISPLAY;
}

std::unique_ptr<Context> Display::CreateContext(const Config& config,
                                                const Context* share_context,
                                                bool use_protected_context) {
  const auto& desc = config.GetDescriptor();

  std::vector<EGLint> attributes;
  switch (desc.api) {
    case API::kOpenGL:
      break;
    case API::kOpenGLES2:
      attributes.push_back(EGL_CONTEXT_CLIENT_VERSION);
      attributes.push_back(2);
      break;
    case API::kOpenGLES3:
      attributes.push_back(EGL_CONTEXT_CLIENT_VERSION);
      attributes.push_back(3);
      break;
  }
  if (use_protected_context) {
    const char* extensions = ::eglQueryString(display_, EGL_EXTENSIONS);
    if (extensions && strstr(extensions, "EGL_EXT_protected_content")) {
      attributes.push_back(EGL_PROTECTED_CONTENT_EXT);
      attributes.push_back(EGL_TRUE);
    } else {
      FML_LOG(WARNING) << "Protected context requested but "
                          "EGL_EXT_protected_content not supported.";
    }
  }
  // Termination sentinel must be present.
  attributes.push_back(EGL_NONE);

  auto context = ::eglCreateContext(
      display_,            // display
      config.GetHandle(),  // config
      share_context != nullptr ? share_context->GetHandle() : nullptr,  // share
      attributes.data()  // attributes
  );

  if (context == EGL_NO_CONTEXT) {
    IMPELLER_LOG_EGL_ERROR;
    return nullptr;
  }

  return std::unique_ptr<Context>(new Context(display_, context));
}

std::unique_ptr<Config> Display::ChooseConfig(ConfigDescriptor config) const {
  if (!display_) {
    return nullptr;
  }

  std::vector<EGLint> attributes;

  {
    attributes.push_back(EGL_RENDERABLE_TYPE);
    switch (config.api) {
      case API::kOpenGL:
        attributes.push_back(EGL_OPENGL_BIT);
        break;
      case API::kOpenGLES2:
        attributes.push_back(EGL_OPENGL_ES2_BIT);
        break;
      case API::kOpenGLES3:
        attributes.push_back(EGL_OPENGL_ES3_BIT);
        break;
    }
  }

  {
    attributes.push_back(EGL_SURFACE_TYPE);
    switch (config.surface_type) {
      case SurfaceType::kWindow:
        attributes.push_back(EGL_WINDOW_BIT);
        break;
      case SurfaceType::kPBuffer:
        attributes.push_back(EGL_PBUFFER_BIT);
        break;
    }
  }

  {
    switch (config.color_format) {
      case ColorFormat::kRGBA8888:
        attributes.push_back(EGL_RED_SIZE);
        attributes.push_back(8);
        attributes.push_back(EGL_GREEN_SIZE);
        attributes.push_back(8);
        attributes.push_back(EGL_BLUE_SIZE);
        attributes.push_back(8);
        attributes.push_back(EGL_ALPHA_SIZE);
        attributes.push_back(8);
        break;
      case ColorFormat::kRGB565:
        attributes.push_back(EGL_RED_SIZE);
        attributes.push_back(5);
        attributes.push_back(EGL_GREEN_SIZE);
        attributes.push_back(6);
        attributes.push_back(EGL_BLUE_SIZE);
        attributes.push_back(5);
        break;
    }
  }

  {
    attributes.push_back(EGL_DEPTH_SIZE);
    attributes.push_back(static_cast<EGLint>(config.depth_bits));
  }

  {
    attributes.push_back(EGL_STENCIL_SIZE);
    attributes.push_back(static_cast<EGLint>(config.stencil_bits));
  }

  {
    const auto sample_count = static_cast<EGLint>(config.samples);
    if (sample_count > 1) {
      attributes.push_back(EGL_SAMPLE_BUFFERS);
      attributes.push_back(1);
      attributes.push_back(EGL_SAMPLES);
      attributes.push_back(sample_count);
    }
  }

  // termination sentinel must be present.
  attributes.push_back(EGL_NONE);

  EGLConfig config_out = nullptr;
  EGLint config_count_out = 0;
  if (::eglChooseConfig(display_,           // display
                        attributes.data(),  // attributes (null terminated)
                        &config_out,        // matched configs
                        1,                  // configs array size
                        &config_count_out   // match configs count
                        ) != EGL_TRUE) {
    IMPELLER_LOG_EGL_ERROR;
    return nullptr;
  }

  if (config_count_out < 1) {
    return nullptr;
  }

  return std::make_unique<Config>(config, config_out);
}

std::unique_ptr<Surface> Display::CreateWindowSurface(
    const Config& config,
    EGLNativeWindowType window,
    bool use_protected_context) {
  std::vector<EGLint> attribs;
  if (use_protected_context) {
    const char* extensions = ::eglQueryString(display_, EGL_EXTENSIONS);
    if (extensions && strstr(extensions, "EGL_EXT_protected_content")) {
      attribs.push_back(EGL_PROTECTED_CONTENT_EXT);
      attribs.push_back(EGL_TRUE);
    }
  }
  attribs.push_back(EGL_NONE);
  auto surface = ::eglCreateWindowSurface(display_,            // display
                                          config.GetHandle(),  // config
                                          window,              // window
                                          attribs.data()       // attrib_list
  );
  if (surface == EGL_NO_SURFACE) {
    IMPELLER_LOG_EGL_ERROR;
    return nullptr;
  }
  return std::unique_ptr<Surface>(new Surface(display_, surface));
}

std::unique_ptr<Surface> Display::CreatePixelBufferSurface(
    const Config& config,
    size_t width,
    size_t height,
    bool use_protected_context) {
  std::vector<EGLint> attribs;
  attribs.push_back(EGL_WIDTH);
  attribs.push_back(static_cast<EGLint>(width));
  attribs.push_back(EGL_HEIGHT);
  attribs.push_back(static_cast<EGLint>(height));
  if (use_protected_context) {
    const char* extensions = ::eglQueryString(display_, EGL_EXTENSIONS);
    if (extensions && strstr(extensions, "EGL_EXT_protected_content")) {
      attribs.push_back(EGL_PROTECTED_CONTENT_EXT);
      attribs.push_back(EGL_TRUE);
    }
  }
  attribs.push_back(EGL_NONE);
  auto surface = ::eglCreatePbufferSurface(display_,            // display
                                           config.GetHandle(),  // config
                                           attribs.data()       // attrib_list
  );
  if (surface == EGL_NO_SURFACE) {
    IMPELLER_LOG_EGL_ERROR;
    return nullptr;
  }
  return std::unique_ptr<Surface>(new Surface(display_, surface));
}

const EGLDisplay& Display::GetHandle() const {
  return display_;
}

}  // namespace egl
}  // namespace impeller
