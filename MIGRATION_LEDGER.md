# Flutter Android Embedder API Migration — State Ledger

## Current Status
- **Active Phase**: Phase 2.3 (Standardize Thread Priority Setters on Custom Task Runners)
- **Active Branch**: `android-embedder-v3/phase-2.3-custom-task-runners`
- **Base Commit SHA**: `f7d7a1900e2` (Phase 2.2 commit)
- **Completed PRs**: [0.1, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9, 1.10, 1.11, 2.1, 2.2]
- **Adversarial Review Loop Status**: Complete (Phase 2.2 Approved with 0 findings)
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
## Quality & Architectural Invariants (Enforced via Ledger)
1. **The Struct-Size & C-ABI Invariant**: Every C API structural addition (`info`) MUST have `struct_size` validated. C++ headers (like Skia's `SkPath.h`) MUST NOT be `#include`d in `embedder.h` under any circumstances to preserve the C-ABI.
2. **The Flag-Gate Matrix Invariant**: Any change to rendering logic in Phase 2/3/4 must be gated behind the `FlutterMain::IsEmbedderAPIEnabled()` conditional. Furthermore, STARTING IN PHASE 2.4, ALL testing (Unit, Integration, and Goldens) MUST be executed sequentially under BOTH flag states (`flag=true` AND `flag=false`) to prevent breaking the legacy canary path.
3. **The Cleanup Invariant**: The migration is NOT complete until all `android_legacy_engine_holder` targets are deleted and NO polymorphic intermediate bridges remain.
4. **Thread-Safe Surface Detach**: Must prevent the OS from destroying `ANativeWindow` until the rasterizer completes shutdown, but MUST execute this asynchronously or with safe timeouts to prevent ANRs (Application Not Responding) on the Main/JNI thread.
5. **Thread-local EGL Isolation**: Prevent `EGL_BAD_ACCESS` collisions using thread local contexts for offscreen resource pooling.
6. **GN Target Isolation & Dependency Invariant**: The legacy implementation MUST be quarantined with strict `BUILD.gn` visibility rules starting in Phase 2.1 (JNI routing layers are granted cross-visibility to compile). By the conclusion of Phase 5.2, the Android Embedder MUST depend EXCLUSIVELY on the permitted targets & NDK libraries.
7. **Every-PR Validation Invariant**: EVERY PR submitted during this migration MUST verify correctness by executing: (1) Unit tests, (2) Integration tests (`dev/integration_tests`), and (3) The Dual-Pass Golden methodology (Pass 1: non-local generation with `UPDATE_GOLDENS=true`, Pass 2: local engine verification with `UPDATE_GOLDENS=false`). **Goldens MUST NOT be checked into the repository.**
8. **The Adversarial Review Invariant**: EVERY PR MUST be submitted for independent adversarial review. You cannot mark a PR complete until all reviewer feedback is addressed and tests re-verify successfully (capped at a maximum of 3 iterations to prevent infinite algorithmic stalling).
## Live Phase Tracker (Tick when tests & review are validated)
### Phase 0: Baselining
- [x] 0.1: Baseline C++ Unit & Thread-Safety Tests (Verified Tests & Adversarial Review)
### Phase 1: API Gaps and Additions
- [x] 1.1: Custom Asset Resolvers (Verified Tests & Adversarial Review)
- [x] 1.2: Vulkan Impeller Render Target (Verified Tests & Adversarial Review)
- [x] 1.3: AHardwareBuffer & Vulkan External Textures (Verified Tests & Adversarial Review)
- [x] 1.4: Multi-Engine Spawning (Verified Tests & Adversarial Review)
- [x] 1.5: Dart Deferred Library Loading (Verified ABI & Tests & Adversarial Review)
- [x] 1.6: Raster Context Setup & Teardown Hooks (Verified Tests & Adversarial Review)
- [x] 1.7: Extended Semantics Completeness (`FlutterSemanticsNode2`) (Verified Tests & Adversarial Review)
- [x] 1.8: Embedder Screenshot / Raster Bitmap API (Verified Tests & Adversarial Review)
- [x] 1.9: Dart Callback Information Lookup API (Verified Tests & Adversarial Review)
- [x] 1.10: Platform View Multi-Mutations (`ClipPath`, `ClipRSE`) (Verified C-ABI & Tests & Adversarial Review)
- [x] 1.11: Platform Image Decoder / Generator (Verified Tests & Adversarial Review)
### Phase 2: Decoupling and Feature Flagging
- [x] 2.1: GN Quarantine Visibility (Verified Tests & Adversarial Review)
- [x] 2.2: Adapt APKAssetProvider (Verified Tests & Adversarial Review)
- [x] 2.3: Custom Task Runners (Verified Tests & Adversarial Review)
- [ ] 2.4: Feature Flag Switch (Verified Tests & Adversarial Review)
- [ ] 2.5: Decouple `flutter_main.cc` (Verified Tests & Adversarial Review)
### Phase 3: Abstractions & Architecture
- [ ] 3.1: AndroidSurfaceManager (Verified Dual-Flag Tests & Adversarial Review)
- [ ] 3.2: AndroidCompositor (Verified ANR-Safe & Dual-Flag Tests & Adversarial Review)
- [ ] 3.3: Direct JNI Mutator Mapping (Verified Dual-Flag Tests & Adversarial Review)
### Phase 4: JNI Routing & Dual-Stack Rollout
- [ ] 4.1: AndroidEngine Implementation (Verified Dual-Flag Tests & Adversarial Review)
- [ ] 4.2: JNI Dispatch Routing / Inline if-statements (Verified Dual-Flag Tests & Adversarial Review)
- [ ] 4.3: Parameterized Multi-Backend Matrix Testing (Verified Dual-Flag Tests & Adversarial Review)
### Phase 5: Emancipation & Final Purge
- [ ] 5.1: Enable Embedder API Default (Verified Dual-Flag Tests & Adversarial Review)
- [ ] 5.2: (Waited for Rollout Window) Legacy Bridge Removal & Total GN Pruning (Verified Tests & Adversarial Review)
