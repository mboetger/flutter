# Flutter Android Embedder API Migration — Branch Index & Comparison Matrix

This document provides a comprehensive index of all 25 branches created during the Flutter Android Embedder API Decoupling & Migration project.

For each branch, this document lists:
1. **Branch Name**
2. **GitHub Compare URL** (comparing the branch with its predecessor, or with base commit `0c041cb5252307a0f57d55dcb270c802197d14ff` for Phase 0.1 to avoid master drift)
3. **Summary of Changes in the Branch**

---

## 1. Phase 0.1: Baseline & Thread-Safety Tests
* **Branch Name**: [`android-migration-2/phase-0.1-baseline-tests`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-0.1-baseline-tests)
* **Compare URL**: [0c041cb525...phase-0.1-baseline-tests](https://github.com/mboetger/flutter/compare/0c041cb5252307a0f57d55dcb270c802197d14ff...android-migration-2/phase-0.1-baseline-tests)
* **Changes in Branch**:
  * Added baseline unit tests in `shell/platform/android/test/io/flutter/embedding/engine/` establishing behavioral parity before migration.
  * Added multithreaded test coverage for `PlatformMessageHandlerAndroid` message dispatch under concurrent isolate and UI load.
  * Added lifecycle and rendering backend baseline unit tests for OpenGL, Vulkan, and Software surfaces.

---

## 2. Phase 1.1: Custom Asset Resolvers (`FlutterAssetResolver`)
* **Branch Name**: [`android-migration-2/phase-1.1-custom-asset-resolvers`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-1.1-custom-asset-resolvers)
* **Compare URL**: [phase-0.1-baseline-tests...phase-1.1-custom-asset-resolvers](https://github.com/mboetger/flutter/compare/android-migration-2/phase-0.1-baseline-tests...android-migration-2/phase-1.1-custom-asset-resolvers)
* **Changes in Branch**:
  * Introduced `FlutterAssetResolver` and `FlutterMapping` in `embedder.h` to allow embedders to provide in-memory and custom asset resolution.
  * Added `EmbedderAssetResolver` adapter implementation bridging embedder asset callbacks to Flutter core asset providers.
  * Added unit test coverage for asset resolution, memory mapping, and lifetime cleanup.

---

## 3. Phase 1.2: Vulkan Impeller Render Target Backing Store
* **Branch Name**: [`android-migration-2/phase-1.2-vulkan-impeller-backing-store`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-1.2-vulkan-impeller-backing-store)
* **Compare URL**: [phase-1.1-custom-asset-resolvers...phase-1.2-vulkan-impeller-backing-store](https://github.com/mboetger/flutter/compare/android-migration-2/phase-1.1-custom-asset-resolvers...android-migration-2/phase-1.2-vulkan-impeller-backing-store)
* **Changes in Branch**:
  * Extended `FlutterBackingStore` and `FlutterBackingStoreConfig` with Vulkan Impeller render target support.
  * Implemented backing store creation, caching, and presentation callbacks for Vulkan-backed rendering.
  * Added embedder unit tests validating Vulkan backing store allocation and presentation.

---

## 4. Phase 1.3: AHardwareBuffer & Vulkan External Textures
* **Branch Name**: [`android-migration-2/phase-1.3-ahb-vulkan-external-textures`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-1.3-ahb-vulkan-external-textures)
* **Compare URL**: [phase-1.2-vulkan-impeller-backing-store...phase-1.3-ahb-vulkan-external-textures](https://github.com/mboetger/flutter/compare/android-migration-2/phase-1.2-vulkan-impeller-backing-store...android-migration-2/phase-1.3-ahb-vulkan-external-textures)
* **Changes in Branch**:
  * Added Vulkan external texture support (`EmbedderExternalTextureVK`) in `shell/platform/embedder/` supporting both Skia and Impeller backends.
  * Added `FlutterVulkanImageCallback` and `FlutterVulkanImageFrameCallback` to `FlutterVulkanRendererConfig`.
  * Implemented texture lifecycle and frame notification plumbing for hardware buffers and external textures.

---

## 5. Phase 1.4: Multi-Engine Spawning (`FlutterEngineSpawn`)
* **Branch Name**: [`android-migration-2/phase-1.4-engine-spawning`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-1.4-engine-spawning)
* **Compare URL**: [phase-1.3-ahb-vulkan-external-textures...phase-1.4-engine-spawning](https://github.com/mboetger/flutter/compare/android-migration-2/phase-1.3-ahb-vulkan-external-textures...android-migration-2/phase-1.4-engine-spawning)
* **Changes in Branch**:
  * Implemented `FlutterEngineSpawn` in `embedder.h` and `embedder.cc` with `FlutterEngineSpawnInfo` struct.
  * Supported isolated isolate groups sharing the existing Dart VM instance for lightweight multi-engine scenarios.
  * Added unit test suite `EmbedderTest.CanSpawnEngine` and proc table validation.

---

## 6. Phase 1.5: Dart Deferred Library Loading
* **Branch Name**: [`android-migration-2/phase-1.5-deferred-components`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-1.5-deferred-components)
* **Compare URL**: [phase-1.4-engine-spawning...phase-1.5-deferred-components](https://github.com/mboetger/flutter/compare/android-migration-2/phase-1.4-engine-spawning...android-migration-2/phase-1.5-deferred-components)
* **Changes in Branch**:
  * Added `FlutterEngineLoadDartDeferredLibrary` and `FlutterEngineLoadDartDeferredLibraryError` to `embedder.h` and `embedder.cc` utilizing `FlutterDeferredLibraryInfo` and `FlutterDeferredLibraryErrorInfo` struct pointers for forward ABI stability and extensibility.
  * Added `FlutterRequestDartDeferredLibraryCallback` to `FlutterProjectArgs` for dynamic module downloading.
  * Added unit test coverage verifying loading unit dispatch, struct validation (`struct_size` mismatch checks), payload delivery, and error callbacks.

---

## 7. Phase 1.6: Raster Context Setup & Teardown Hooks
* **Branch Name**: [`android-migration-2/phase-1.6-raster-context-setup`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-1.6-raster-context-setup)
* **Compare URL**: [phase-1.5-deferred-components...phase-1.6-raster-context-setup](https://github.com/mboetger/flutter/compare/android-migration-2/phase-1.5-deferred-components...android-migration-2/phase-1.6-raster-context-setup)
* **Changes in Branch**:
  * Added `setup_callback` and `teardown_callback` to `FlutterOpenGLRendererConfig` for raster thread initialization and destruction.
  * Invoked callbacks during raster thread context setup and teardown.
  * Added unit tests verifying raster thread context initialization and teardown sequence.

---

## 8. Phase 1.7: Extended Semantics Completeness (`FlutterSemanticsNode2`)
* **Branch Name**: [`android-migration-2/phase-1.7-semantics-node2`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-1.7-semantics-node2)
* **Compare URL**: [phase-1.6-raster-context-setup...phase-1.7-semantics-node2](https://github.com/mboetger/flutter/compare/android-migration-2/phase-1.6-raster-context-setup...android-migration-2/phase-1.7-semantics-node2)
* **Changes in Branch**:
  * Introduced `FlutterSemanticsNode2` and `FlutterSemanticsUpdate2` with full parity for modern accessibility features (attributed strings, string attributes, locale, tooltip, identifier, heading level, and hit test transform matrices).
  * Added `FlutterEngineUpdateSemantics2` in `embedder.h` and `embedder.cc`.
  * Added exhaustive unit tests in `embedder_a11y_unittests.cc`.

---

## 9. Phase 1.8: Embedder Screenshot / Raster Bitmap API (`FlutterEngineScreenshot`)
* **Branch Name**: [`android-migration-2/phase-1.8-screenshot-api`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-1.8-screenshot-api)
* **Compare URL**: [phase-1.7-semantics-node2...phase-1.8-screenshot-api](https://github.com/mboetger/flutter/compare/android-migration-2/phase-1.7-semantics-node2...android-migration-2/phase-1.8-screenshot-api)
* **Changes in Branch**:
  * Added `FlutterEngineScreenshot` C API to capture raster bitmaps directly from embedder surfaces.
  * Supported both raw pixel buffer data and base64-encoded PNG output formats.
  * Added multi-backend unit tests verifying raster bitmap capture and proc table export.

---

## 10. Phase 1.9: Dart Callback Information Lookup API (`FlutterEngineGetCallbackInformation`)
* **Branch Name**: [`android-migration-2/phase-1.9-callback-information`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-1.9-callback-information)
* **Compare URL**: [phase-1.8-screenshot-api...phase-1.9-callback-information](https://github.com/mboetger/flutter/compare/android-migration-2/phase-1.8-screenshot-api...android-migration-2/phase-1.9-callback-information)
* **Changes in Branch**:
  * Added `FlutterEngineGetCallbackInformation` to `embedder.h` and `embedder.cc` querying `DartCallbackCache`.
  * Exposed `FlutterCallbackInformation` struct providing callback name, class name, and library path.
  * Added unit test suite covering top-level functions, class methods, and proc table access.

---

## 11. Phase 1.10: Platform View Extended Mutation Types (`ClipPath`, `ClipRSE`)
* **Branch Name**: [`android-migration-2/phase-1.10-platform-view-mutations`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-1.10-platform-view-mutations)
* **Compare URL**: [phase-1.9-callback-information...phase-1.10-platform-view-mutations](https://github.com/mboetger/flutter/compare/android-migration-2/phase-1.9-callback-information...android-migration-2/phase-1.10-platform-view-mutations)
* **Changes in Branch**:
  * Added `kFlutterPlatformViewMutationTypeClipPath` and `kFlutterPlatformViewMutationTypeClipRoundedSuperellipse` to `FlutterPlatformViewMutationType`.
  * Defined `FlutterPath` and `FlutterRoundedSuperellipse` C structs.
  * Added translation of `DlPath` and `DlRoundSuperellipse` in `EmbedderLayers` and multi-backend unit tests in GL and Metal test suites.

---

## 12. Phase 1.11: Platform Image Decoder / Generator Registration C API
* **Branch Name**: [`android-migration-2/phase-1.11-image-generator-api`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-1.11-image-generator-api)
* **Compare URL**: [phase-1.10-platform-view-mutations...phase-1.11-image-generator-api](https://github.com/mboetger/flutter/compare/android-migration-2/phase-1.10-platform-view-mutations...android-migration-2/phase-1.11-image-generator-api)
* **Changes in Branch**:
  * Added `FlutterEngineRegisterImageGenerator` and `FlutterEngineUnregisterImageGenerator` to `embedder.h` and `embedder.cc`.
  * Implemented `EmbedderImageGenerator` bridging embedder decoders into the image decoding pipeline.
  * Added unit test suite verifying image generator registration, invocation, and teardown.

---

## 13. Phase 2.1: Break PlatformViewAndroid Inheritance & Establish GN Quarantine Target
* **Branch Name**: [`android-migration-2/phase-2.1-quarantine-gn`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-2.1-quarantine-gn)
* **Compare URL**: [phase-1.11-image-generator-api...phase-2.1-quarantine-gn](https://github.com/mboetger/flutter/compare/android-migration-2/phase-1.11-image-generator-api...android-migration-2/phase-2.1-quarantine-gn)
* **Changes in Branch**:
  * Split legacy `PlatformViewAndroid` implementation into `android_legacy_engine_holder` GN target with strict visibility.
  * Created clean `flutter_shell_native_src` target strictly depending on `//flutter/shell/platform/embedder:embedder_as_internal_library`.
  * Established the architecture boundary to prevent new dependencies on internal engine subsystems.

---

## 14. Phase 2.2: Adapt APKAssetProvider to FlutterAssetResolver
* **Branch Name**: [`android-migration-2/phase-2.2-apk-asset-provider`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-2.2-apk-asset-provider)
* **Compare URL**: [phase-2.1-quarantine-gn...phase-2.2-apk-asset-provider](https://github.com/mboetger/flutter/compare/android-migration-2/phase-2.1-quarantine-gn...android-migration-2/phase-2.2-apk-asset-provider)
* **Changes in Branch**:
  * Refactored `APKAssetProvider` to implement `FlutterAssetResolver` interface via `AAssetManager`.
  * Removed internal `fml::Mapping` dependencies from Android asset loading.
  * Added unit test coverage for asset resolution and bundle loading.

---

## 15. Phase 2.3: Custom Task Runners & Thread Priorities
* **Branch Name**: [`android-migration-2/phase-2.3-custom-task-runners`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-2.3-custom-task-runners)
* **Compare URL**: [phase-2.2-apk-asset-provider...phase-2.3-custom-task-runners](https://github.com/mboetger/flutter/compare/android-migration-2/phase-2.2-apk-asset-provider...android-migration-2/phase-2.3-custom-task-runners)
* **Changes in Branch**:
  * Configured `FlutterTaskRunnerDescription` custom task runner descriptors for UI, Platform, and Raster threads.
  * Implemented Android Linux thread priority setters (`setpriority(PRIO_PROCESS, ...)`).
  * Added unit test coverage for task dispatch and thread identification.

---

## 16. Phase 2.4: Runtime Feature Flag Switch & Test Harness Overrides
* **Branch Name**: [`android-migration-2/phase-2.4-feature-flag`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-2.4-feature-flag)
* **Compare URL**: [phase-2.3-custom-task-runners...phase-2.4-feature-flag](https://github.com/mboetger/flutter/compare/android-migration-2/phase-2.3-custom-task-runners...android-migration-2/phase-2.4-feature-flag)
* **Changes in Branch**:
  * Added `ENABLE_ANDROID_EMBEDDER_API` feature flag support to `FlutterMain` with CLI switch `--enable-android-embedder-api` and Dart define parsing.
  * Added thread-safe test harness override API `FlutterMain::SetEmbedderAPIEnabledForTest()`.
  * Added unit tests validating flag parsing and test harness overrides.

---

## 17. Phase 2.5: Decouple `flutter_main.cc` using `//flutter/shell/platform/common`
* **Branch Name**: [`android-migration-2/phase-2.5-decouple-flutter-main`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-2.5-decouple-flutter-main)
* **Compare URL**: [phase-2.4-feature-flag...phase-2.5-decouple-flutter-main](https://github.com/mboetger/flutter/compare/android-migration-2/phase-2.4-feature-flag...android-migration-2/phase-2.5-decouple-flutter-main)
* **Changes in Branch**:
  * Replaced `flutter::Settings` and internal engine header usage in `flutter_main.cc` with `//flutter/shell/platform/common` command-line switch decoders.
  * Wired VM Service URI logging callback to Logcat for DevTools and Flutter Driver auto-discovery.
  * Decoupled Flutter loader initialization from legacy shell dependencies.

---

## 18. Phase 3.1: AndroidSurfaceManager Backing Store Pool
* **Branch Name**: [`android-migration-2/phase-3.1-surface-manager`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-3.1-surface-manager)
* **Compare URL**: [phase-2.5-decouple-flutter-main...phase-3.1-surface-manager](https://github.com/mboetger/flutter/compare/android-migration-2/phase-2.5-decouple-flutter-main...android-migration-2/phase-3.1-surface-manager)
* **Changes in Branch**:
  * Implemented `AndroidSurfaceManager` with EGL display, context, window surface, and PBuffer initialization.
  * Added thread-local isolated EGL resource contexts (`MakeResourceCurrent()`) preventing `EGL_BAD_ACCESS` (0x3002) collisions.
  * Implemented cached backing store allocation and recycling pool (`CreateBackingStore` / `CollectBackingStore`).

---

## 19. Phase 3.2: AndroidCompositor Layer Presentation & Surface Detach Barrier
* **Branch Name**: [`android-migration-2/phase-3.2-compositor`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-3.2-compositor)
* **Compare URL**: [phase-3.1-surface-manager...phase-3.2-compositor](https://github.com/mboetger/flutter/compare/android-migration-2/phase-3.1-surface-manager...android-migration-2/phase-3.2-compositor)
* **Changes in Branch**:
  * Implemented `AndroidCompositor` managing `FlutterCompositor` layer composition and frame presentation.
  * Implemented synchronous surface detach barrier in `OnSurfaceDestroyed()` waiting on rasterizer task runner before `ANativeWindow` destruction.
  * Handled backing store present info and frame damage.

---

## 20. Phase 3.3: Direct JNI Platform View Mutator Mapping & DPR Normalization
* **Branch Name**: [`android-migration-2/phase-3.3-platform-views`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-3.3-platform-views)
* **Compare URL**: [phase-3.2-compositor...phase-3.3-platform-views](https://github.com/mboetger/flutter/compare/android-migration-2/phase-3.2-compositor...android-migration-2/phase-3.3-platform-views)
* **Changes in Branch**:
  * Implemented `AndroidCompositor::PopulateMutatorsStack` directly translating `FlutterPlatformView` mutations into JNI mutators.
  * Added Device Pixel Ratio (DPR) coordinate and transform normalization.
  * Added full support for `ClipRect`, `ClipRRect`, `ClipRSuperellipse`, `ClipPath`, `Opacity`, and `Transform` mutations.

---

## 21. Phase 4.1: AndroidEngine Implementation
* **Branch Name**: [`android-migration-2/phase-4.1-android-engine`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-4.1-android-engine)
* **Compare URL**: [phase-3.3-platform-views...phase-4.1-android-engine](https://github.com/mboetger/flutter/compare/android-migration-2/phase-3.3-platform-views...android-migration-2/phase-4.1-android-engine)
* **Changes in Branch**:
  * Implemented `AndroidEngine` orchestrator wrapping the C Embedder API (`FlutterEngineRun`, `FlutterEngineSendWindowMetricsEvent`, `FlutterEngineSendPointerEvent`, `FlutterEngineSendPlatformMessage`, `FlutterEngineSpawn`, etc.).
  * Added fallback routing for Impeller Vulkan to OpenGL renderer configuration with `AndroidSurfaceManager`.
  * Added automatic frame scheduling on surface creation to satisfy Android 14+ `SurfaceSyncGroup` transactions.

---

## 22. Phase 4.2: JNI Dispatch Dual-Path Routing
* **Branch Name**: [`android-migration-2/phase-4.2-jni-dispatch`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-4.2-jni-dispatch)
* **Compare URL**: [phase-4.1-android-engine...phase-4.2-jni-dispatch](https://github.com/mboetger/flutter/compare/android-migration-2/phase-4.1-android-engine...android-migration-2/phase-4.2-jni-dispatch)
* **Changes in Branch**:
  * Updated `platform_view_android_jni_impl.cc` to route all JNI calls dynamically based on `FlutterMain::IsEmbedderAPIEnabled()`.
  * Guaranteed seamless dual-path execution where `flag=false` uses `AndroidShellHolder` and `flag=true` uses `AndroidEngine`.
  * Added UI thread dispatch for `onFirstFrame` and `onPreEngineRestart` callbacks via `platform_task_runner_`.

---

## 23. Phase 4.3: Parameterized Multi-Backend Matrix (`TEST_P`) & Scenario Validation
* **Branch Name**: [`android-migration-2/phase-4.3-matrix-tests`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-4.3-matrix-tests)
* **Compare URL**: [phase-4.2-jni-dispatch...phase-4.3-matrix-tests](https://github.com/mboetger/flutter/compare/android-migration-2/phase-4.2-jni-dispatch...android-migration-2/phase-4.3-matrix-tests)
* **Changes in Branch**:
  * Added parameterized multi-backend unit test suite (`TEST_P(AndroidEngineMatrixTest, ...)`) testing all scenarios under both `flag=true` and `flag=false`.
  * Tested engine launching, window metrics, platform messages, touch input, semantics, and multi-engine spawning under both configurations.
  * Verified backward compatibility and embedder execution on physical device.

---

## 24. Phase 5.1: Enable Embedder API by Default (with Negative Rollback Flags)
* **Branch Name**: [`android-migration-2/phase-5.1-enable-default`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-5.1-enable-default)
* **Compare URL**: [phase-4.3-matrix-tests...phase-5.1-enable-default](https://github.com/mboetger/flutter/compare/android-migration-2/phase-4.3-matrix-tests...android-migration-2/phase-5.1-enable-default)
* **Changes in Branch**:
  * Switched `FlutterMain::IsEmbedderAPIEnabled()` default return value from `false` to `true`.
  * Added negative rollback switch `--no-enable-android-embedder-api` and `--dart-define=ENABLE_ANDROID_EMBEDDER_API=false`.
  * Verified integration and golden tests on physical device under both default and rollback states.

---

## 25. Phase 5.2: Legacy Bridge Removal & Total BUILD.gn Dependency Pruning
* **Branch Name**: [`android-migration-2/phase-5.2-legacy-pruning`](https://github.com/mboetger/flutter/tree/android-migration-2/phase-5.2-legacy-pruning)
* **Compare URL**: [phase-5.1-enable-default...phase-5.2-legacy-pruning](https://github.com/mboetger/flutter/compare/android-migration-2/phase-5.1-enable-default...android-migration-2/phase-5.2-legacy-pruning)
* **Changes in Branch**:
  * Completely removed `android_legacy_engine_holder` GN target and deleted all legacy platform view, shell holder, and surface files.
  * Pruned prohibited dependencies (`flow`, `runtime`, `shell/common`, `skia`, `impeller`, `lib/ui`, `txt`, `assets`) from `BUILD.gn`.
  * Preserved full JNI native method registrations and VM service callback hooks, validating all golden and integration tests on physical hardware with embedder-only architecture.

---

*Generated: 2026-08-28*
