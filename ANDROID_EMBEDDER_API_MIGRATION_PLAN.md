# Flutter Android Embedder API Migration Master Plan

- **Author**: Senior Engineer, Flutter Android Team
- **Target Repository**: `flutter/flutter` (`engine/src/flutter`)
- **Scope**: `engine/src/flutter/shell/platform/android`
- **Final Dependency Boundary**:
  - `//flutter/shell/platform/embedder:embedder_as_internal_library` (**Required**)
  - `//flutter/fml` (**Optional**)
  - `//flutter/shell/platform/common` (**Optional**)
  - `//flutter/third_party` (**Optional**)
  - **Strictly Prohibited**: `//flutter/assets`, `//flutter/common`, `//flutter/flow`, `//flutter/impeller`, `//flutter/lib/ui`, `//flutter/runtime`, `//flutter/shell/common`, `//flutter/skia`, `//flutter/txt`, `//flutter/vulkan`.

---

## 1. Executive Summary & Architectural Vision

Flutter's Android embedder (`engine/src/flutter/shell/platform/android`) currently bypasses the official public C Embedder API (`embedder.h`), coupling directly to internal engine C++ symbols: `flutter::Shell`, `flutter::PlatformView`, `flutter::ThreadHost`, `flutter::Rasterizer`, `flutter::RunConfiguration`, `flutter::DartVM`, `flutter::ImageGeneratorRegistry`, `flutter::DartCallbackCache`, and `flutter::Settings`.

This tight coupling creates severe technical debt:
1. Core engine refactors risk destabilizing Android-specific logic.
2. Android cannot be compiled or tested in isolation from engine internals.
3. Code duplication flourishes between Android, iOS, macOS, Windows, Linux, and custom embedders.

### The Target State
The Android embedder will become a pure platform coordinator that communicates with the Flutter engine exclusively through the public ABI-stable C Embedder API (`embedder.h` via `FlutterEngineProcTable` / `embedder_as_internal_library`). The engine produces z-ordered layers (`FlutterLayer`) presented through a `FlutterCompositor`, while Android delegates lifecycle, threading, input, and rasterization to `FlutterEngineInitialize`, `FlutterEngineRunInitialized`, `FlutterEngineSpawn`, `FlutterEngineSendPointerEvent`, etc.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           LEGACY ARCHITECTURE                               │
│                                                                             │
│  FlutterJNI.java ──► PlatformViewAndroidJNIImpl ──► AndroidShellHolder      │
│                              │                               │              │
│                              ▼                               ▼              │
│                    PlatformViewAndroid              flutter::Shell          │
│                 (subclasses PlatformView)           flutter::ThreadHost     │
│                              │                      flutter::Rasterizer     │
│                              ▼                               │              │
│                  AndroidExternalViewEmbedder2 ◄──────────────┘              │
└─────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼ [MIGRATION]
┌─────────────────────────────────────────────────────────────────────────────┐
│                           TARGET ARCHITECTURE                               │
│                                                                             │
│  FlutterJNI.java ──► PlatformViewAndroidJNIImpl ──► AndroidEngine           │
│                              │                               │              │
│                              ▼                               ▼              │
│                     PlatformViewAndroid             Public C Embedder API   │
│                 (Pure Platform Coordinator)       (FlutterEngineProcTable)  │
│                              │                               │              │
│                              ▼                               ▼              │
│                      AndroidCompositor ◄────────── PlatformViewEmbedder     │
│                 (FlutterCompositor Callbacks)       EmbedderThreadHost      │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Core Invariants & Quality Contract

To guarantee that the migration is safe, reversible, and non-disruptive, every PR in this plan must adhere to the following invariants:

1. **Continuous Test Invariance & Dual-State Flag Testing**:
   - All unit tests (`flutter_shell_native_unittests`, `embedder_unittests`, `embedder_gl_unittests`, `embedder_vk_unittests`) must pass on every PR.
   - All scenario tests (`dev/integration_tests/android_views`) and integration tests (`dev/integration_tests/channels`, `dev/integration_tests/external_textures`) must remain 100% green.
   - **Feature Flag Dual-State Gate**: Any PR that changes rendering logic **must be gated by a feature flag**. When a feature flag is present, tests **must pass with BOTH states of the flag** (run all unit, scenario, and integration tests with `flag = true` AND `flag = false`).
2. **Mandatory Iterative Adversarial Review Loop**:
   - Every single PR must undergo an adversarial code review by an independent agent (e.g. `reidbaker-agent`) before merging.
   - All reviewer findings must be addressed, followed by subsequent re-reviews until zero findings remain.
3. **Golden Pixel Alignment**:
   - Golden test outputs generated with the local engine must remain pixel-identical or within existing tolerance thresholds to baseline goldens captured prior to the migration.
4. **Local Engine Application Continuity**:
   - A standard Flutter application built against local engine artifacts (`flutter run --local-engine`) must continue to launch, render, receive touch input, and hot-restart seamlessly at every stage.
5. **Piecewise, Revertible Delivery**:
   - No monolithic PRs. Each PR must represent a standalone, reviewable commit with its own dedicated unit and integration tests.
6. **Dual-Path Zero-Regression Safety**:
   - Architectural decoupling using delegates and facades occurs first, followed by dual-path execution gated behind a runtime feature flag (`--enable-android-embedder-api`), with final cleanup only after validation.

7. **GN Target Isolation & Visibility Ratchet (Strict Compile-Time Quarantine)**:
   - In PR 2.1, all legacy engine C++ source files and all internal engine dependencies are quarantined into `source_set("android_legacy_engine_holder")` with strict visibility: `visibility = [ ":flutter_shell_native_src", ":flutter_shell_native_unittests" ]`.
   - `source_set("flutter_shell_native_src")` is immediately configured with only the clean/allowed public dependency set (`//flutter/shell/platform/embedder:embedder_as_internal_library`, `//flutter/fml`, `//flutter/shell/platform/common`, `//flutter/third_party`).
   - Because `android_legacy_engine_holder` is a private `deps` of `flutter_shell_native_src`, include directories of internal engine headers (`shell.h`, `layer.h`, `thread_host.h`, etc.) do **NOT** leak into migrated source files. Any direct dependency or internal engine include in newly written/migrated code will cause an immediate compile-time build failure.
   - Throughout Phases 2, 3, and 4, dependencies are monotonically pruned from `android_legacy_engine_holder`. In Phase 5.2, `android_legacy_engine_holder` is deleted entirely.

---

## 3. Critical Technical Lessons & Pitfall Mitigations

