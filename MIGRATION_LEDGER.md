# Flutter Android Embedder API Migration — State Ledger

## Execution Status Constraints
To enforce flawless execution across PR sequences, this ledger MUST be updated and verified at every checkpoint phase. Future LLM agents should read this ledger to understand the exact state of the rollout before proposing next steps.

* **Base Tracking Commit SHA**: `[TBD: Insert SHA when starting]`
* **Target OS Environment**: Android (All rendering backends: GL, Vulkan, Software)
* **Goal State**: Android Engine initializes completely independent of legacy engine internals, backed purely by public C APIs (`embedder.h`), with 0 legacy code remaining in `shell/platform/android`. All unit tests, integration tests, and golden tests MUST pass. Furthermore, the ONLY engine dependencies permitted are: `//flutter/shell/platform/embedder:embedder_as_internal_library` (required), `//flutter/fml` (optional), `//flutter/shell/platform/common` (optional), and `//flutter/third_party` (optional).

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
7. **Every-PR Validation Invariant**: EVERY PR submitted during this migration MUST verify correctness by executing: (1) Unit tests, (2) Integration tests (`dev/integration_tests`), and (3) The Dual-Pass Golden methodology (Pass 1: non-local generation with `UPDATE_GOLDENS=true`, Pass 2: local engine verification with `UPDATE_GOLDENS=false`). **Goldens MUST NOT be checked into the repository.**
8. **The Adversarial Review Invariant**: EVERY PR MUST be submitted for independent adversarial review. You cannot mark a PR complete until all reviewer feedback is addressed, tests re-verify successfully, and the reviewer signs off with zero remaining findings.

## Live Phase Tracker (Tick when tests & review are validated)

### Phase 0: Baselining
- [ ] 0.1: Baseline C++ Unit & Thread-Safety Tests (Verified Tests & Adversarial Review)

### Phase 1: API Gaps and Additions
- [ ] 1.1: Custom Asset Resolvers (Verified Tests & Adversarial Review)
- [ ] 1.2: Vulkan Impeller Render Target (Verified Tests & Adversarial Review)
- [ ] 1.3: AHardwareBuffer & Vulkan External Textures (Verified Tests & Adversarial Review)
- [ ] 1.4: Multi-Engine Spawning (Verified Tests & Adversarial Review)
- [ ] 1.5: Dart Deferred Library Loading (Verified ABI & Tests & Adversarial Review)
- [ ] 1.6: Raster Context Setup & Teardown Hooks (Verified Tests & Adversarial Review)
- [ ] 1.7: Extended Semantics Completeness (`FlutterSemanticsNode2`) (Verified Tests & Adversarial Review)
- [ ] 1.8: Embedder Screenshot / Raster Bitmap API (Verified Tests & Adversarial Review)
- [ ] 1.9: Dart Callback Information Lookup API (Verified Tests & Adversarial Review)
- [ ] 1.10: Platform View Multi-Mutations (`ClipPath`, `ClipRSE`) (Verified Tests & Adversarial Review)
- [ ] 1.11: Platform Image Decoder / Generator (Verified Tests & Adversarial Review)

### Phase 2: Decoupling and Feature Flagging
- [ ] 2.1: GN Quarantine Visibility (Verified Tests & Adversarial Review)
- [ ] 2.2: Adapt APKAssetProvider (Verified Tests & Adversarial Review)
- [ ] 2.3: Custom Task Runners (Verified Tests & Adversarial Review)
- [ ] 2.4: Feature Flag Switch (Verified Tests & Adversarial Review)
- [ ] 2.5: Decouple `flutter_main.cc` (Verified Tests & Adversarial Review)

### Phase 3: Abstractions & Architecture
- [ ] 3.1: AndroidSurfaceManager (Verified Tests & Adversarial Review)
- [ ] 3.2: AndroidCompositor (Verified Tests & Adversarial Review)
- [ ] 3.3: Direct JNI Mutator Mapping (Verified Tests & Adversarial Review)

### Phase 4: JNI Routing & Dual-Stack Rollout
- [ ] 4.1: AndroidEngine Implementation (Verified Tests & Adversarial Review)
- [ ] 4.2: JNI Dispatch Routing / Inline if-statements (Verified Tests & Adversarial Review)
- [ ] 4.3: Parameterized Multi-Backend Matrix Testing (Verified Tests & Adversarial Review)

### Phase 5: Emancipation & Final Purge
- [ ] 5.1: Enable Embedder API Default (Verified Tests & Adversarial Review)
- [ ] 5.2: Legacy Bridge Removal & Total GN Pruning (Verified Tests & Adversarial Review)
