# Flutter Android Embedder API Migration - Master Plan

This document serves as the absolute source of truth and architectural guideline for migrating the Flutter Android Embedder to use the public C Embedder API (`embedder.h`).

Multiple previous LLM migration attempts have been analyzed, and this plan synthesizes their successes while directly preventing their recurrent architectural mistakes. Adhere to this document strictly to ensure ABI stability, subsystem completeness, strict dependency isolation, and zero-debt code cleanup.

## Core Migration Rules (DO NOT DEVIATE)

### 1. C-API ABI Extensibility (The "Struct Size" Rule)
Any new C API endpoint added to `embedder.h` (e.g., Dart Deferred Library Loading, Screenshotting, Callback lookups) that accepts more than a single primitive value **MUST NOT use flat function arguments**. 
Instead, you must declare a `struct` to pass the arguments. 
**CRITICAL**: The very first field of this struct MUST be `size_t struct_size;`. This allows future framework iterations to append fields without breaking the compiled ABI for older clients.
* **INCORRECT**: `FlutterEngineResult FlutterEngineLoadDartDeferredLibrary(engine, loading_unit_id, data, data_size);`
* **CORRECT**: 
  ```c
  typedef struct {
    size_t struct_size;
    intptr_t loading_unit_id;
    const uint8_t* snapshot_data;
    size_t snapshot_data_size;
  } FlutterDeferredLibraryInfo;

  FlutterEngineResult FlutterEngineLoadDartDeferredLibrary(FLUTTER_API_SYMBOL(FlutterEngine) engine, const FlutterDeferredLibraryInfo* info);
  ```
  *(Note: You must validate `info->struct_size` inside the C++ implementation before accessing fields).*

### 2. Comprehensive Subsystem Identification
Previous attempts incorrectly assumed the Android embedder only manages the graphics surface and input mutators. The Android Embedder manages far more. Before attempting to swap the JNI backend, you MUST implement the C-APIs for the following subsystems:
1. **Screenshot API**: `FlutterEngineScreenshot` is required for testing.
2. **Dart Callback Information**: `FlutterEngineGetCallbackInformation` (lookup via plugin handles).
3. **Image Generator Extension**: `FlutterEngineRegisterImageGenerator`.
4. **Extended Platform View Mutations**: E.g., `ClipPath` and `ClipRSE` (Rounded Superellipse).
5. **Semantics/A11y Extensions**: `FlutterSemanticsNode2` for advanced string attributions.

### 3. GN Target Quarantine & Strict Dependency Rules
During the migration, you MUST use `BUILD.gn` targets and visibility rules to completely quarantine existing legacy engine dependencies and guarantee the new implementation does not accidentally rely on internal engine components.

1. **Quarantine Target**: Early in Phase 2, separate legacy components into an `android_legacy_engine_holder` GN target. Apply strict visibility rules to this target so NO new code can depend on it.
2. **Final Dependency End State**: By the end of Phase 5, the Android Embedder implementation MUST depend **ONLY** on the following dependencies:
   * `//flutter/shell/platform/embedder:embedder_as_internal_library` (REQUIRED)
   * `//flutter/fml` (OPTIONAL)
   * `//flutter/shell/platform/common` (OPTIONAL)
   * `//flutter/third_party` (OPTIONAL)
3. **Prohibited Dependencies**: If your Android embedder GN target depends on `assets`, `common`, `flow`, `impeller`, `lib/ui`, `runtime`, `shell/common`, `skia`, `txt`, or `vulkan`, **you have failed the abstraction**.

### 4. JNI Conditional Dispatch (Transition Phase)
During the transition (Phase 4), you will need to dual-dispatch JNI calls to either the legacy `AndroidShellHolder` or the new `AndroidEngine` based on `FlutterMain::IsEmbedderAPIEnabled()`.
* **DO NOT** introduce a lingering polymorphic C++ Facade abstraction (e.g., an `AndroidEngineBridge` interface that wraps both). This creates leftover pointer indirection that will rot in the codebase after the migration finishes.
* **DO** modify `platform_view_android_jni_impl.cc` directly to use inline `if/else` checks. This localizes the transition mess to the JNI file, making Phase 5 cleanup perfectly clean by simply deleting the `else` blocks.

### 5. Zero-Debt Surgical Cleanup (Phase 5)
When the feature flag is rolled out (Phase 5.2), your final cleanup branch must be absolute.
* Delete the `android_legacy_engine_holder` GN target entirely.
* Delete `AndroidShellHolder`, `PlatformViewAndroid`, legacy adapters, and old EGL/Surface wrapper implementations.
* Prune all prohibited dependencies from the Android `BUILD.gn`.
* Update `platform_view_android_jni_impl.cc` to map 1:1 solely to `AndroidEngine`.
* Expect to delete over 100+ files and 14,000+ lines of codebase to complete this phase.

---

## The 5-Phase Execution Sequence

All PRs should be strictly prefixed with `android-migration/phase-X.Y-[description]` to provide sequential reviewability.

### Phase 0: Baselining
* **0.1**: Add extensive C++ unit tests to baseline rendering layers (GL/Vulkan/Software), message handlers, and isolate thread-safety *before* starting the C++ refactor.

### Phase 1: API Gaps and Additions
* **1.1**: Custom Asset Resolvers (`FlutterAssetResolver`)
* **1.2**: Vulkan Impeller Render Target Backing Store
* **1.3**: AHardwareBuffer & Vulkan External Textures
* **1.4**: Multi-Engine Spawning (`FlutterEngineSpawn`)
* **1.5**: Dart Deferred Library Loading (MUST USE STRUCT WITH `struct_size`)
* **1.6**: Raster Context Setup & Teardown Hooks
* **1.7**: Extended Semantics Completeness (`FlutterSemanticsNode2`)
* **1.8**: Embedder Screenshot / Raster Bitmap API
* **1.9**: Dart Callback Information Lookup API
* **1.10**: Platform View Extended Mutation Types (`ClipPath`, `ClipRSE`)
* **1.11**: Platform Image Decoder / Generator Registration C API

### Phase 2: Decoupling and Feature Flagging
* **2.1**: Break `PlatformViewAndroid` Inheritance & Establish GN Quarantine Target with Strict Visibility
* **2.2**: Adapt `APKAssetProvider` to `FlutterAssetResolver`
* **2.3**: Custom Task Runners & Thread Priorities
* **2.4**: Wire up the `ENABLE_ANDROID_EMBEDDER_API` feature flag & test harness overrides
* **2.5**: Decouple `flutter_main.cc` from engine internals

### Phase 3: Abstractions & Architecture
* **3.1**: Implement `AndroidSurfaceManager` (Backing store pool, EGL Contexts)
* **3.2**: Implement `AndroidCompositor` (Layer presentation, Synchronous detach barrier)
* **3.3**: Direct Platform View Mutators mapping & DPR Normalization

### Phase 4: JNI Routing & Dual-Stack Rollout
* **4.1**: Implement `AndroidEngine` to orchestrate C-APIs
* **4.2**: Implement JNI Dispatch Dual-Path Routing (Inline `if`-statements)
* **4.3**: Add Parameterized Multi-Backend Matrix units tests (`flag=true`, `flag=false`)

### Phase 5: Emancipation
* **5.1**: Enable Embedder API by default with negative rollback flags
* **5.2**: Legacy Bridge Removal & Total BUILD.gn Dependency Pruning (Ensure END STATE dependencies only)
