// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_context_vk_impeller.h"

#include "flutter/fml/logging.h"
#include "flutter/fml/paths.h"
#include "flutter/impeller/entity/vk/entity_shaders_vk.h"
#include "flutter/impeller/entity/vk/framebuffer_blend_shaders_vk.h"
#include "flutter/impeller/entity/vk/modern_shaders_vk.h"
#include "flutter/impeller/renderer/backend/vulkan/context_vk.h"
#include "flutter/shell/platform/embedder/embedder.h"
#include "shell/platform/android/android_rendering_selector.h"
#include "shell/platform/android/context/android_context.h"

namespace flutter {

static std::shared_ptr<impeller::Context> CreateImpellerContext(
    const fml::RefPtr<fml::NativeLibrary>& vulkan_dylib,
    const AndroidContext::ContextSettings& p_settings) {
  if (!vulkan_dylib) {
    VALIDATION_LOG << "Could not open the Vulkan dylib.";
    return nullptr;
  }

  std::vector<std::shared_ptr<fml::Mapping>> shader_mappings = {
      std::make_shared<fml::NonOwnedMapping>(impeller_entity_shaders_vk_data,
                                             impeller_entity_shaders_vk_length),
      std::make_shared<fml::NonOwnedMapping>(
          impeller_framebuffer_blend_shaders_vk_data,
          impeller_framebuffer_blend_shaders_vk_length),
      std::make_shared<fml::NonOwnedMapping>(impeller_modern_shaders_vk_data,
                                             impeller_modern_shaders_vk_length),
  };

  auto instance_proc_addr =
      vulkan_dylib->ResolveFunction<PFN_vkGetInstanceProcAddr>(
          "vkGetInstanceProcAddr");

  if (!instance_proc_addr.has_value()) {
    VALIDATION_LOG << "Could not setup Vulkan proc table.";
    return nullptr;
  }

  impeller::ContextVK::Settings settings;
  settings.proc_address_callback = instance_proc_addr.value();
  settings.shader_libraries_data = std::move(shader_mappings);
  settings.cache_directory = fml::paths::GetCachesDirectory();
  settings.enable_validation = p_settings.enable_validation;
  settings.enable_gpu_tracing = p_settings.enable_gpu_tracing;
  settings.enable_surface_control = p_settings.enable_surface_control;
  settings.flags = p_settings.impeller_flags;
  // Android can fall back to OpenGLES, so drivers that are known to have
  // issues with Vulkan are rejected as soon as the physical device is
  // identified, before the (expensive) remainder of Vulkan setup runs.
  auto selection = impeller::ContextVK::SelectDevice(std::move(settings));
  if (!selection.ok()) {
    return nullptr;
  }
  const auto& props = selection->physical_device_properties;
  FlutterVulkanDriverProperties driver_props = {};
  driver_props.struct_size = sizeof(FlutterVulkanDriverProperties);
  driver_props.api_version = props.apiVersion;
  driver_props.driver_version = props.driverVersion;
  driver_props.vendor_id = props.vendorID;
  driver_props.device_id = props.deviceID;
  driver_props.device_type = static_cast<uint32_t>(props.deviceType);
  driver_props.device_name = props.deviceName.data();
  bool is_known_bad = false;
  if (FlutterEngineQueryVulkanDriverSupport(&driver_props, &is_known_bad) !=
          kSuccess ||
      is_known_bad) {
    FML_LOG(INFO)
        << "Known bad Vulkan driver encountered, falling back to OpenGLES.";
    return nullptr;
  }

  auto context = selection->CreateContext();

  if (!p_settings.quiet) {
    if (context && impeller::CapabilitiesVK::Cast(*context->GetCapabilities())
                       .AreValidationsEnabled()) {
      FML_LOG(IMPORTANT) << "Using the Impeller rendering backend (Vulkan with "
                            "Validation Layers).";
    } else if (context) {
      FML_LOG(IMPORTANT) << "Using the Impeller rendering backend (Vulkan).";
    }
  }

  return context;
}

AndroidContextVKImpeller::AndroidContextVKImpeller(
    const AndroidContext::ContextSettings& settings)
    : AndroidContext(AndroidRenderingAPI::kImpellerVulkan),
      vulkan_dylib_(fml::NativeLibrary::Create("libvulkan.so")) {
  auto impeller_context = CreateImpellerContext(vulkan_dylib_, settings);
  is_valid_ = !!impeller_context;
  SetImpellerContext(impeller_context);
}

AndroidContextVKImpeller::~AndroidContextVKImpeller() = default;

bool AndroidContextVKImpeller::IsValid() const {
  return is_valid_;
}

AndroidRenderingAPI AndroidContextVKImpeller::RenderingApi() const {
  return AndroidRenderingAPI::kImpellerVulkan;
}

}  // namespace flutter
