// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/embedder/platform_view_embedder.h"

#include <mutex>
#include <utility>

#include "flutter/fml/make_copyable.h"

namespace flutter {

class PlatformViewEmbedder::EmbedderPlatformMessageHandler
    : public PlatformMessageHandler,
      public std::enable_shared_from_this<EmbedderPlatformMessageHandler> {
 public:
  EmbedderPlatformMessageHandler(
      PlatformViewEmbedder* parent,
      fml::RefPtr<fml::TaskRunner> platform_task_runner,
      bool enable_direct_dispatch)
      : parent_(parent),
        platform_task_runner_(std::move(platform_task_runner)),
        enable_direct_dispatch_(enable_direct_dispatch) {}

  void Shutdown() {
    std::scoped_lock lock(mutex_);
    parent_ = nullptr;
    platform_task_runner_ = nullptr;
  }

  virtual void HandlePlatformMessage(std::unique_ptr<PlatformMessage> message) {
    auto self = shared_from_this();
    fml::RefPtr<fml::TaskRunner> task_runner;
    {
      std::scoped_lock lock(mutex_);
      if (enable_direct_dispatch_ || !platform_task_runner_) {
        if (parent_) {
          parent_->HandlePlatformMessage(std::move(message));
        } else if (message) {
          FML_DLOG(WARNING) << "Deleted engine dropping message on channel "
                            << message->channel();
          if (message->response()) {
            message->response()->CompleteEmpty();
          }
        }
        return;
      }
      task_runner = platform_task_runner_;
    }
    task_runner->PostTask(fml::MakeCopyable(
        [self = std::move(self), message = std::move(message)]() mutable {
          auto* handler =
              static_cast<EmbedderPlatformMessageHandler*>(self.get());
          std::scoped_lock lock(handler->mutex_);
          if (handler->parent_) {
            handler->parent_->HandlePlatformMessage(std::move(message));
          } else if (message) {
            FML_DLOG(WARNING) << "Deleted engine dropping message on channel "
                              << message->channel();
            if (message->response()) {
              message->response()->CompleteEmpty();
            }
          }
        }));
  }

  virtual bool DoesHandlePlatformMessageOnPlatformThread() const {
    return !enable_direct_dispatch_;
  }

  virtual void InvokePlatformMessageResponseCallback(
      int response_id,
      std::unique_ptr<fml::Mapping> mapping) {
    auto self = shared_from_this();
    fml::RefPtr<fml::TaskRunner> task_runner;
    {
      std::scoped_lock lock(mutex_);
      if (enable_direct_dispatch_ || !platform_task_runner_) {
        if (parent_) {
          parent_->InvokePlatformMessageResponseCallback(response_id,
                                                         std::move(mapping));
        }
        return;
      }
      task_runner = platform_task_runner_;
    }
    task_runner->PostTask(
        fml::MakeCopyable([self = std::move(self), response_id,
                           mapping = std::move(mapping)]() mutable {
          auto* handler =
              static_cast<EmbedderPlatformMessageHandler*>(self.get());
          std::scoped_lock lock(handler->mutex_);
          if (handler->parent_) {
            handler->parent_->InvokePlatformMessageResponseCallback(
                response_id, std::move(mapping));
          }
        }));
  }

  virtual void InvokePlatformMessageEmptyResponseCallback(int response_id) {
    auto self = shared_from_this();
    fml::RefPtr<fml::TaskRunner> task_runner;
    {
      std::scoped_lock lock(mutex_);
      if (enable_direct_dispatch_ || !platform_task_runner_) {
        if (parent_) {
          parent_->InvokePlatformMessageEmptyResponseCallback(response_id);
        }
        return;
      }
      task_runner = platform_task_runner_;
    }
    task_runner->PostTask([self = std::move(self), response_id]() {
      auto* handler = static_cast<EmbedderPlatformMessageHandler*>(self.get());
      std::scoped_lock lock(handler->mutex_);
      if (handler->parent_) {
        handler->parent_->InvokePlatformMessageEmptyResponseCallback(
            response_id);
      }
    });
  }

 private:
  std::recursive_mutex mutex_;
  PlatformViewEmbedder* parent_ = nullptr;
  fml::RefPtr<fml::TaskRunner> platform_task_runner_;
  const bool enable_direct_dispatch_ = false;
};

PlatformViewEmbedder::PlatformViewEmbedder(
    PlatformView::Delegate& delegate,
    const flutter::TaskRunners& task_runners,
    const EmbedderSurfaceSoftware::SoftwareDispatchTable&
        software_dispatch_table,
    PlatformDispatchTable platform_dispatch_table,
    std::shared_ptr<EmbedderExternalViewEmbedder> external_view_embedder)
    : PlatformView(delegate, task_runners),
      external_view_embedder_(std::move(external_view_embedder)),
      embedder_surface_(
          std::make_unique<EmbedderSurfaceSoftware>(software_dispatch_table,
                                                    external_view_embedder_)),
      embedder_platform_message_handler_(
          platform_dispatch_table.custom_platform_message_handler
              ? nullptr
              : std::make_shared<EmbedderPlatformMessageHandler>(
                    this,
                    task_runners.GetPlatformTaskRunner(),
                    platform_dispatch_table
                        .enable_direct_platform_message_dispatch)),
      platform_message_handler_(
          platform_dispatch_table.custom_platform_message_handler
              ? platform_dispatch_table.custom_platform_message_handler
              : embedder_platform_message_handler_),
      platform_dispatch_table_(std::move(platform_dispatch_table)) {}

#ifdef SHELL_ENABLE_GL
PlatformViewEmbedder::PlatformViewEmbedder(
    PlatformView::Delegate& delegate,
    const flutter::TaskRunners& task_runners,
    std::unique_ptr<EmbedderSurface> embedder_surface,
    PlatformDispatchTable platform_dispatch_table,
    std::shared_ptr<EmbedderExternalViewEmbedder> external_view_embedder)
    : PlatformView(delegate, task_runners),
      external_view_embedder_(std::move(external_view_embedder)),
      embedder_surface_(std::move(embedder_surface)),
      embedder_platform_message_handler_(
          platform_dispatch_table.custom_platform_message_handler
              ? nullptr
              : std::make_shared<EmbedderPlatformMessageHandler>(
                    this,
                    task_runners.GetPlatformTaskRunner(),
                    platform_dispatch_table
                        .enable_direct_platform_message_dispatch)),
      platform_message_handler_(
          platform_dispatch_table.custom_platform_message_handler
              ? platform_dispatch_table.custom_platform_message_handler
              : embedder_platform_message_handler_),
      platform_dispatch_table_(std::move(platform_dispatch_table)) {}
#endif

#ifdef SHELL_ENABLE_METAL
PlatformViewEmbedder::PlatformViewEmbedder(
    PlatformView::Delegate& delegate,
    const flutter::TaskRunners& task_runners,
    std::unique_ptr<EmbedderSurface> embedder_surface,
    PlatformDispatchTable platform_dispatch_table,
    std::shared_ptr<EmbedderExternalViewEmbedder> external_view_embedder)
    : PlatformView(delegate, task_runners),
      external_view_embedder_(std::move(external_view_embedder)),
      embedder_surface_(std::move(embedder_surface)),
      embedder_platform_message_handler_(
          platform_dispatch_table.custom_platform_message_handler
              ? nullptr
              : std::make_shared<EmbedderPlatformMessageHandler>(
                    this,
                    task_runners.GetPlatformTaskRunner(),
                    platform_dispatch_table
                        .enable_direct_platform_message_dispatch)),
      platform_message_handler_(
          platform_dispatch_table.custom_platform_message_handler
              ? platform_dispatch_table.custom_platform_message_handler
              : embedder_platform_message_handler_),
      platform_dispatch_table_(std::move(platform_dispatch_table)) {}
#endif

#ifdef SHELL_ENABLE_VULKAN
PlatformViewEmbedder::PlatformViewEmbedder(
    PlatformView::Delegate& delegate,
    const flutter::TaskRunners& task_runners,
    std::unique_ptr<EmbedderSurfaceVulkan> embedder_surface,
    PlatformDispatchTable platform_dispatch_table,
    std::shared_ptr<EmbedderExternalViewEmbedder> external_view_embedder)
    : PlatformView(delegate, task_runners),
      external_view_embedder_(std::move(external_view_embedder)),
      embedder_surface_(std::move(embedder_surface)),
      embedder_platform_message_handler_(
          platform_dispatch_table.custom_platform_message_handler
              ? nullptr
              : std::make_shared<EmbedderPlatformMessageHandler>(
                    this,
                    task_runners.GetPlatformTaskRunner(),
                    platform_dispatch_table
                        .enable_direct_platform_message_dispatch)),
      platform_message_handler_(
          platform_dispatch_table.custom_platform_message_handler
              ? platform_dispatch_table.custom_platform_message_handler
              : embedder_platform_message_handler_),
      platform_dispatch_table_(std::move(platform_dispatch_table)) {}
#endif

PlatformViewEmbedder::~PlatformViewEmbedder() {
  if (embedder_platform_message_handler_) {
    embedder_platform_message_handler_->Shutdown();
  }
}

void PlatformViewEmbedder::UpdateSemantics(
    int64_t view_id,
    flutter::SemanticsNodeUpdates update,
    flutter::CustomAccessibilityActionUpdates actions) {
  if (platform_dispatch_table_.update_semantics_callback != nullptr) {
    platform_dispatch_table_.update_semantics_callback(
        view_id, std::move(update), std::move(actions));
  }
}

void PlatformViewEmbedder::HandlePlatformMessage(
    std::unique_ptr<flutter::PlatformMessage> message) {
  if (!message) {
    return;
  }

  if (platform_dispatch_table_.platform_message_response_callback == nullptr) {
    if (message->response()) {
      message->response()->CompleteEmpty();
    }
    return;
  }

  auto callback = platform_dispatch_table_.platform_message_response_callback;
  callback(std::move(message));
}

// |PlatformView|
std::unique_ptr<Surface> PlatformViewEmbedder::CreateRenderingSurface() {
  if (embedder_surface_ == nullptr) {
    FML_LOG(ERROR) << "Embedder surface was null.";
    return nullptr;
  }
  return embedder_surface_->CreateGPUSurface();
}

// |PlatformView|
std::shared_ptr<ExternalViewEmbedder>
PlatformViewEmbedder::CreateExternalViewEmbedder() {
  return external_view_embedder_;
}

std::shared_ptr<impeller::Context> PlatformViewEmbedder::GetImpellerContext()
    const {
  return embedder_surface_->CreateImpellerContext();
}

// |PlatformView|
sk_sp<GrDirectContext> PlatformViewEmbedder::CreateResourceContext() const {
  if (embedder_surface_ == nullptr) {
    FML_LOG(ERROR) << "Embedder surface was null.";
    return nullptr;
  }
  return embedder_surface_->CreateResourceContext();
}

// |PlatformView|
void PlatformViewEmbedder::ReleaseResourceContext() const {
  if (embedder_surface_ == nullptr) {
    FML_LOG(ERROR) << "Embedder surface was null.";
    return;
  }
  embedder_surface_->ReleaseResourceContext();
}

// |PlatformView|
std::unique_ptr<VsyncWaiter> PlatformViewEmbedder::CreateVSyncWaiter() {
  if (!platform_dispatch_table_.vsync_callback) {
    // Superclass implementation creates a timer based fallback.
    return PlatformView::CreateVSyncWaiter();
  }

  return std::make_unique<VsyncWaiterEmbedder>(
      platform_dispatch_table_.vsync_callback, task_runners_);
}

// |PlatformView|
std::unique_ptr<std::vector<std::string>>
PlatformViewEmbedder::ComputePlatformResolvedLocales(
    const std::vector<std::string>& supported_locale_data) {
  if (platform_dispatch_table_.compute_platform_resolved_locale_callback !=
      nullptr) {
    return platform_dispatch_table_.compute_platform_resolved_locale_callback(
        supported_locale_data);
  }
  std::unique_ptr<std::vector<std::string>> out =
      std::make_unique<std::vector<std::string>>();
  return out;
}

// |PlatformView|
void PlatformViewEmbedder::OnPreEngineRestart() const {
  if (platform_dispatch_table_.on_pre_engine_restart_callback != nullptr) {
    platform_dispatch_table_.on_pre_engine_restart_callback();
  }
}

// |PlatformView|
void PlatformViewEmbedder::SendChannelUpdate(const std::string& name,
                                             bool listening) {
  if (platform_dispatch_table_.on_channel_update != nullptr) {
    platform_dispatch_table_.on_channel_update(name, listening);
  }
}

void PlatformViewEmbedder::RequestViewFocusChange(
    const ViewFocusChangeRequest& request) {
  if (platform_dispatch_table_.view_focus_change_request_callback != nullptr) {
    platform_dispatch_table_.view_focus_change_request_callback(request);
  }
}

// |PlatformView|
void PlatformViewEmbedder::RequestDartDeferredLibrary(
    intptr_t loading_unit_id) {
  if (platform_dispatch_table_.request_dart_deferred_library_callback !=
      nullptr) {
    platform_dispatch_table_.request_dart_deferred_library_callback(
        loading_unit_id);
  } else {
    delegate_.LoadDartDeferredLibraryError(
        loading_unit_id, "Deferred loading is not configured in the embedder.",
        /*transient=*/false);
  }
}

// |PlatformView|
void PlatformViewEmbedder::LoadDartDeferredLibrary(
    intptr_t loading_unit_id,
    std::unique_ptr<const fml::Mapping> snapshot_data,
    std::unique_ptr<const fml::Mapping> snapshot_instructions) {
  delegate_.LoadDartDeferredLibrary(loading_unit_id, std::move(snapshot_data),
                                    std::move(snapshot_instructions));
}

// |PlatformView|
void PlatformViewEmbedder::LoadDartDeferredLibraryError(
    intptr_t loading_unit_id,
    const std::string error_message,
    bool transient) {
  delegate_.LoadDartDeferredLibraryError(loading_unit_id, error_message,
                                         transient);
}

// |PlatformView|
double PlatformViewEmbedder::GetScaledFontSize(double unscaled_font_size,
                                               int configuration_id) const {
  if (platform_dispatch_table_.get_scaled_font_size_callback != nullptr) {
    return platform_dispatch_table_.get_scaled_font_size_callback(
        unscaled_font_size, configuration_id);
  }
  return -1.0;
}

void PlatformViewEmbedder::InvokePlatformMessageResponseCallback(
    int response_id,
    std::unique_ptr<fml::Mapping> mapping) {
  if (platform_dispatch_table_.platform_message_response_completion_callback !=
      nullptr) {
    auto callback =
        platform_dispatch_table_.platform_message_response_completion_callback;
    callback(response_id, std::move(mapping));
  }
}

void PlatformViewEmbedder::InvokePlatformMessageEmptyResponseCallback(
    int response_id) {
  if (platform_dispatch_table_
          .platform_message_empty_response_completion_callback != nullptr) {
    auto callback = platform_dispatch_table_
                        .platform_message_empty_response_completion_callback;
    callback(response_id);
  }
}

std::shared_ptr<PlatformMessageHandler>
PlatformViewEmbedder::GetPlatformMessageHandler() const {
  return platform_message_handler_;
}

}  // namespace flutter
