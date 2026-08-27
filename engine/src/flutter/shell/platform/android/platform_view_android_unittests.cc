// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "shell/platform/android/flutter_main.h"

namespace flutter {
namespace testing {

struct RenderingAPITestCase {
  int api_level;
  bool enable_impeller;
  bool enable_software_rendering;
  std::string requested_backend;
  AndroidRenderingAPI expected_api;
};

class AndroidRenderingAPISelectionTest
    : public ::testing::TestWithParam<RenderingAPITestCase> {};

TEST_P(AndroidRenderingAPISelectionTest, SelectsExpectedBackend) {
  const auto& param = GetParam();
  Settings settings;
  settings.enable_impeller = param.enable_impeller;
  settings.enable_software_rendering = param.enable_software_rendering;
  settings.requested_rendering_backend = param.requested_backend;

  EXPECT_EQ(FlutterMain::SelectedRenderingAPI(settings, param.api_level),
            param.expected_api);
}

#if !SLIMPELLER
INSTANTIATE_TEST_SUITE_P(
    BackendSelectionAcrossAPILevels,
    AndroidRenderingAPISelectionTest,
    ::testing::Values(
        // Software rendering overrides everything when impeller is false.
        RenderingAPITestCase{21, false, true, "",
                             AndroidRenderingAPI::kSoftware},
        RenderingAPITestCase{28, false, true, "",
                             AndroidRenderingAPI::kSoftware},
        RenderingAPITestCase{29, false, true, "",
                             AndroidRenderingAPI::kSoftware},
        RenderingAPITestCase{35, false, true, "",
                             AndroidRenderingAPI::kSoftware},

        // Explicit requested backend "opengles" with Impeller.
        RenderingAPITestCase{21, true, false, "opengles",
                             AndroidRenderingAPI::kImpellerOpenGLES},
        RenderingAPITestCase{24, true, false, "opengles",
                             AndroidRenderingAPI::kImpellerOpenGLES},
        RenderingAPITestCase{28, true, false, "opengles",
                             AndroidRenderingAPI::kImpellerOpenGLES},
        RenderingAPITestCase{29, true, false, "opengles",
                             AndroidRenderingAPI::kImpellerOpenGLES},
        RenderingAPITestCase{35, true, false, "opengles",
                             AndroidRenderingAPI::kImpellerOpenGLES},

        // Explicit requested backend "vulkan" with Impeller.
        RenderingAPITestCase{21, true, false, "vulkan",
                             AndroidRenderingAPI::kImpellerVulkan},
        RenderingAPITestCase{24, true, false, "vulkan",
                             AndroidRenderingAPI::kImpellerVulkan},
        RenderingAPITestCase{28, true, false, "vulkan",
                             AndroidRenderingAPI::kImpellerVulkan},
        RenderingAPITestCase{29, true, false, "vulkan",
                             AndroidRenderingAPI::kImpellerVulkan},
        RenderingAPITestCase{35, true, false, "vulkan",
                             AndroidRenderingAPI::kImpellerVulkan},

        // Impeller enabled, no requested backend:
        // Below API 29 falls back to Skia OpenGLES.
        RenderingAPITestCase{21, true, false, "",
                             AndroidRenderingAPI::kSkiaOpenGLES},
        RenderingAPITestCase{24, true, false, "",
                             AndroidRenderingAPI::kSkiaOpenGLES},
        RenderingAPITestCase{28, true, false, "",
                             AndroidRenderingAPI::kSkiaOpenGLES},
        // API 29+ selects Impeller Autoselect.
        RenderingAPITestCase{29, true, false, "",
                             AndroidRenderingAPI::kImpellerAutoselect},
        RenderingAPITestCase{30, true, false, "",
                             AndroidRenderingAPI::kImpellerAutoselect},
        RenderingAPITestCase{31, true, false, "",
                             AndroidRenderingAPI::kImpellerAutoselect},
        RenderingAPITestCase{33, true, false, "",
                             AndroidRenderingAPI::kImpellerAutoselect},
        RenderingAPITestCase{34, true, false, "",
                             AndroidRenderingAPI::kImpellerAutoselect},
        RenderingAPITestCase{35, true, false, "",
                             AndroidRenderingAPI::kImpellerAutoselect},

        // Impeller disabled falls back to Skia OpenGLES regardless of requested
        // backend.
        RenderingAPITestCase{21, false, false, "",
                             AndroidRenderingAPI::kSkiaOpenGLES},
        RenderingAPITestCase{28, false, false, "",
                             AndroidRenderingAPI::kSkiaOpenGLES},
        RenderingAPITestCase{29, false, false, "",
                             AndroidRenderingAPI::kSkiaOpenGLES},
        RenderingAPITestCase{35, false, false, "",
                             AndroidRenderingAPI::kSkiaOpenGLES},
        RenderingAPITestCase{29, false, false, "vulkan",
                             AndroidRenderingAPI::kSkiaOpenGLES},
        RenderingAPITestCase{29, false, false, "opengles",
                             AndroidRenderingAPI::kSkiaOpenGLES}));
#else
INSTANTIATE_TEST_SUITE_P(
    BackendSelectionSlimpeller,
    AndroidRenderingAPISelectionTest,
    ::testing::Values(
        RenderingAPITestCase{21, true, false, "",
                             AndroidRenderingAPI::kImpellerAutoselect},
        RenderingAPITestCase{29, true, false, "",
                             AndroidRenderingAPI::kImpellerAutoselect},
        RenderingAPITestCase{35, true, false, "",
                             AndroidRenderingAPI::kImpellerAutoselect}));
#endif  // !SLIMPELLER

}  // namespace testing
}  // namespace flutter
