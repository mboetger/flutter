// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/embedder/embedder_engine.h"

#include "flutter/fml/make_copyable.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/shell/platform/embedder/embedder_asset_resolver.h"
#include "flutter/shell/platform/embedder/embedder_image_generator.h"
#include "flutter/shell/platform/embedder/vsync_waiter_embedder.h"

namespace flutter {

struct ShellArgs {
  Settings settings;
  Shell::CreateCallback<PlatformView> on_create_platform_view;
  Shell::CreateCallback<Rasterizer> on_create_rasterizer;
  std::vector<FlutterImageDecoder> image_decoders;
  ShellArgs(const Settings& p_settings,
            Shell::CreateCallback<PlatformView> p_on_create_platform_view,
            Shell::CreateCallback<Rasterizer> p_on_create_rasterizer,
            std::vector<FlutterImageDecoder> p_image_decoders)
      : settings(p_settings),
        on_create_platform_view(std::move(p_on_create_platform_view)),
        on_create_rasterizer(std::move(p_on_create_rasterizer)),
        image_decoders(std::move(p_image_decoders)) {}
};

EmbedderEngine::EmbedderEngine(
    std::shared_ptr<EmbedderThreadHost> thread_host,
    const flutter::TaskRunners& task_runners,
    const flutter::Settings& settings,
    RunConfiguration run_configuration,
    const Shell::CreateCallback<PlatformView>& on_create_platform_view,
    const Shell::CreateCallback<Rasterizer>& on_create_rasterizer,
    std::unique_ptr<EmbedderExternalTextureResolver> external_texture_resolver,
    PlatformViewCreationCallbackFactory platform_view_creation_callback_factory,
    std::vector<FlutterImageDecoder> initial_image_decoders)
    : thread_host_(std::move(thread_host)),
      task_runners_(task_runners),
      run_configuration_(std::move(run_configuration)),
      shell_args_(
          std::make_unique<ShellArgs>(settings,
                                      on_create_platform_view,
                                      on_create_rasterizer,
                                      std::move(initial_image_decoders))),
      on_create_platform_view_(on_create_platform_view),
      on_create_rasterizer_(on_create_rasterizer),
      external_texture_resolver_(std::move(external_texture_resolver)),
      platform_view_creation_callback_factory_(
          std::move(platform_view_creation_callback_factory)) {}

EmbedderEngine::EmbedderEngine(
    std::shared_ptr<EmbedderThreadHost> thread_host,
    const flutter::TaskRunners& task_runners,
    std::unique_ptr<Shell> shell,
    const Shell::CreateCallback<PlatformView>& on_create_platform_view,
    const Shell::CreateCallback<Rasterizer>& on_create_rasterizer,
    std::unique_ptr<EmbedderExternalTextureResolver> external_texture_resolver,
    PlatformViewCreationCallbackFactory platform_view_creation_callback_factory,
    std::vector<FlutterImageDecoder> initial_image_decoders)
    : thread_host_(std::move(thread_host)),
      task_runners_(task_runners),
      on_create_platform_view_(on_create_platform_view),
      on_create_rasterizer_(on_create_rasterizer),
      shell_(std::move(shell)),
      external_texture_resolver_(std::move(external_texture_resolver)),
      platform_view_creation_callback_factory_(
          std::move(platform_view_creation_callback_factory)) {
  if (shell_) {
    for (const auto& decoder : initial_image_decoders) {
      shell_->RegisterImageDecoder(
          [decoder](sk_sp<SkData> buffer) -> std::shared_ptr<ImageGenerator> {
            return EmbedderImageGenerator::Make(decoder, std::move(buffer));
          },
          1);
    }
  }
}

EmbedderEngine::~EmbedderEngine() = default;

std::unique_ptr<EmbedderEngine> EmbedderEngine::Spawn(
    RunConfiguration run_configuration,
    const std::string& initial_route,
    std::vector<FlutterImageDecoder> image_decoders,
    void* user_data) {
  if (!IsValid()) {
    return nullptr;
  }
  Shell::CreateCallback<PlatformView> on_create_platform_view =
      on_create_platform_view_;
  if (platform_view_creation_callback_factory_ && user_data != nullptr) {
    on_create_platform_view =
        platform_view_creation_callback_factory_(user_data);
  }
  if (!on_create_platform_view || !on_create_rasterizer_) {
    return nullptr;
  }
  std::unique_ptr<Shell> spawned_shell =
      shell_->Spawn(std::move(run_configuration), initial_route,
                    on_create_platform_view, on_create_rasterizer_);
  if (!spawned_shell) {
    return nullptr;
  }

  for (const auto& decoder : image_decoders) {
    spawned_shell->RegisterImageDecoder(
        [decoder](sk_sp<SkData> buffer) -> std::shared_ptr<ImageGenerator> {
          return EmbedderImageGenerator::Make(decoder, std::move(buffer));
        },
        1);
  }

  std::unique_ptr<EmbedderExternalTextureResolver> external_texture_resolver;
  if (external_texture_resolver_) {
    external_texture_resolver =
        std::make_unique<EmbedderExternalTextureResolver>(
            *external_texture_resolver_);
  } else {
    external_texture_resolver =
        std::make_unique<EmbedderExternalTextureResolver>();
  }

  return std::make_unique<EmbedderEngine>(
      thread_host_, task_runners_, std::move(spawned_shell),
      on_create_platform_view, on_create_rasterizer_,
      std::move(external_texture_resolver),
      platform_view_creation_callback_factory_, image_decoders);
}

bool EmbedderEngine::LaunchShell() {
  if (!shell_args_) {
    FML_DLOG(ERROR) << "Invalid shell arguments.";
    return false;
  }

  if (shell_) {
    FML_DLOG(ERROR) << "Shell already initialized";
  }

  auto image_decoders = std::move(shell_args_->image_decoders);
  shell_ = Shell::Create(
      flutter::PlatformData(), task_runners_, shell_args_->settings,
      shell_args_->on_create_platform_view, shell_args_->on_create_rasterizer);

  // Reset the args no matter what. They will never be used to initialize a
  // shell again.
  shell_args_.reset();

  if (shell_) {
    for (const auto& decoder : image_decoders) {
      shell_->RegisterImageDecoder(
          [decoder](sk_sp<SkData> buffer) -> std::shared_ptr<ImageGenerator> {
            return EmbedderImageGenerator::Make(decoder, std::move(buffer));
          },
          1);
    }
  }

  return IsValid();
}

bool EmbedderEngine::CollectShell() {
  shell_.reset();
  return IsValid();
}

void EmbedderEngine::CollectThreadHost() {
  if (!thread_host_) {
    return;
  }

  // If other engines (e.g. spawned engines) are still sharing this thread host,
  // do not invalidate runners or tear down OS threads yet.
  if (thread_host_.use_count() > 1) {
    thread_host_.reset();
    return;
  }

  // Once the collected, EmbedderThreadHost::RunnerIsValid will return false for
  // all runners belonging to this thread host. This must be done with UI task
  // runner blocked to prevent possible raciness that could happen when
  // destroying the thread host in the middle of UI task runner execution. This
  // is not an issue for other runners, because raster task runner should not
  // have anything scheduled after engine shutdown and platform task runner is
  // where this method is called from.
  if (thread_host_->GetTaskRunners().GetUITaskRunner() &&
      !thread_host_->GetTaskRunners()
           .GetUITaskRunner()
           ->RunsTasksOnCurrentThread()) {
    fml::AutoResetWaitableEvent ui_thread_running;
    fml::AutoResetWaitableEvent ui_thread_block;
    fml::AutoResetWaitableEvent ui_thread_finished;

    thread_host_->GetTaskRunners().GetUITaskRunner()->PostTask([&] {
      ui_thread_running.Signal();
      ui_thread_block.Wait();
      ui_thread_finished.Signal();
    });

    // Wait until the task is running on the UI thread.
    ui_thread_running.Wait();
    thread_host_->InvalidateActiveRunners();
    ui_thread_block.Signal();

    // Needed to keep ui_thread_block in scope until the UI thread execution
    // finishes.
    ui_thread_finished.Wait();
  } else {
    thread_host_->InvalidateActiveRunners();
  }
  thread_host_.reset();
}

bool EmbedderEngine::RunRootIsolate() {
  if (!IsValid() || !run_configuration_.has_value() ||
      !run_configuration_->IsValid()) {
    return false;
  }
  shell_->RunEngine(std::move(*run_configuration_));
  return true;
}

bool EmbedderEngine::IsValid() const {
  return static_cast<bool>(shell_);
}

const TaskRunners& EmbedderEngine::GetTaskRunners() const {
  return task_runners_;
}

bool EmbedderEngine::NotifyCreated() {
  if (!IsValid()) {
    return false;
  }

  shell_->GetPlatformView()->NotifyCreated();
  return true;
}

bool EmbedderEngine::NotifyDestroyed() {
  if (!IsValid()) {
    return false;
  }

  shell_->GetPlatformView()->NotifyDestroyed();

  return true;
}

bool EmbedderEngine::SetViewportMetrics(
    int64_t view_id,
    const flutter::ViewportMetrics& metrics) {
  if (!IsValid()) {
    return false;
  }

  auto platform_view = shell_->GetPlatformView();
  if (!platform_view) {
    return false;
  }
  platform_view->SetViewportMetrics(view_id, metrics);
  return true;
}

bool EmbedderEngine::DispatchPointerDataPacket(
    std::unique_ptr<flutter::PointerDataPacket> packet) {
  if (!IsValid() || !packet) {
    return false;
  }

  auto platform_view = shell_->GetPlatformView();
  if (!platform_view) {
    return false;
  }

  platform_view->DispatchPointerDataPacket(std::move(packet));
  return true;
}

bool EmbedderEngine::SendPlatformMessage(
    std::unique_ptr<PlatformMessage> message) {
  if (!IsValid() || !message) {
    return false;
  }

  auto platform_view = shell_->GetPlatformView();
  if (!platform_view) {
    return false;
  }

  platform_view->DispatchPlatformMessage(std::move(message));
  return true;
}

bool EmbedderEngine::RegisterTexture(int64_t texture) {
  if (!IsValid()) {
    return false;
  }
  shell_->GetPlatformView()->RegisterTexture(
      external_texture_resolver_->ResolveExternalTexture(texture));
  return true;
}

bool EmbedderEngine::UnregisterTexture(int64_t texture) {
  if (!IsValid()) {
    return false;
  }
  shell_->GetPlatformView()->UnregisterTexture(texture);
  return true;
}

bool EmbedderEngine::MarkTextureFrameAvailable(int64_t texture) {
  if (!IsValid()) {
    return false;
  }
  shell_->GetPlatformView()->MarkTextureFrameAvailable(texture);
  return true;
}

bool EmbedderEngine::SetSemanticsEnabled(bool enabled) {
  if (!IsValid()) {
    return false;
  }

  auto platform_view = shell_->GetPlatformView();
  if (!platform_view) {
    return false;
  }
  platform_view->SetSemanticsEnabled(enabled);
  return true;
}

bool EmbedderEngine::SetAccessibilityFeatures(int32_t flags) {
  if (!IsValid()) {
    return false;
  }
  auto platform_view = shell_->GetPlatformView();
  if (!platform_view) {
    return false;
  }
  platform_view->SetAccessibilityFeatures(flags);
  return true;
}

bool EmbedderEngine::DispatchSemanticsAction(int64_t view_id,
                                             int node_id,
                                             flutter::SemanticsAction action,
                                             fml::MallocMapping args) {
  if (!IsValid()) {
    return false;
  }
  auto platform_view = shell_->GetPlatformView();
  if (!platform_view) {
    return false;
  }
  platform_view->DispatchSemanticsAction(view_id, node_id, action,
                                         std::move(args));
  return true;
}

bool EmbedderEngine::OnVsyncEvent(intptr_t baton,
                                  fml::TimePoint frame_start_time,
                                  fml::TimePoint frame_target_time) {
  if (!IsValid()) {
    return false;
  }

  return VsyncWaiterEmbedder::OnEmbedderVsync(
      task_runners_, baton, frame_start_time, frame_target_time);
}

bool EmbedderEngine::ReloadSystemFonts() {
  if (!IsValid()) {
    return false;
  }

  return shell_->ReloadSystemFonts();
}

bool EmbedderEngine::PostRenderThreadTask(const fml::closure& task) {
  if (!IsValid()) {
    return false;
  }

  shell_->GetTaskRunners().GetRasterTaskRunner()->PostTask(task);
  return true;
}

bool EmbedderEngine::RunTask(const FlutterTask* task) {
  // The shell doesn't need to be running or valid for access to the thread
  // host. This is why there is no `IsValid` check here. This allows embedders
  // to perform custom task runner interop before the shell is running.
  if (task == nullptr || !thread_host_) {
    return false;
  }
  auto result = thread_host_->PostTask(reinterpret_cast<intptr_t>(task->runner),
                                       task->task);
  // If the UI and platform threads are separate, the microtask queue is
  // flushed through MessageLoopTaskQueues observer.
  // If the UI and platform threads are merged, the UI task runner has no
  // associated task queue, and microtasks need to be flushed manually
  // after running the task.
  if (result && shell_ && task_runners_.GetUITaskRunner() &&
      task_runners_.GetUITaskRunner()->RunsTasksOnCurrentThread() &&
      !task_runners_.GetUITaskRunner()->GetTaskQueueId().is_valid()) {
    shell_->FlushMicrotaskQueue();
  }

  return result;
}

bool EmbedderEngine::PostTaskOnEngineManagedNativeThreads(
    const std::function<void(FlutterNativeThreadType)>& closure) const {
  if (!IsValid() || closure == nullptr) {
    return false;
  }

  const auto trampoline = [closure](
                              FlutterNativeThreadType type,
                              const fml::RefPtr<fml::TaskRunner>& runner) {
    runner->PostTask([closure, type] { closure(type); });
  };

  // Post the task to all thread host threads.
  const auto& task_runners = shell_->GetTaskRunners();
  trampoline(kFlutterNativeThreadTypeRender,
             task_runners.GetRasterTaskRunner());
  trampoline(kFlutterNativeThreadTypeWorker, task_runners.GetIOTaskRunner());
  trampoline(kFlutterNativeThreadTypeUI, task_runners.GetUITaskRunner());
  trampoline(kFlutterNativeThreadTypePlatform,
             task_runners.GetPlatformTaskRunner());

  // Post the task to all worker threads.
  auto vm = shell_->GetDartVM();
  vm->GetConcurrentMessageLoop()->PostTaskToAllWorkers(
      [closure]() { closure(kFlutterNativeThreadTypeWorker); });

  return true;
}

bool EmbedderEngine::ScheduleFrame() {
  if (!IsValid()) {
    return false;
  }

  auto platform_view = shell_->GetPlatformView();
  if (!platform_view) {
    return false;
  }
  platform_view->ScheduleFrame();
  return true;
}

bool EmbedderEngine::UpdateAssetResolvers(
    const FlutterAssetResolver* const* asset_resolvers,
    size_t asset_resolvers_count) {
  if (!IsValid()) {
    return false;
  }

  // Copy resolver structs so we can safely pass them to the UI thread.
  std::vector<FlutterAssetResolver> resolvers_copy;
  resolvers_copy.reserve(asset_resolvers_count);
  for (size_t i = 0; i < asset_resolvers_count; ++i) {
    if (asset_resolvers != nullptr && asset_resolvers[i] != nullptr) {
      resolvers_copy.push_back(*asset_resolvers[i]);
    }
  }

  fml::AutoResetWaitableEvent latch;
  bool success = false;
  fml::TaskRunner::RunNowOrPostTask(
      task_runners_.GetUITaskRunner(),
      [&latch, &success, engine = shell_->GetEngine(),
       resolvers_copy = std::move(resolvers_copy)]() mutable {
        if (engine) {
          auto old_asset_manager = engine->GetAssetManager();
          auto new_asset_manager = std::make_shared<AssetManager>();

          for (const auto& resolver_struct : resolvers_copy) {
            auto resolver =
                std::make_unique<EmbedderAssetResolver>(&resolver_struct);
            if (resolver->IsValid()) {
              new_asset_manager->PushBack(std::move(resolver));
            }
          }

          if (old_asset_manager) {
            auto old_resolvers = old_asset_manager->TakeResolvers();
            for (auto& old_resolver : old_resolvers) {
              if (old_resolver &&
                  old_resolver->GetType() !=
                      AssetResolver::AssetResolverType::kCustomResolver) {
                new_asset_manager->PushBack(std::move(old_resolver));
              }
            }
          }

          engine->UpdateAssetManager(new_asset_manager);
          success = true;
        }
        latch.Signal();
      });
  latch.Wait();
  return success;
}

bool EmbedderEngine::LoadDartDeferredLibrary(
    intptr_t loading_unit_id,
    std::unique_ptr<const fml::Mapping> snapshot_data,
    std::unique_ptr<const fml::Mapping> snapshot_instructions) {
  if (!IsValid()) {
    return false;
  }
  auto platform_view = shell_->GetPlatformView();
  if (!platform_view) {
    return false;
  }
  platform_view->LoadDartDeferredLibrary(loading_unit_id,
                                         std::move(snapshot_data),
                                         std::move(snapshot_instructions));
  return true;
}

bool EmbedderEngine::LoadDartDeferredLibraryError(
    intptr_t loading_unit_id,
    const std::string& error_message,
    bool transient) {
  if (!IsValid()) {
    return false;
  }
  auto platform_view = shell_->GetPlatformView();
  if (!platform_view) {
    return false;
  }
  platform_view->LoadDartDeferredLibraryError(loading_unit_id, error_message,
                                              transient);
  return true;
}

Rasterizer::Screenshot EmbedderEngine::Screenshot(
    Rasterizer::ScreenshotType type,
    bool base64_encode) {
  if (!IsValid()) {
    return {};
  }
  return shell_->Screenshot(type, base64_encode);
}

Shell& EmbedderEngine::GetShell() {
  FML_DCHECK(shell_);
  return *shell_.get();
}

bool EmbedderEngine::RegisterImageDecoder(const FlutterImageDecoder* decoder,
                                          int32_t priority) {
  if (!IsValid() || !decoder) {
    return false;
  }

  if (decoder->struct_size != sizeof(FlutterImageDecoder) ||
      !decoder->create_generator ||
      decoder->generator.struct_size != sizeof(FlutterImageGenerator) ||
      !decoder->generator.get_image_info || !decoder->generator.get_pixels) {
    return false;
  }

  FlutterImageDecoder decoder_copy = *decoder;

  auto task = [shell = shell_.get(), decoder_copy, priority]() {
    if (shell) {
      shell->RegisterImageDecoder(
          [decoder_copy](
              sk_sp<SkData> buffer) -> std::shared_ptr<ImageGenerator> {
            return EmbedderImageGenerator::Make(decoder_copy,
                                                std::move(buffer));
          },
          priority);
    }
  };

  if (task_runners_.GetPlatformTaskRunner()->RunsTasksOnCurrentThread()) {
    task();
  } else {
    task_runners_.GetPlatformTaskRunner()->PostTask(task);
  }

  return true;
}

}  // namespace flutter
