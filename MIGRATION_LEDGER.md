# Flutter Android Embedder API Migration — State Ledger

## Execution Status Constraints
To enforce flawless execution across PR sequences, this ledger MUST be updated and verified at every checkpoint phase. Future LLM agents should read this ledger to understand the exact state of the rollout before proposing next steps.

* **Base Tracking Commit SHA**: `[TBD: Insert SHA when starting]`
* **Target OS Environment**: Android (All rendering backends: GL, Vulkan, Software)
* **Goal State**: Android Engine initializes completely independent of legacy engine internals, backed purely by public C APIs (`embedder.h`), with 0 legacy code remaining in `shell/platform/android`.

## Quality & Architectural Invariants (Enforced via Ledger)
1. **The Struct-Size Invariant**: Every C API structural addition (`info`) MUST have `struct_size` validated.
2. **The Flag-Gate Invariant**: Any change to rendering logic in Phase 2/3/4 must be gated behind the `FlutterMain::IsEmbedderAPIEnabled()` conditional.
3. **The Cleanup Invariant**: The migration is NOT complete until all `android_legacy_engine_holder` targets are deleted and NO polymorphic intermediate bridges remain.
4. **Synchronous Surface Detach**: Must block the OS from destroying `ANativeWindow` until the rasterizer completes shutdown.
5. **Thread-local EGL Isolation**: Prevent `EGL_BAD_ACCESS` collisions using thread local contexts for offscreen resource pooling.
6. **GN Target Isolation & Dependency Invariant**: The legacy implementation MUST be quarantined with strict `BUILD.gn` visibility rules starting in Phase 2.1. By the conclusion of Phase 5.2, the Android Embedder MUST depend EXCLUSIVELY on:
   * `//flutter/shell/platform/embedder:embedder_as_internal_library` (REQUIRED)
   * `//flutter/fml` (OPTIONAL)
   * `//flutter/shell/platform/common` (OPTIONAL)
   * `//flutter/third_party` (OPTIONAL)
7. **Integration & Golden Testing**: Unit tests are insufficient. The `dev/integration_tests` directory MUST be used to run tests against the local engine. Goldens MUST be generated via the baseline (non-local) engine build, and local engine tests MUST run against those goldens WITHOUT the `UPDATE_GOLDENS` flag.

## Live Phase Tracker

### Phase 0: Baselining
- [ ] 0.1: Baseline C++ Unit & Thread-Safety Tests
- [ ] 0.2: Generate Baseline Golden Images (using non-local engine build via `dev/integration_tests`)

### Phase 1: API Gaps and Additions
- [ ] 1.1: Custom Asset Resolvers
- [ ] 1.2: Vulkan Impeller Render Target
- [ ] 1.3: AHardwareBuffer & Vulkan External Textures
- [ ] 1.4: Multi-Engine Spawning
- [ ] 1.5: Dart Deferred Library Loading (Verified ABI stability / struct-size layout)
- [ ] 1.6: Raster Context Setup & Teardown Hooks
- [ ] 1.7: Extended Semantics Completeness (`FlutterSemanticsNode2`)
- [ ] 1.8: Embedder Screenshot / Raster Bitmap API
- [ ] 1.9: Dart Callback Information Lookup API
- [ ] 1.10: Platform View Multi-Mutations (`ClipPath`, `ClipRSE`)
- [ ] 1.11: Platform Image Decoder / Generator

### Phase 2: Decoupling and Feature Flagging
- [ ] 2.1: Break PlatformViewAndroid Inheritance & Set strict GN Quarantine Visibility
- [ ] 2.2: Adapt APKAssetProvider
- [ ] 2.3: Custom Task Runners & Priorities
- [ ] 2.4: Feature Flag Switch & Test Harness
- [ ] 2.5: Decouple `flutter_main.cc`

### Phase 3: Abstractions & Architecture
- [ ] 3.1: AndroidSurfaceManager Backing Store Pool
- [ ] 3.2: AndroidCompositor & Surface Detach Barrier
- [ ] 3.3: Direct JNI Platform View Mutator Mapping

### Phase 4: JNI Routing & Dual-Stack Rollout
- [ ] 4.1: AndroidEngine Implementation
- [ ] 4.2: JNI Dispatch Dual-Path Routing (Inline if-statements verified)
- [ ] 4.3: Parameterized Multi-Backend Matrix Testing
- [ ] 4.4: Execute E2E Integration and Golden Matrix (Compare local engine runs without UPDATE_GOLDENS against Phase 0.2 baselines)

### Phase 5: Emancipation & Final Purge
- [ ] 5.1: Enable Embedder API Default
- [ ] 5.2: Legacy Bridge Removal & Total BUILD.gn Dependency Pruning (Ensure ONLY permitted dependencies remain)