Analysis of prior prototypes (#1, #2, #3), alignment with the iOS Embedder Migration, and findings from the adversarial review reveal 8 foundational traps that must be mitigated:

### 3.1. The Surface Lifecycle Clash & Early Frame Requests
- **The Problem**: In the legacy flow, `Shell::Create` initializes early, but the rendering surface is not attached until the Android OS creates the `SurfaceView`/`TextureView` (triggering `nativeSurfaceCreated`). In contrast, calling `FlutterEngineRunInitialized` spins up Raster and IO threads immediately, requesting a render context (`make_current`) before the onscreen surface exists.
- **The Solution**: 
  - Do not assume `onscreen_surface_` is non-null during initial `make_current` callbacks.
  - Implement surface availability semantics (mirroring iOS `SetGpuAvailability` and `EmbedderSurface` lifecycle decoupling). Provide an offscreen PBuffer or handle null surface draw calls gracefully until `nativeSurfaceCreated` attaches the window.

### 3.2. Synchronous `surfaceDestroyed` Race Condition & Detach Barrier
- **The Problem**: In Android's `SurfaceHolder.Callback`, `surfaceDestroyed` is invoked synchronously on the Android main thread. Once `surfaceDestroyed` returns, the OS WindowManager immediately frees the underlying `ANativeWindow`. If the engine's raster thread is in the middle of executing `eglSwapBuffers` or `vkQueuePresentKHR`, a fatal crash occurs (`SIGSEGV` or `EGL_BAD_NATIVE_WINDOW`).
- **The Solution**:
  - Implement a synchronous surface detachment barrier in `AndroidEngine` / `AndroidCompositor`.
  - When `nativeSurfaceDestroyed` is called, `AndroidEngine` must signal the compositor to withdraw the window target and synchronously block on the rasterizer via `fml::AutoResetWaitableEvent` until pending frame presentation completes and the native window handle is released.

### 3.3. Multi-Thread Context Collisions (`EGL_BAD_ACCESS` 12290)
- **The Problem**: OpenGL contexts are strictly bound to one thread at a time. When the raster thread (`make_current`) and IO thread (`make_resource_current`) share fallback offscreen contexts before the onscreen window is ready, EGL throws fatal `EGL_BAD_ACCESS` errors.
- **The Solution**:
  - Implement dedicated context separation in `AndroidContextGLImpeller` / `AndroidSurfaceGLImpeller`.
  - Maintain a separate offscreen PBuffer context specifically for the IO thread, completely isolated from raster thread surface bindings.

### 3.4. JIT Asset Resolution in APK Archives
- **The Problem**: In debug/JIT mode, `FlutterEngineInitialize` defaults to `fml::IsFile` to find `kernel_blob.bin` on disk, which fails on Android because assets reside inside the APK archive (`AAssetManager`).
- **The Solution**:
  - Implement `FlutterAssetResolver` in `embedder.h` (PR 1.1), allowing custom memory-mapped asset retrieval directly via `APKAssetProvider` without file-system extraction.

### 3.5. Background Platform Channel Handlers (`FlutterTaskQueue`)
- **The Problem**: `EmbedderPlatformMessageHandler` by default trampolines messages to the platform thread, but Android's `BinaryMessenger.TaskQueue` supports background worker threads.
- **The Solution**:
  - Implement thread-safe response handling via `FlutterPlatformMessageResponseHandle` without forcing main-thread redirection for background task queues.

### 3.6. Platform View Extended Mutations & DPR Scaling Normalization
- **The Problem**: `FlutterPlatformViewMutationType` in `embedder.h` previously lacked support for `ClipPath` and rounded superellipses (`ClipRSE`). Furthermore, `FlutterPlatformView` mutations pre-scale coordinates with the device pixel ratio (DPR). If not normalized before passing to `FlutterMutatorsStack.java`, coordinates are scaled twice.
- **The Solution**:
  - Add `kFlutterPlatformViewMutationTypeClipPath` and `kFlutterPlatformViewMutationTypeClipRoundedSuperellipse` to `embedder.h` (PR 1.10).
  - Explicitly isolate the root DPR matrix in `AndroidCompositor` to pass unscaled logical coordinates to Android's `FlutterMutatorsStack`.

### 3.7. Vulkan Backing Store & AHardwareBuffer Integration
- **The Problem**: Forcing the Android embedder to manually import `AHardwareBuffer` into Vulkan requires hundreds of lines of Vulkan driver memory allocation and extensions without access to `//flutter/impeller` or `//flutter/vulkan`.
- **The Solution**:
  - Implement `MakeRenderTargetFromBackingStoreImpeller` for `kFlutterBackingStoreTypeVulkan` inside `shell/platform/embedder/embedder.cc` (PR 1.2).
  - Add native `AHardwareBuffer` backing store and external texture support to `embedder.h` (PR 1.3), encapsulating driver importation inside `shell/platform/embedder/`.

### 3.8. Compile-Time Dependency Containment via GN Visibility & Quarantine Target
- **The Problem**: Over a multi-phase refactor, developers or LLM agents risk inadvertently introducing `#include "flutter/shell/common/..."` or `#include "flutter/flow/..."` into newly written or migrated files (such as `AndroidEngine` or `AndroidCompositor`), silently violating the final architectural boundary and accumulating hidden coupling.
- **The Solution**:
  - Use GN's native dependency encapsulation and target visibility controls to construct a compile-time firewall in `engine/src/flutter/shell/platform/android/BUILD.gn`.
  - Split the native sources into two targets in PR 2.1:
    1. `source_set("android_legacy_engine_holder")`: Contains all legacy C++ sources (`android_shell_holder.cc`, `platform_view_android_adapter.cc`, legacy `android_surface_*.cc`, etc.) and all internal engine `deps` (`assets`, `common`, `flow`, `impeller`, `lib/ui`, `runtime`, `shell/common`, `skia`, `txt`, `vulkan`). Set `visibility = [ ":flutter_shell_native_src", ":flutter_shell_native_unittests" ]`.
    2. `source_set("flutter_shell_native_src")`: The primary target containing only migrated embedder C API sources (`android_engine.cc`, `android_compositor.cc`, `platform_view_android_jni_impl.cc`, etc.). It declares only approved public dependencies in `public_deps` (`embedder_as_internal_library`, `fml`, `shell/platform/common`, `third_party`), and links `:android_legacy_engine_holder` as a private `deps`.
  - Result: GN does not propagate include directories from private `deps`. Any newly migrated source file in `flutter_shell_native_src` that attempts to `#include` an internal engine header will fail compilation immediately.
  - Throughout Phases 2, 3, and 4, dependencies are monotonically pruned from `android_legacy_engine_holder`. In Phase 5.2, `android_legacy_engine_holder` is completely deleted.

---

## 4. LLM Context Management & Execution Governance

Large-scale refactorings spanning 20+ PRs frequently fail when handled by LLMs due to **context window saturation, state drift, missed invariants, and hallucinated APIs**.

To guarantee deterministic, defect-free execution, the LLM must follow this strict **Governance Protocol**:

### 4.1. The "Single-PR Execution Bubble" Protocol
The LLM must treat every PR as an isolated transaction:
1. **Isolated Context**: Never dump the entire `shell/platform/android/` codebase into the prompt. Read only the specific files declared in the PR Contract.
2. **Explicit Entry Preconditions**: Verify the base git branch and commit SHA before making changes.
3. **Feature Flag Gating for Rendering Changes**:
   - Any PR that changes rendering logic (compositor, surface manager, backing stores, textures, or mutators) **MUST** be gated by a feature flag (e.g. `--enable-android-embedder-api`).
   - When a feature flag is present, tests **MUST pass with BOTH states of the flag** (run integration tests, scenario tests, and unit tests with `flag = true` AND `flag = false`).
4. **Adversarial Code Review Loop**:
   - Each PR **must receive an adversarial review from an independent agent** (`reidbaker-agent`) prior to landing.
   - All reviewer feedback, edge cases, thread-safety hazards, memory leak risks, and invariant checks must be addressed.
   - Another adversarial review must happen again on the updated diff.
   - **This loop must continue until there is NO remaining feedback from the adversarial review.**
5. **Rigorous Verification Gate**: Run the exact compilation and test targets specified for the PR under both flag states where applicable. Do not advance to the next PR until all tests are green.
6. **Atomic Commit & Checkpoint**: Create a clean git commit tagged with the Phase and PR number, updating `MIGRATION_LEDGER.md`.

### 4.2. The Living State Ledger (`MIGRATION_LEDGER.md`)
A persistent markdown ledger (`MIGRATION_LEDGER.md`) must be maintained in the workspace root. At the start of every turn or new session, the LLM reads **only** `MIGRATION_LEDGER.md` and the master plan to restore its full operational state.

#### Ledger Schema:
```markdown
# Migration State Ledger
- **Active Phase**: Phase 2.1
- **Current Branch**: `android-embedder-migration/phase-2.1-decouple-platform-view`
- **Base Commit SHA**: `866b24f5df2`
- **Completed PRs**: [0.1, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 1.10, 1.11]
- **Active Blockers / Edge Cases**: None
- **Adversarial Review Status**: Approved with 0 findings (Reviewer: reidbaker-agent)
- **Last Verification Status**: ALL PASS (flag=true & flag=false for flutter_shell_native_unittests, embedder_unittests, android_views)
```

### 4.3. Multi-Agent Delegation Strategy
- **Parent Agent (Architect & Orchestrator)**: Manages roadmap progression, updates `MIGRATION_LEDGER.md`, and runs full integration verification.
- **Specialized Subagents**:
  - `Adversarial Reviewer` (`reidbaker-agent`): Conducts rigorous, critical, adversarial code review passes on every PR diff until zero findings remain.
  - `Codebase Researcher`: Performs targeted symbol searches across `embedder.h` and `darwin/macos` without cluttering parent context.
  - `Android Engineer`: Implements C++ and JNI file edits and runs local unit test binaries.
  - `Test & Log Failure Parser`: Parses verbose compilation logs and test failures, summarizing only actionable errors back to the parent.

---

## 5. Master Piecewise PR Breakdown

```
Phase 0: Test Baseline Hardening
 └── PR 0.1: Multi-Threaded & Lifecycle Baseline Test Suite

Phase 1: Public Embedder C API Gaps (embedder.h)
 ├── PR 1.1: Custom Asset Resolvers (FlutterAssetResolver & FlutterMapping)
 ├── PR 1.2: Vulkan Impeller Render Target Backing Store (MakeRenderTargetFromBackingStoreImpeller)
 ├── PR 1.3: AHardwareBuffer & Vulkan External Texture Support
 ├── PR 1.4: Multi-Engine Spawning (FlutterEngineSpawn)
 ├── PR 1.5: Dart Deferred Library Loading Callbacks
 ├── PR 1.6: Raster Context Setup & Teardown Hooks (setup_callback)
 ├── PR 1.7: Semantics Completeness (FlutterSemanticsNode2 Attribute Parity)
 ├── PR 1.8: Embedder Screenshot / Raster Bitmap API (FlutterEngineScreenshot)
 ├── PR 1.9: Dart Callback Information Lookup API (FlutterEngineGetCallbackInformation)
 ├── PR 1.10: Platform View Extended Mutation Types (ClipPath & ClipRSE)
 └── PR 1.11: Platform Image Decoder / Generator Registration C API

Phase 2: Architectural Decoupling & Scaffolding
 ├── PR 2.1: Break PlatformViewAndroid Inheritance & Establish GN Quarantine Target
 ├── PR 2.2: Adapt APKAssetProvider to FlutterAssetResolver
 ├── PR 2.3: Standardize Thread Priority Setters on Custom Task Runners
 ├── PR 2.4: Runtime Feature Flag Switch (--enable-android-embedder-api) & Test Harness Overrides
 └── PR 2.5: Decouple flutter_main.cc using //flutter/shell/platform/common

Phase 3: Compositor, Platform Views & Surface Management
 ├── PR 3.1: AndroidSurfaceManager Backing Store Pool (Gated: flag=true & false)
 ├── PR 3.2: AndroidCompositor Layer Presentation & Synchronous Surface Detach Barrier (Gated)
 └── PR 3.3: Direct JNI Platform View Mutator Mapping & DPR Normalization (Gated)

Phase 4: Engine Instantiation & Dual-Path Switching
 ├── PR 4.1: Implement AndroidEngine Orchestrator (Gated)
 ├── PR 4.2: JNI Dispatch Dual-Path Routing (PlatformViewAndroidJNIImpl) (Gated)
 └── PR 4.3: Parameterized Multi-Backend Matrix (TEST_P) & Scenario Validation

Phase 5: Default Cutover & Dependency Pruning
 ├── PR 5.1: Enable Embedder API by Default with Rollback Switches
 └── PR 5.2: Delete Legacy Bridges, AndroidShellHolder & Purge BUILD.gn to Allowed Set
```

---

### Detailed PR Specifications

#### Phase 0: Baseline Verification & Test Hardening

##### PR 0.1: Multi-Threaded & Lifecycle Baseline Test Suite
- **Goal**: Harden existing test coverage for Android lifecycle, surface attach/detach, multi-thread message queues, and rendering backend selection before introducing architectural changes.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/android/android_shell_holder_unittests.cc`
  - `engine/src/flutter/shell/platform/android/platform_view_android_unittests.cc`
- **Key Modifications**:
  - Add parameterized unit tests for background thread message handlers (`BinaryMessenger.TaskQueue`).
  - Add surface recreate tests simulating rapid `onPause` -> `onResume` Android lifecycle events.
  - Verify Vulkan vs OpenGL vs Software fallback selection across API levels 21–35.
- **Verification Commands**:
  ```bash
  ninja -C out/android_debug_unopt_x64 flutter_shell_native_unittests
  ./out/android_debug_unopt_x64/flutter_shell_native_unittests --gtest_filter="AndroidShellHolder*:PlatformViewAndroid*"
  ```

---

#### Phase 1: Public Embedder C API Gaps (`embedder.h`)

##### PR 1.1: Custom Asset Resolvers (`FlutterAssetResolver`)
- **Goal**: Allow embedders to supply custom in-memory asset providers (such as `AAssetManager`) without requiring disk paths.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/embedder/embedder.h`
  - `engine/src/flutter/shell/platform/embedder/embedder.cc`
  - `engine/src/flutter/shell/platform/embedder/embedder_engine.h`, `.cc`
  - `engine/src/flutter/shell/platform/embedder/embedder_asset_resolver.h`, `.cc`
  - `engine/src/flutter/shell/platform/embedder/tests/embedder_unittests.cc`
- **Key Modifications**:
  - Define `FlutterMapping`, `FlutterAssetResolver`, and `FlutterAssetResolverGetAssetCallback` in `embedder.h`.
  - Add `FlutterProjectArgs.asset_resolvers` array and `asset_resolvers_count`.
  - Add `FlutterEngineUpdateAssetResolver` for runtime asset updates during hot reload/restart.
- **Verification Commands**:
  ```bash
  ninja -C out/host_debug_unopt embedder_unittests
  ./out/host_debug_unopt/embedder_unittests --gtest_filter="EmbedderTest.CustomAssetResolver*"
  ```

##### PR 1.2: Vulkan Impeller Render Target Backing Store
- **Goal**: Implement `MakeRenderTargetFromBackingStoreImpeller` for `kFlutterBackingStoreTypeVulkan` in `embedder.cc`.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/embedder/embedder.h`
  - `engine/src/flutter/shell/platform/embedder/embedder.cc`
  - `engine/src/flutter/shell/platform/embedder/tests/embedder_vk_unittests.cc`
- **Key Modifications**:
  - Implement Impeller Vulkan render target backing store construction in `embedder.cc`.
  - Wrap Vulkan images with `EmbedderWrappedTextureSourceVK`.
- **Verification Commands**:
  ```bash
  ninja -C out/host_debug_unopt embedder_vk_unittests
  ./out/host_debug_unopt/embedder_vk_unittests --gtest_filter="EmbedderVkTest.CanRenderWithImpellerVulkanCompositor*"
  ```

##### PR 1.3: AHardwareBuffer & Vulkan External Textures in Embedder API
- **Goal**: Add native `AHardwareBuffer` backing store and external texture support to `embedder.h`.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/embedder/embedder.h`
  - `engine/src/flutter/shell/platform/embedder/embedder_external_texture_resolver.h`, `.cc`
  - `engine/src/flutter/shell/platform/embedder/tests/embedder_vk_unittests.cc`
- **Key Modifications**:
  - Add `FlutterVulkanImageFrameCallback` and `external_texture_frame_callback` to `FlutterVulkanRendererConfig`.
  - Encapsulate `AHardwareBuffer` to `VkImage` / `EGLImage` importation inside `shell/platform/embedder/`.
- **Verification Commands**:
  ```bash
  ninja -C out/host_debug_unopt embedder_vk_unittests
  ./out/host_debug_unopt/embedder_vk_unittests --gtest_filter="*ExternalTexture*"
  ```

##### PR 1.4: Multi-Engine Spawning (`FlutterEngineSpawn`)
- **Goal**: Provide an ABI-stable C API for spawning lightweight engines sharing the Dart VM and Isolate Group.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/embedder/embedder.h`
  - `engine/src/flutter/shell/platform/embedder/embedder.cc`
  - `engine/src/flutter/shell/platform/embedder/embedder_engine.h`, `.cc`
  - `engine/src/flutter/shell/platform/embedder/tests/embedder_unittests.cc`
- **Key Modifications**:
  - Add `FlutterEngineSpawnInfo` struct with struct size versioning, `initial_route`, `entrypoint`, and `user_data`.
  - Add `FlutterEngineSpawn` mapping to `Shell::Spawn`.
- **Verification Commands**:
  ```bash
  ninja -C out/host_debug_unopt embedder_unittests
  ./out/host_debug_unopt/embedder_unittests --gtest_filter="EmbedderTest.CanSpawnEngine*"
  ```

##### PR 1.5: Dart Deferred Library Loading Callbacks
- **Goal**: Support dynamic split component/deferred library loading in `embedder.h`.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/embedder/embedder.h`
  - `engine/src/flutter/shell/platform/embedder/embedder.cc`
  - `engine/src/flutter/shell/platform/embedder/platform_view_embedder.h`, `.cc`
  - `engine/src/flutter/shell/platform/embedder/tests/embedder_unittests.cc`
- **Key Modifications**:
  - Add `FlutterRequestDartDeferredLibraryCallback` to `FlutterProjectArgs`.
  - Add `FlutterEngineLoadDartDeferredLibrary` and `FlutterEngineLoadDartDeferredLibraryError`.
- **Verification Commands**:
  ```bash
  ninja -C out/host_debug_unopt embedder_unittests
  ./out/host_debug_unopt/embedder_unittests --gtest_filter="EmbedderTest.DartDeferredLoading*"
  ```

##### PR 1.6: Raster Context Setup & Teardown Hooks
- **Goal**: Enable lazy raster thread context initialization and backend selection.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/embedder/embedder.h`
  - `engine/src/flutter/shell/platform/embedder/embedder_surface_gl_impeller.cc`
  - `engine/src/flutter/shell/platform/embedder/embedder_surface_vulkan_impeller.cc`
  - `engine/src/flutter/shell/platform/embedder/tests/embedder_gl_unittests.cc`
- **Key Modifications**:
  - Add `setup_callback` (`FlutterRasterContextSetupCallback`) to `FlutterOpenGLRendererConfig` and `FlutterVulkanRendererConfig`.
- **Verification Commands**:
  ```bash
  ninja -C out/host_debug_unopt embedder_gl_unittests
  ./out/host_debug_unopt/embedder_gl_unittests --gtest_filter="EmbedderGLTest.SetupCallback*"
  ```

##### PR 1.7: Semantics Completeness (`FlutterSemanticsNode2`)
- **Goal**: Guarantee 100% field parity between `FlutterSemanticsNode2` and Android `AccessibilityBridge.java`.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/embedder/embedder.h`
  - `engine/src/flutter/shell/platform/embedder/embedder_semantics_update.cc`
  - `engine/src/flutter/shell/platform/embedder/tests/embedder_a11y_unittests.cc`
- **Key Modifications**:
  - Add missing fields: `max_value_length`, `current_value_length`, `traversal_parent`, `role`, `validation_result`, `link_url`, `locale`, `min_value`, `max_value`, `hit_test_transform`.
- **Verification Commands**:
  ```bash
  ninja -C out/host_debug_unopt embedder_a11y_unittests
  ./out/host_debug_unopt/embedder_a11y_unittests
  ```

##### PR 1.8: Embedder Screenshot / Raster Bitmap API
- **Goal**: Add `FlutterEngineScreenshot` / `FlutterEngineGetBitmap` to `embedder.h` to service `FlutterView.getBitmap()`.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/embedder/embedder.h`
  - `engine/src/flutter/shell/platform/embedder/embedder.cc`
  - `engine/src/flutter/shell/platform/embedder/embedder_engine.h`, `.cc`
  - `engine/src/flutter/shell/platform/embedder/tests/embedder_unittests.cc`
- **Key Modifications**:
  - Define `FlutterEngineScreenshotCallback` and `FlutterEngineScreenshot` in `embedder.h`.
  - Wire callback to engine rasterizer or surface readback.
- **Verification Commands**:
  ```bash
  ninja -C out/host_debug_unopt embedder_unittests
  ./out/host_debug_unopt/embedder_unittests --gtest_filter="EmbedderTest.Screenshot*"
  ```

##### PR 1.9: Dart Callback Information Lookup API
- **Goal**: Expose Dart callback information lookup in `embedder.h` for `FlutterCallbackInformation`.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/embedder/embedder.h`
  - `engine/src/flutter/shell/platform/embedder/embedder.cc`
  - `engine/src/flutter/shell/platform/embedder/tests/embedder_unittests.cc`
- **Key Modifications**:
  - Add `FlutterEngineGetCallbackInformation(int64_t handle, FlutterCallbackInformation* info_out)`.
- **Verification Commands**:
  ```bash
  ninja -C out/host_debug_unopt embedder_unittests
  ./out/host_debug_unopt/embedder_unittests --gtest_filter="EmbedderTest.CallbackInfo*"
  ```

##### PR 1.10: Platform View Extended Mutation Types (`ClipPath`, `ClipRSE`)
- **Goal**: Add `kFlutterPlatformViewMutationTypeClipPath` and `kFlutterPlatformViewMutationTypeClipRoundedSuperellipse` to `embedder.h`.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/embedder/embedder.h`
  - `engine/src/flutter/shell/platform/embedder/embedder.cc`
  - `engine/src/flutter/shell/platform/embedder/tests/embedder_unittests.cc`
- **Key Modifications**:
  - Extend `FlutterPlatformViewMutation` with path verbs/points and superellipse corner radii.
- **Verification Commands**:
  ```bash
  ninja -C out/host_debug_unopt embedder_unittests
  ./out/host_debug_unopt/embedder_unittests --gtest_filter="EmbedderTest.PlatformViewMutations*"
  ```

##### PR 1.11: Platform Image Decoder / Generator Registration C API
- **Goal**: Expose public C API for registering platform image decoders (Android ImageDecoder API 28+).
- **Modified Files**:
  - `engine/src/flutter/shell/platform/embedder/embedder.h`
  - `engine/src/flutter/shell/platform/embedder/embedder.cc`
  - `engine/src/flutter/shell/platform/embedder/tests/embedder_unittests.cc`
- **Key Modifications**:
  - Add `FlutterEngineRegisterImageDecoder` and callback structs to `embedder.h`.
  - Move `:image_generator` implementation into `shell/platform/embedder/` or pure C callbacks.
- **Verification Commands**:
  ```bash
  ninja -C out/host_debug_unopt embedder_unittests
  ./out/host_debug_unopt/embedder_unittests --gtest_filter="EmbedderTest.ImageDecoder*"
  ```

---

#### Phase 2: Architectural Decoupling & Scaffolding

##### PR 2.1: Break `PlatformViewAndroid` Inheritance via Delegate & Establish GN Legacy Quarantine Target
- **Goal**:
  1. Remove `PlatformViewAndroid`'s inheritance from `flutter::PlatformView` using a delegate interface (`PlatformViewAndroid::Delegate`).
  2. Create `source_set("android_legacy_engine_holder")` in `shell/platform/android/BUILD.gn` with strict visibility `[ ":flutter_shell_native_src", ":flutter_shell_native_unittests" ]` to isolate all internal engine dependencies.
  3. Prune `flutter_shell_native_src`'s `public_deps` to only allowed dependencies (`embedder_as_internal_library`, `fml`, `shell/platform/common`, `third_party`), linking `:android_legacy_engine_holder` as a private `deps`.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/android/BUILD.gn`
  - `engine/src/flutter/shell/platform/android/platform_view_android.h`, `.cc`
  - `engine/src/flutter/shell/platform/android/platform_view_android_adapter.h`, `.cc`
  - `engine/src/flutter/shell/platform/android/android_shell_holder.cc`
  - `engine/src/flutter/shell/platform/android/platform_view_android_unittests.cc`
- **Key Modifications**:
  - Split `BUILD.gn` native sources:
    ```gn
    source_set("android_legacy_engine_holder") {
      visibility = [
        ":flutter_shell_native_src",
        ":flutter_shell_native_unittests",
      ]
      sources = [
        "android_context_dynamic_impeller.cc",
        "android_context_dynamic_impeller.h",
        "android_context_gl_impeller.cc",
        "android_context_gl_impeller.h",
        "android_context_vk_impeller.cc",
        "android_context_vk_impeller.h",
        "android_shell_holder.cc",
        "android_shell_holder.h",
        "android_surface_dynamic_impeller.cc",
        "android_surface_dynamic_impeller.h",
        "android_surface_gl_impeller.cc",
        "android_surface_gl_impeller.h",
        "android_surface_vk_impeller.cc",
        "android_surface_vk_impeller.h",
        "platform_view_android_adapter.cc",
        "platform_view_android_adapter.h",
      ]
      if (!slimpeller) {
        sources += [
          "android_context_gl_skia.cc",
          "android_context_gl_skia.h",
          "android_surface_gl_skia.cc",
          "android_surface_gl_skia.h",
          "android_surface_software.cc",
          "android_surface_software.h",
        ]
      }
      deps = [
        "//flutter/assets",
        "//flutter/common",
        "//flutter/common/graphics",
        "//flutter/flow",
        "//flutter/impeller",
        "//flutter/impeller/toolkit/android",
        "//flutter/impeller/toolkit/egl",
        "//flutter/impeller/toolkit/gles",
        "//flutter/impeller/toolkit/glvk",
        "//flutter/lib/ui",
        "//flutter/runtime",
        "//flutter/runtime:libdart",
        "//flutter/shell/common",
        "//flutter/skia",
        "//flutter/txt",
        "//flutter/vulkan",
      ]
    }

    source_set("flutter_shell_native_src") {
      visibility = [ ":*" ]
      sources = [
        "apk_asset_provider.cc",
        "apk_asset_provider.h",
        "flutter_main.cc",
        "flutter_main.h",
        "library_loader.cc",
        "platform_message_handler_android.cc",
        "platform_message_handler_android.h",
        "platform_message_response_android.cc",
        "platform_message_response_android.h",
        "platform_view_android.cc",
        "platform_view_android.h",
        "platform_view_android_jni_impl.cc",
        "platform_view_android_jni_impl.h",
        "vsync_waiter_android.cc",
        "vsync_waiter_android.h",
      ]
      public_deps = [
        ":android_gpu_configuration",
        ":icudtl_asm",
        ":image_generator",
        "//flutter/fml",
        "//flutter/shell/platform/common",
        "//flutter/shell/platform/embedder:embedder_as_internal_library",
      ]
      deps = [
        ":android_legacy_engine_holder",
      ]
    }
    ```
  - Create `PlatformViewAndroid::Delegate` abstract interface declaring all engine interactions.
  - Implement `PlatformViewAndroidAdapter` as a concrete subclass of `flutter::PlatformView` implementing `PlatformViewAndroid::Delegate`.
  - Convert `PlatformViewAndroid` into a standalone class that holds a `Delegate*` pointer.
- **Verification Commands**:
  ```bash
  ninja -C out/android_debug_unopt_x64 flutter_shell_native_unittests
  ./out/android_debug_unopt_x64/flutter_shell_native_unittests --gtest_filter="PlatformViewAndroid*"
  ```

##### PR 2.2: Adapt `APKAssetProvider` to `FlutterAssetResolver`
- **Goal**: Adapt `APKAssetProvider` to populate `FlutterAssetResolverConfig` callbacks.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/android/apk_asset_provider.h`, `.cc`
  - `engine/src/flutter/shell/platform/android/apk_asset_provider_unittests.cc`
- **Key Modifications**:
  - Implement `GetAssetResolverConfig()` on `APKAssetProvider`.
- **Verification Commands**:
  ```bash
  ninja -C out/android_debug_unopt_x64 flutter_shell_native_unittests
  ./out/android_debug_unopt_x64/flutter_shell_native_unittests --gtest_filter="ApkAssetProvider*"
  ```

##### PR 2.3: Standardize Thread Priority Setters on Custom Task Runners
- **Goal**: Standardize Android thread priority setting using `FlutterCustomTaskRunners::thread_priority_setter`.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/android/android_thread_config.h`, `.cc`
  - `engine/src/flutter/shell/platform/android/android_shell_holder.cc`
  - `engine/src/flutter/shell/platform/android/android_thread_config_unittests.cc`
- **Key Modifications**:
  - Map `FlutterThreadPriority` (`kBackground`, `kDisplay`, `kRaster`) to Android `nice` levels (-1 UI, -5 Raster, 10 IO).
- **Verification Commands**:
  ```bash
  ninja -C out/android_debug_unopt_x64 flutter_shell_native_unittests
  ./out/android_debug_unopt_x64/flutter_shell_native_unittests --gtest_filter="AndroidThreadConfig*"
  ```

##### PR 2.4: Introduce Runtime Feature Flag Switch & Test Harness Overrides
- **Goal**: Introduce `--enable-android-embedder-api` engine switch, Java configuration flags, and test reset hooks.
- **Modified Files**:
  - `engine/src/flutter/common/settings.h`
  - `engine/src/flutter/shell/common/switch_defs.h`
  - `engine/src/flutter/shell/common/switches.cc`
  - `engine/src/flutter/shell/platform/android/flutter_main.h`, `.cc`
  - `engine/src/flutter/shell/platform/android/io/flutter/embedding/engine/FlutterEngineFlags.java`
- **Key Modifications**:
  - Define `DEF_SWITCH(EnableAndroidEmbedderAPI)`.
  - Add `FlutterMain::SetEmbedderAPIEnabledForTesting(std::optional<bool>)` for dynamic test switching.
- **Verification Commands**:
  ```bash
  ninja -C out/host_debug_unopt switches_unittests
  ./out/host_debug_unopt/switches_unittests --gtest_filter="SwitchesTest.EnableAndroidEmbedderAPI*"
  ```

##### PR 2.5: Decouple `flutter_main.cc` using `//flutter/shell/platform/common`
- **Goal**: Remove `flutter_main.cc`'s direct dependencies on `//flutter/shell/common` and `//flutter/common/settings.h`.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/android/flutter_main.h`, `.cc`
  - `engine/src/flutter/shell/platform/android/BUILD.gn`
- **Key Modifications**:
  - Use `//flutter/shell/platform/common:common_cpp_switches` and `common_cpp_core` for argument parsing.
- **Verification Commands**:
  ```bash
  ninja -C out/android_debug_unopt_x64 flutter_shell_native_unittests
  ```

---

#### Phase 3: Compositor, Platform Views & Surface Management

##### PR 3.1: Android Surface Backing Store Manager (`AndroidSurfaceManager`)
- **Goal**: Manage backing store allocation pools (`AHardwareBuffer`, `EGLSurface`, `VkImage`) for the layer compositor.
- **Gated**: Feature-flag gated. Must test with `flag = true` AND `flag = false`.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/android/android_surface_manager.h`, `.cc`
  - `engine/src/flutter/shell/platform/android/android_surface_manager_unittests.cc`
- **Key Modifications**:
  - Implement size-matched backing store caching (`FlutterBackBufferCache`) to prevent buffer allocation churn.
  - Implement dedicated context isolation to eliminate cross-thread `EGL_BAD_ACCESS` collisions.
- **Verification Commands**:
  ```bash
  ninja -C out/android_debug_unopt_x64 flutter_shell_native_unittests
  ./out/android_debug_unopt_x64/flutter_shell_native_unittests --gtest_filter="AndroidSurfaceManager*"
  ```

##### PR 3.2: Android Compositor Layer Presentation & Synchronous Surface Detach Barrier
- **Goal**: Implement `FlutterCompositor` layer presentation callbacks and synchronous surface destruction barrier.
- **Gated**: Feature-flag gated. Must test with `flag = true` AND `flag = false`.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/android/android_compositor.h`, `.cc`
  - `engine/src/flutter/shell/platform/android/android_compositor_unittests.cc`
- **Key Modifications**:
  - Implement `CreateBackingStore`, `CollectBackingStore`, and `PresentView` callbacks.
  - Implement synchronous surface detachment barrier using `fml::AutoResetWaitableEvent` when `surfaceDestroyed` occurs.
- **Verification Commands**:
  ```bash
  ninja -C out/android_debug_unopt_x64 flutter_shell_native_unittests
  ./out/android_debug_unopt_x64/flutter_shell_native_unittests --gtest_filter="AndroidCompositor*"
  ```

##### PR 3.3: Direct JNI Platform View Mutator Mapping & DPR Normalization
- **Goal**: Map `FlutterPlatformView` mutations directly to Java `FlutterMutatorsStack` without referencing `//flutter/flow`.
- **Gated**: Feature-flag gated. Must test with `flag = true` AND `flag = false`.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/android/android_compositor.cc`
  - `engine/src/flutter/shell/platform/android/android_compositor_unittests.cc`
- **Key Modifications**:
  - Map `FlutterPlatformViewMutation` directly to JNI calls on `FlutterMutatorsStack.java`.
  - Isolate root DPR transform matrix to guarantee unscaled logical coordinates in Java mutator stack.
- **Verification Commands**:
  ```bash
  ninja -C out/android_debug_unopt_x64 flutter_shell_native_unittests
  ./out/android_debug_unopt_x64/flutter_shell_native_unittests --gtest_filter="AndroidCompositor*Mutation*"
  ```

---

#### Phase 4: Engine Instantiation & Dual-Path Switching

##### PR 4.1: Implement `AndroidEngine` Orchestrator
- **Goal**: Implement the primary C++ engine backend that consumes the public Embedder API on Android.
- **Gated**: Feature-flag gated. Must test with `flag = true` AND `flag = false`.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/android/android_engine.h`, `.cc`
  - `engine/src/flutter/shell/platform/android/android_engine_unittests.cc`
- **Key Modifications**:
  - Create `AndroidEngine` owning `FLUTTER_API_SYMBOL(FlutterEngine)` and `FlutterEngineProcTable`.
  - Implement engine launch, Dart snapshot execution, semantics actions, window metrics updates, and pointer event dispatch via `_embedderAPI`.
- **Verification Commands**:
  ```bash
  ninja -C out/android_debug_unopt_x64 flutter_shell_native_unittests
  ./out/android_debug_unopt_x64/flutter_shell_native_unittests --gtest_filter="AndroidEngine*"
  ```

##### PR 4.2: JNI Dispatch Dual-Path Routing
- **Goal**: Wire native JNI entry points to dynamically route to either `AndroidShellHolder` (legacy) or `AndroidEngine` (new) based on `FlutterMain::IsEmbedderAPIEnabled()`.
- **Gated**: Feature-flag gated. Must test with `flag = true` AND `flag = false`.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/android/platform_view_android_jni_impl.cc`
  - `engine/src/flutter/shell/platform/android/platform_view_android_jni_impl_unittests.cc`
- **Key Modifications**:
  - Route `AttachJNI`, `DestroyJNI`, `SpawnJNI`, `RunBundleAndSnapshotFromLibrary`, `SetViewportMetrics`, `DispatchPointerDataPacket`, etc., dynamically.
- **Verification Commands**:
  ```bash
  ninja -C out/android_debug_unopt_x64 flutter_shell_native_unittests
  ./out/android_debug_unopt_x64/flutter_shell_native_unittests --gtest_filter="PlatformViewAndroidJNI*"
  ```

##### PR 4.3: Parameterized Multi-Backend Matrix (`TEST_P`) & Scenario Validation
- **Goal**: Add parameterized tests running all unit, scenario, and integration suites across both `flag = true` and `flag = false`.
- **Modified Files**:
  - `engine/src/flutter/shell/platform/android/platform_view_android_jni_impl_unittests.cc`
  - `engine/src/flutter/shell/platform/android/android_engine_unittests.cc`
- **Verification Commands**:
  - Run all parameterized unit tests:
    ```bash
    ./out/android_debug_unopt_x64/flutter_shell_native_unittests
    ```
  - Run scenario and integration test suites with `flag = false` and `flag = true`:
    - `dev/integration_tests/android_views`
    - `dev/integration_tests/channels`
    - `dev/integration_tests/external_textures`

---

#### Phase 5: Default Cutover & Dependency Pruning

##### PR 5.1: Enable Embedder API by Default with Rollback Switches
- **Goal**: Flip the default engine execution on Android to the public Embedder API while providing negative flags for canary rollback.
- **Modified Files**:
  - `engine/src/flutter/common/settings.h`
  - `engine/src/flutter/shell/common/switch_defs.h`
  - `engine/src/flutter/shell/common/switches.cc`
  - `engine/src/flutter/shell/platform/android/io/flutter/embedding/engine/FlutterEngineFlags.java`
- **Key Modifications**:
  - Set `enable_embedder_api = true` by default.
  - Support negative rollback switches (`--no-enable-android-embedder-api`, `--enable-android-embedder-api=false`).

##### PR 5.2: Legacy Bridge Deletion & Total Dependency Pruning in `BUILD.gn`
- **Goal**: Delete `android_legacy_engine_holder` target, `AndroidShellHolder`, `PlatformViewAndroidAdapter`, and all legacy surface/context files. Remove `:android_legacy_engine_holder` from `flutter_shell_native_src.deps` and completely prune all prohibited internal engine dependencies from `shell/platform/android/BUILD.gn`.
- **Deleted Targets & Files**:
  - `source_set("android_legacy_engine_holder")` in `engine/src/flutter/shell/platform/android/BUILD.gn`
  - `engine/src/flutter/shell/platform/android/android_shell_holder.h`, `.cc`
  - `engine/src/flutter/shell/platform/android/android_shell_holder_unittests.cc`
  - `engine/src/flutter/shell/platform/android/platform_view_android_adapter.h`, `.cc`
  - Legacy context and surface files (`android_surface_dynamic_impeller.*`, `android_surface_gl_impeller.*`, `android_surface_vk_impeller.*`, `android_context_dynamic_impeller.*`, `android_context_gl_impeller.*`, `android_context_vk_impeller.*`, `android_surface_software.*`, `android_context_gl_skia.*`, `android_surface_gl_skia.*`)
- **Modified Files**:
  - `engine/src/flutter/shell/platform/android/BUILD.gn`
- **Final `BUILD.gn` Target Rule**:
  ```gn
  source_set("flutter_shell_native_src") {
    visibility = [ ":*" ]

    sources = [
      "android_compositor.cc",
      "android_compositor.h",
      "android_display.cc",
      "android_display.h",
      "android_egl_surface.cc",
      "android_egl_surface.h",
      "android_engine.cc",
      "android_engine.h",
      "android_environment_gl.cc",
      "android_environment_gl.h",
      "android_render_target.cc",
      "android_render_target.h",
      "android_surface_manager.cc",
      "android_surface_manager.h",
      "android_thread_config.cc",
      "android_thread_config.h",
      "apk_asset_provider.cc",
      "apk_asset_provider.h",
      "flutter_main.cc",
      "flutter_main.h",
      "image_external_texture.cc",
      "image_external_texture.h",
      "image_external_texture_gl.cc",
      "image_external_texture_gl.h",
      "image_external_texture_gl_impeller.cc",
      "image_external_texture_gl_impeller.h",
      "image_external_texture_vk_impeller.cc",
      "image_external_texture_vk_impeller.h",
      "image_lru.cc",
      "image_lru.h",
      "library_loader.cc",
      "platform_message_handler_android.cc",
      "platform_message_handler_android.h",
      "platform_message_response_android.cc",
      "platform_message_response_android.h",
      "platform_view_android.cc",
      "platform_view_android.h",
      "platform_view_android_jni_impl.cc",
      "platform_view_android_jni_impl.h",
      "surface_texture_external_texture.cc",
      "surface_texture_external_texture.h",
      "surface_texture_external_texture_gl_impeller.cc",
      "surface_texture_external_texture_gl_impeller.h",
      "surface_texture_external_texture_vk_impeller.cc",
      "surface_texture_external_texture_vk_impeller.h",
      "vsync_waiter_android.cc",
      "vsync_waiter_android.h",
    ]

    sources += get_target_outputs(":icudtl_asm")

    # Allowed dependencies ONLY:
    public_deps = [
      ":android_gpu_configuration",
      ":icudtl_asm",
      "//flutter/fml",
      "//flutter/shell/platform/common",
      "//flutter/shell/platform/embedder:embedder_as_internal_library",
    ]

    # android_legacy_engine_holder DELETED completely.
    deps = []

    public_configs = [ "//flutter:config" ]

    libs = [
      "android",
      "EGL",
      "GLESv2",
    ]
  }
  ```
- **Verification Commands**:
  - Full clean build of `host_debug_unopt` and `android_debug_unopt_x64`.
  - Verify that no internal engine headers (`shell.h`, `thread_host.h`, `dart_vm.h`, `mutators_stack.h`, etc.) are included in `shell/platform/android/`.
  - All unit, integration, and scenario tests pass 100%.

---

## 6. Verification & Invariant Matrix

| Test Layer | Test Suite Target | Success Criteria |
| :--- | :--- | :--- |
| **C++ Unit Tests** | `flutter_shell_native_unittests` (`TEST_P`)<br>`embedder_unittests`<br>`embedder_gl_unittests`<br>`embedder_vk_unittests` | 100% pass rate under BOTH `flag=true` and `flag=false`; clean ASAN/TSAN memory leak audits. |
| **Java Unit Tests** | `io.flutter.embedding.engine.*` Robolectric tests | JNI bindings and Java listeners operate identically across both flag states. |
| **Integration Scenarios** | `dev/integration_tests/android_views`<br>`dev/integration_tests/channels`<br>`dev/integration_tests/external_textures` | Platform views, touch events, text input, and textures render seamlessly under both flag states. |
| **Performance Benchmarks** | Devicelab `flutter_gallery__transition_perf`<br>`android_driver_test` | Zero FPS drops; identical startup latency and memory footprint. |
| **Golden Invariants** | Framework Golden Tests | Pixel-identical rendering with pre-migration goldens. |
| **Local Engine Validation** | `flutter run --local-engine` test app | Sample app runs, hot-restarts, and animates seamlessly. |

---

## 7. Migration State Ledger Initial Template (`MIGRATION_LEDGER.md`)

```markdown
# Flutter Android Embedder API Migration — State Ledger

## Current Status
- **Active Phase**: Phase 0.1 (Baseline Verification)
- **Active Branch**: `android-embedder-migration/phase-0.1-baseline-tests`
- **Base Commit SHA**: `08d2a471fd10152bb7dbfbb9636cf8fa53e94b8b`
- **Completed PRs**: []
- **Adversarial Review Loop Status**: Pending Phase 0.1 review
- **Target Dependency Boundary**:
  - Required: `//flutter/shell/platform/embedder:embedder_as_internal_library`
  - Optional: `//flutter/fml`, `//flutter/shell/platform/common`, `//flutter/third_party`
  - Prohibited: `//flutter/assets`, `common`, `flow`, `impeller`, `lib/ui`, `runtime`, `shell/common`, `skia`, `txt`, `vulkan`

## Quick PR Index & Checkpoints
- [ ] Phase 0.1: Baseline & Thread-Safety Tests
- [ ] Phase 1.1: Custom Asset Resolvers (`FlutterAssetResolver`)
- [ ] Phase 1.2: Vulkan Impeller Render Target Backing Store
- [ ] Phase 1.3: AHardwareBuffer & Vulkan External Textures
- [ ] Phase 1.4: Multi-Engine Spawning (`FlutterEngineSpawn`)
- [ ] Phase 1.5: Dart Deferred Library Loading
- [ ] Phase 1.6: Raster Context Setup Hooks (`setup_callback`)
- [ ] Phase 1.7: Extended Semantics Completeness (`FlutterSemanticsNode2`)
- [ ] Phase 1.8: Embedder Screenshot / Raster Bitmap API (`FlutterEngineScreenshot`)
- [ ] Phase 1.9: Dart Callback Information Lookup API (`FlutterEngineGetCallbackInformation`)
- [ ] Phase 1.10: Platform View Extended Mutation Types (`ClipPath`, `ClipRSE`)
- [ ] Phase 1.11: Platform Image Decoder / Generator Registration C API
- [ ] Phase 2.1: Break PlatformViewAndroid Inheritance & Establish GN Quarantine Target
- [ ] Phase 2.2: Adapt APKAssetProvider
- [ ] Phase 2.3: Custom Task Runners & Thread Priorities
- [ ] Phase 2.4: Runtime Feature Flag Switch & Test Harness Overrides
- [ ] Phase 2.5: Decouple `flutter_main.cc` using `//flutter/shell/platform/common`
- [ ] Phase 3.1: AndroidSurfaceManager Backing Store Pool (Gated: flag=true & false)
- [ ] Phase 3.2: AndroidCompositor Layer Presentation & Surface Detach Barrier (Gated: flag=true & false)
- [ ] Phase 3.3: Direct JNI Platform View Mutator Mapping & DPR Normalization (Gated: flag=true & false)
- [ ] Phase 4.1: AndroidEngine Implementation (Gated: flag=true & false)
- [ ] Phase 4.2: JNI Dispatch Dual-Path Routing (Gated: flag=true & false)
- [ ] Phase 4.3: Parameterized Multi-Backend Matrix (`TEST_P`) & Scenario Validation
- [ ] Phase 5.1: Enable Embedder API by Default (with negative rollback flags)
- [ ] Phase 5.2: Legacy Bridge Removal & Total BUILD.gn Dependency Pruning

## Quality & Review Invariants
1. **Rendering Feature Flag Gating**: Any PR touching rendering logic must be feature-flag gated and tested under BOTH states (`flag=true` AND `flag=false`).
2. **Adversarial Review Loop**: Every PR must undergo iterative review by an independent agent (`reidbaker-agent`) until 0 findings remain.
3. **Synchronous Surface Detach Barrier**: Block on rasterizer during `nativeSurfaceDestroyed` before OS destroys `ANativeWindow`.
4. **EGL_BAD_ACCESS Defense**: Ensure IO thread has isolated offscreen PBuffer context.
5. **Surface Lifecycle Defense**: Handle early `make_current` calls gracefully before `nativeSurfaceCreated`.
6. **JIT Assets Defense**: Route `kernel_blob.bin` queries through `FlutterAssetResolver` instead of file paths.
7. **GN Target Isolation & Visibility Ratchet**: Quarantine all legacy engine dependencies into `android_legacy_engine_holder` with strict visibility, keeping `flutter_shell_native_src` clean from Day 1.
```
