# Flutter Android Embedder API Migration — State Ledger

## Current Status
- **Active Phase**: Phase 2.3 (Custom Task Runners & Thread Priorities)
- **Active Branch**: `android-migration-2/phase-2.3-custom-task-runners`
- **Base Commit SHA**: `08d2a471fd10152bb7dbfbb9636cf8fa53e94b8b`
- **Completed PRs**: [0.1, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 1.10, 1.11, 2.1, 2.2]
- **Latest Commit SHA**: `e95aa6041e6` (Phase 2.2)
- **Adversarial Review Loop Status**: Phase 2.2 Approved by reidbaker-agent
- **Target Dependency Boundary**:
  - Required: `//flutter/shell/platform/embedder:embedder_as_internal_library`
  - Optional: `//flutter/fml`, `//flutter/shell/platform/common`, `//flutter/third_party`
  - Prohibited: `//flutter/assets`, `common`, `flow`, `impeller`, `lib/ui`, `runtime`, `shell/common`, `skia`, `txt`, `vulkan`

## Quick PR Index & Checkpoints
- [x] Phase 0.1: Baseline & Thread-Safety Tests
- [x] Phase 1.1: Custom Asset Resolvers (`FlutterAssetResolver`)
- [x] Phase 1.2: Vulkan Impeller Render Target Backing Store
- [x] Phase 1.3: AHardwareBuffer & Vulkan External Textures
- [x] Phase 1.4: Multi-Engine Spawning (`FlutterEngineSpawn`)
- [x] Phase 1.5: Dart Deferred Library Loading
- [x] Phase 1.6: Raster Context Setup Hooks (`setup_callback`)
- [x] Phase 1.7: Extended Semantics Completeness (`FlutterSemanticsNode2`)
- [x] Phase 1.8: Embedder Screenshot / Raster Bitmap API (`FlutterEngineScreenshot`)
- [x] Phase 1.9: Dart Callback Information Lookup API (`FlutterEngineGetCallbackInformation`)
- [x] Phase 1.10: Platform View Extended Mutation Types (`ClipPath`, `ClipRSE`)
- [x] Phase 1.11: Platform Image Decoder / Generator Registration C API
- [x] Phase 2.1: Break PlatformViewAndroid Inheritance & Establish GN Quarantine Target
- [x] Phase 2.2: Adapt APKAssetProvider
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
