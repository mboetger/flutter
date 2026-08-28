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
7. **Every-PR Validation Invariant**: EVERY PR submitted during this migration MUST verify correctness by executing: (1) Unit tests, (2) Integration tests (`dev/integration_tests`), and (3) The Dual-Pass Golden methodology (Pass 1: non-local generation with `UPDATE_GOLDENS=true`, Pass 2: local engine verification with `UPDATE_GOLDENS=false`). **Goldens MUST NOT be checked into the repository.**

## Live Phase Tracker (Tick when tests are validated)

### Phase 0: Baselining
- [ ] 0.1: Baseline C++ Unit & Thread-Safety Tests (Verified Unit/Integration/Dual-Pass Goldens)

### Phase 1: API Gaps and Additions
- [ ] 1.1: Custom Asset Resolvers (Verified Unit/Integration/Dual-Pass Goldens)
- [ ] 1.2: Vulkan Impeller Render Target (Verified Unit/Integration/Dual-Pass Goldens)
- [ ] 1.3: AHardwareBuffer & Vulkan External Textures (Verified Unit/Integration/Dual-Pass Goldens)
- [ ] 1.4: Multi-Engine Spawning (Verified Unit/Integration/Dual-Pass Goldens)
- [ ] 1.5: Dart Deferred Library Loading (Verified ABI & Unit/Integration/Dual-Pass Goldens)
- [ ] 1.6: Raster Context Setup & Teardown Hooks (Verified Unit/Integration/Dual-Pass Goldens)
- [ ] 1.7: Extended Semantics Completeness (`FlutterSemanticsNode2`) (Verified Unit/Integration/Dual-Pass Goldens)
- [ ] 1.8: Embedder Screenshot / Raster Bitmap API (Verified Unit/Integration/Dual-Pass Goldens)
- [ ] 1.9: Dart Callback Information Lookup API (Verified Unit/Integration/Dual-Pass Goldens)
- [ ] 1.10: Platform View Multi-Mutations (`ClipPath`, `ClipRSE`) (Verified Unit/Integration/Dual-Pass Goldens)
- [ ] 1.11: Platform Image Decoder / Generator (Verified Unit/Integration/Dual-Pass Goldens)

### Phase 2: Decoupling and Feature Flagging
- [ ] 2.1: GN Quarantine Visibility (Verified Unit/Integration/Dual-Pass Goldens)
- [ ] 2.2: Adapt APKAssetProvider (Verified Unit/Integration/Dual-Pass Goldens)
- [ ] 2.3: Custom Task Runners (Verified Unit/Integration/Dual-Pass Goldens)
- [ ] 2.4: Feature Flag Switch (Verified Unit/Integration/Dual-Pass Goldens)
- [ ] 2.5: Decouple `flutter_main.cc` (Verified Unit/Integration/Dual-Pass Goldens)

### Phase 3: Abstractions & Architecture
- [ ] 3.1: AndroidSurfaceManager (Verified Unit/Integration/Dual-Pass Goldens)
- [ ] 3.2: AndroidCompositor (Verified Unit/Integration/Dual-Pass Goldens)
- [ ] 3.3: Direct JNI Mutator Mapping (Verified Unit/Integration/Dual-Pass Goldens)

### Phase 4: JNI Routing & Dual-Stack Rollout
- [ ] 4.1: AndroidEngine Implementation (Verified Unit/Integration/Dual-Pass Goldens)
- [ ] 4.2: JNI Dispatch Routing / Inline if-statements (Verified Unit/Integration/Dual-Pass Goldens)
- [ ] 4.3: Parameterized Multi-Backend Matrix Testing (Verified Unit/Integration/Dual-Pass Goldens)

### Phase 5: Emancipation & Final Purge
- [ ] 5.1: Enable Embedder API Default (Verified Unit/Integration/Dual-Pass Goldens)
- [ ] 5.2: Legacy Bridge Removal & Total GN Pruning (Verified Unit/Integration/Dual-Pass Goldens)
