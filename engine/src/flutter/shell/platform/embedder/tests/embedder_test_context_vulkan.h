// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_EMBEDDER_TESTS_EMBEDDER_TEST_CONTEXT_VULKAN_H_
#define FLUTTER_SHELL_PLATFORM_EMBEDDER_TESTS_EMBEDDER_TEST_CONTEXT_VULKAN_H_

#include <memory>
#include "flutter/shell/platform/embedder/tests/embedder_test_context.h"
#include "flutter/testing/test_vulkan_context.h"
#include "flutter/vulkan/vulkan_application.h"
#include "testing/test_vulkan_surface.h"

namespace flutter {
namespace testing {

class EmbedderTestContextVulkan : public EmbedderTestContext {
 public:
  explicit EmbedderTestContextVulkan(std::string assets_path = "");

  ~EmbedderTestContextVulkan() override;

  // |EmbedderTestContext|
  EmbedderTestContextType GetContextType() const override;

  // |EmbedderTestContext|
  size_t GetSurfacePresentCount() const override;

  VkImage GetNextImage(const DlISize& size);

  bool PresentImage(VkImage image);

  using VulkanExternalTextureCallback =
      std::function<bool(int64_t texture_identifier,
                         size_t width,
                         size_t height,
                         FlutterVulkanImage* image_out)>;

  void SetVulkanInstanceProcAddressCallback(
      FlutterVulkanInstanceProcAddressCallback callback);

  void SetVulkanExternalTextureCallback(VulkanExternalTextureCallback callback);

  const fml::RefPtr<TestVulkanContext>& GetVulkanContext() const {
    return vulkan_context_;
  }

  std::optional<TestVulkanImage> CreateImage(const DlISize& size) const {
    return vulkan_context_->CreateImage(size);
  }

  static void* InstanceProcAddr(void* user_data,
                                FlutterVulkanInstanceHandle instance,
                                const char* name);

 private:
  VulkanExternalTextureCallback external_texture_callback_;
  // |EmbedderTestContext|
  void SetSurface(DlISize surface_size) override;

  // |EmbedderTestContext|
  void SetupCompositor() override;

  // The TestVulkanContext destructor must be called _after_ the compositor is
  // freed.
  fml::RefPtr<TestVulkanContext> vulkan_context_ = nullptr;

  std::unique_ptr<TestVulkanSurface> surface_;

  DlISize surface_size_;
  size_t present_count_ = 0;

  FML_DISALLOW_COPY_AND_ASSIGN(EmbedderTestContextVulkan);
};

}  // namespace testing
}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_EMBEDDER_TESTS_EMBEDDER_TEST_CONTEXT_VULKAN_H_
