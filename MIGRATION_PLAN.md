# Flutter Android Embedder Migration: Ultimate Master Plan

This document represents the synthesized blueprint for migrating the Flutter Android Embedder to the public C Embedder API (`embedder.h`). It guarantees 100% feature parity without regressions by ensuring all capabilities (including advanced graphics and Add-to-App spawning) are fully integrated before legacy deletion.

## 1. Architectural Guardrails (The Invariants)

1. **Zero-Regression Feature Parity**: All existing features—including Vulkan External Textures and Add-to-App multi-engine spawning—MUST be fully ported to the new Embedder API *before* the legacy code is deleted. 
2. **Strict C-ABI Protection (Opaque Handles)**: `embedder.h` must remain strictly OS-agnostic. Android-specific OS constructs (`AHardwareBuffer`) must be modeled as opaque handles (e.g., `void* os_handle`) within universal structs, or strictly confined to separate OS-specific extension headers. Do NOT shatter the standard C-ABI.
3. **JNI Routing Boundary & JvmInvoker**: The structural rollout flip (`if (IsEmbedderEnabled())`) MUST occur natively inside the raw JNI boundary function. If true, the `JniDelegate` handles the call, but it MUST be injected with an abstracted `JvmInvoker` interface. This prevents the raw JNI boundary from becoming a monolithic god-class and allows host tests to inject a mocked `JvmInvoker` to test JVM callback logic.
4. **Dynamic Decoupling & Host Test Safety**: Use `dlsym`/`dlopen` wrapped in an `OSLibraryLoader` interface for Android native bindings (like `AChoreographer`, `AHardwareBuffer`). This MUST be present before any Android-specific graphics are implemented to protect desktop CI.
5. **Perfetto Tracing Mandate**: All multi-threaded implementations, JNI asynchronous routing bounds, and C++ callbacks (e.g., `AChoreographer`, Engine Spawn threading) MUST be instrumented with Perfetto trace events (`fml/trace_event.h`). Perfetto traces must be used to validate execution correctness and identify lifecycle or pacing bugs.
5. **Pre-Emptive GN Shield**: Create a strict `flutter_embedder_native` GN target in Phase 1 that explicitly forbids internal UI/Skia headers, ensuring all new development is structurally validated from Day 1.

## 2. Sequencing Rules (Correcting Oversights)

- **Rule 1: Virtualization & Routing First**: The JNI routing logic (`if/else`), `JniDelegate` adapter (with `JvmInvoker`), and `OSLibraryLoader` must be implemented in Phase 1 BEFORE subsystem logic.
- **Rule 2: Complete Flag Eradication**: Phase 5.2 must completely eradicate the rollout flag from the Java/C++ API surface and obliterate the `if/else` conditional entirely. Hardcode unconditional routing to the new Embedder API to prevent downstream failures.
- **Rule 3: Multi-Backend Matrix Testing**: All tests must utilize Parameterized Tests (`TEST_P`) across backends.

---

## 3. The Phased Blueprint

### Phase 1: Foundations, Safety Nets, and C-API Prep
* **1.1 Test Matrix**: Wire up `TEST_P` logic. 
* **1.2 Pre-Emptive GN Quarantine**: Create `flutter_embedder_native` target dependent strictly on `embedder.h`.
* **1.3 JNI DI Interface & Inline Routing**: Implement `if (Flags.isEmbedderApiInputEnabled())` inside the raw JNI boundary. Inject `JvmInvoker` into `JniDelegate` for abstracted JVM callbacks.
* **1.4 Dynamic Virtualization**: Implement `OSLibraryLoader` wrapper to shield desktop host tests.
* **1.5 C-API Extension (Vulkan)**: Expand `embedder.h` with opaque cross-platform abstractions for Vulkan External Textures.
* **1.6 C-API Extension (AHardwareBuffer)**: Expand `embedder.h` with opaque abstractions for Android `AHardwareBuffer` zero-copy textures.
* **1.7 C-API Extension (Engine Spawn)**: Expand `embedder.h` with `FlutterEngineSpawn` support for Add-to-App capabilities.
  * *Evaluated API Selection*: We utilize the Approach 5 implementation (`FlutterEngineSpawnConfig` and `FlutterEngineSpawn`) over Approach 4's `FlutterEngineSpawnInfo` because the `Config` suffix correctly aligns with existing embedder paradigms (e.g., `FlutterRendererConfig`).
  * *Required API*: `FlutterEngineResult FlutterEngineSpawn(FlutterEngine engine, const FlutterEngineSpawnConfig* config, FlutterEngine* spawned_engine_out);`
* **1.8 C-API Extension (Dart Deferred Components)**: Expand `embedder.h` with `FlutterEngineLoadDartDeferredLibrary` and corresponding struct configurations to map Play Feature Delivery components safely.
  * *Evaluated API Selection*: We utilize the Approach 4/5 implementation leveraging strict `struct_size` padding across `FlutterDartDeferredLibrary` and `FlutterDartDeferredLibraryLoadError`. This supersedes Approach 3 which catastrophically violated C-ABI backwards compatibility by passing raw integer IDs directly via function arguments.
  * *Required API*: `FlutterEngineResult FlutterEngineLoadDartDeferredLibrary(FlutterEngine engine, const FlutterDartDeferredLibrary* deferred_library);` and `FlutterEngineNotifyDartDeferredLibraryLoadError(FlutterEngine, const FlutterDartDeferredLibraryLoadError*)`.
* **1.9 C-API Extension (Screenshot API)**: Expand `embedder.h` with `FlutterEngineScreenshot` and `FlutterEngineFreeScreenshot` to synchronously capture raster bitmaps across the boundary.
  * *Evaluated API Selection*: We select the Approach 5 implementation (`FlutterScreenshotFormatR8G8B8A8UNormInt`) over Approach 4 (`FlutterEngineScreenshotFormatRGBA8888`), as Approach 5 correctly aligns its enum nomenclature with Impeller/Vulkan spec graphics standards. It also relies exclusively on `size_t width` and `height`, completely avoiding the C-ABI poisoning trap of `SkISize` seen in Approach 1.
  * *Required APIs*: `FlutterEngineResult FlutterEngineScreenshot(FlutterEngine engine, const FlutterScreenshotRequest* request, FlutterScreenshot* screenshot_out);` and `FlutterEngineFreeScreenshot(FlutterScreenshot* screenshot);`
* **1.10 C-API Extension (Raster Context Hooks)**: Expand `FlutterProjectArgs` with `raster_thread_context_make_current` and `clear_current` to explicitly route Thread/EGL context lifetimes.
  * *Required API*: Add `bool (*raster_thread_context_make_current)(void* user_data)` and `bool (*raster_thread_context_clear_current)(void* user_data)` to `FlutterProjectArgs`.
* **1.11 C-API Extension (Thread Priorities)**: Expand `FlutterProjectArgs` with `custom_task_runners` mapping Android's `ALooper` and strict thread priorities (e.g. `PRIORITY_DISPLAY`) onto the backend.
  * *Required API*: Bind the `FlutterProjectArgs::custom_task_runners` struct dynamically.

### Phase 2: Decoupled Subsystems
* **2.1 Asset Resolver**: Adapt `APKAssetProvider` using the embedder custom asset resolver pattern.
  * *Required API*: Add `const FlutterAssetResolver** asset_resolvers; size_t asset_resolvers_count;` to `FlutterProjectArgs`. Wire `FlutterEngineUpdateAssetResolver`.
* **2.2 Dart Callbacks**: Implement `FlutterEngineGetCallbackInformation` hook.
  * *Required APIs*: `FlutterEngineGetCallbackInformation(int64_t handle, FlutterCallbackInformation* callback_info_out);` and `FlutterEngineGetCallbackHandle`.
* **2.3 Image Generators**: Hook `AndroidImageGenerator` to `FlutterEngineRegisterImageGenerator`.
  * *Evaluated API Selection*: We use Approach 5's `FlutterEngineRegisterImageGenerator` over Approach 4's `FlutterEngineRegisterImageDecoder` since the term "Generator" accurately maps natively to `flutter::ImageGenerator` within the engine UI structure. We will implement `FlutterImageGeneratorRegistrationInfo` using pure C structs to avoid pulling in C++ UI headers (avoiding the Approach 3 target bleed).
  * *Required API*: Add `const FlutterImageGeneratorRegistrationInfo** image_generators; size_t image_generators_count;` to `FlutterProjectArgs`.
* **2.4 Mutator Translation**: Implement `AndroidMutatorsMapper`.
* **2.5 Accessibility & Semantics**: Wire the Android accessibility bridge tree native updates.
* **2.6 Platform Views**: Wire `AndroidPlatformView` and `PlatformViewsController` integrations.
* **2.7 Window Metrics Translation**: The C-API uses `FlutterEngineSendWindowMetricsEvent` to handle display DPI, padding, and cutouts. Route Java metrics here to safely drop `android_display.cc`.
* **2.8 AChoreographer VSync Routing**: Utilize the Phase 1.4 Virtualization `OSLibraryLoader` to capture `AChoreographer` callbacks and route them into `FlutterProjectArgs::vsync_callback` to fix 120Hz frame pacing before legacy files drop.
* **2.9 Global VM Initialization (`flutter_main.cc`)**: Migrate global startup (AOT snapshot mapping, ICU data mounting) out of legacy Android singletons into the public `FlutterEngineInitialize` API.

### Phase 3: Advanced Graphics & Multi-Engine Integration
* **3.1 AHardwareBuffer**: Wire the Android implementation to the Phase 1 opaque hooks via `OSLibraryLoader`.
* **3.2 Vulkan External Textures**: Wire the Android Vulkan instances relying on the `OSLibraryLoader` virtualization.
* **3.3 SurfaceControl HCPP**: Add dual-mode UI presentation natively into the new pipeline.
* **3.4 Add-to-App Multi-Engine**: Wire `FlutterEngineGroup` natively to `FlutterEngineSpawn`. Java wrappers must be explicitly wired with a JNI `PhantomReference` or `Cleaner` registry catching GC events to route `FlutterEngineShutdown` and prevent pointer leaks.

### Phase 4: E2E Parity
* **4.1 CI E2E Harness**: Verify 100% test passing on `dev/integration_tests/channels`.

### Phase 5: Emancipation (The Purge)
* **5.1 Default Flip**: Change the default settings fallback to `true`.
* **5.2 Legacy Deletion (Subsystems)**: Delete legacy classes for Asset, Callback, Image, and Mutators.
* **5.3 Legacy Deletion (Platform Views & Semantics)**: Purge legacy accessibility and platform view hierarchies.
* **5.4 Legacy Deletion (Graphics Pipeline)**: Delete `android_context`, `external_view_embedder`, and `android_surface`.
* **5.5 Flag Obliteration**: Obliterate the `IsEmbedderEnabled` flag entirely and hardcode unconditionally to the new Embedder API.
* **5.6 Final GN Integration**: Migrate `flutter_embedder_native` into `flutter_shell_native` AND explicitly purge all legacy UI/Skia dependencies from `flutter_shell_native`'s `BUILD.gn` to prevent Post-Migration Relapse.


## Appendix A: C-API Reference Implementations (v7 Validated)

The following implementations MUST be used literally to implement the C-API boundary. They have been rigorously validated under the v7 sequence for C-ABI safety.

### Phase 1.7: Engine Spawn
```c
typedef struct {
  /// The size of this struct. Must be sizeof(FlutterEngineSpawnConfig).
  size_t struct_size;

  /// Custom project arguments for the spawned engine (e.g. custom entrypoint,
  /// entrypoint arguments, callbacks, engine ID, etc.).
  /// This field is optional; nullptr may be specified.
  const FlutterProjectArgs* custom_args;

  /// Custom renderer configuration for the spawned engine.
  /// This field is optional; if nullptr, renderer configuration from the parent
  /// engine is inherited.
  const FlutterRendererConfig* custom_renderer_config;

  /// User data baton passed back to embedders in callbacks for the spawned
  /// engine. This field is optional.
  void* user_data;

  /// Initial route for the spawned engine isolate.
  /// This field is optional; nullptr or empty string defaults to "/".
  const char* initial_route;
} FlutterEngineSpawnConfig;

FLUTTER_EXPORT
FlutterEngineResult FlutterEngineSpawn(
    FLUTTER_API_SYMBOL(FlutterEngine) parent_engine,
    const FlutterEngineSpawnConfig* config,
    FLUTTER_API_SYMBOL(FlutterEngine)* engine_out);
```

### Phase 1.8: Dart Deferred Libraries
```c
FLUTTER_EXPORT
FlutterEngineResult FlutterEngineLoadDartDeferredLibrary(
    FLUTTER_API_SYMBOL(FlutterEngine) engine,
    int64_t loading_unit_id,
    const uint8_t* snapshot_data,
    size_t snapshot_data_size,
    const uint8_t* snapshot_instructions,
    size_t snapshot_instructions_size);

FLUTTER_EXPORT
FlutterEngineResult FlutterEngineNotifyDartDeferredLibraryLoadError(
    FLUTTER_API_SYMBOL(FlutterEngine) engine,
    int64_t loading_unit_id,
    const char* error_message,
    bool transient);
```

### Phase 1.9: Screenshot API
```c
typedef struct {
  /// The size of this struct. Must be sizeof(FlutterEngineScreenshotInfo).
  size_t struct_size;

  /// The width of the screenshot in physical pixels.
  uint32_t width;

  /// The height of the screenshot in physical pixels.
  uint32_t height;

  /// The number of bytes per row of pixels (stride).
  size_t row_bytes;

  /// Pointer to the raw uncompressed raster pixel buffer.
  /// The memory is allocated by the engine and must be freed by passing this
  /// screenshot struct to `FlutterEngineFreeScreenshot`.
  const void* pixels;

  /// The size in bytes of the buffer pointed to by `pixels`.
  size_t pixels_size;
} FlutterEngineScreenshotInfo;

FLUTTER_EXPORT
FlutterEngineResult FlutterEngineScreenshot(
    FLUTTER_API_SYMBOL(FlutterEngine) engine,
    FlutterEngineScreenshotInfo* screenshot_out);

FLUTTER_EXPORT
FlutterEngineResult FlutterEngineFreeScreenshot(
    const FlutterEngineScreenshotInfo* screenshot);
```

### Phase 2.2: Dart Callbacks API
```c
typedef struct {
  /// The size of this struct. Must be sizeof(FlutterCallbackInformation).
  size_t struct_size;

  /// The name of the callback.
  const char* name;

  /// The class name if the callback is a method of a class. Null if top-level.
  const char* class_name;

  /// The library path where the callback is defined.
  const char* library_path;
} FlutterCallbackInformation;

FLUTTER_EXPORT
FlutterEngineResult FlutterEngineGetCallbackInformation(
    int64_t handle,
    FlutterCallbackInformation* callback_info_out);
```

### Phase 2.3: Image Decoders
```c
typedef bool (*FlutterImageDecoderCallback)(const uint8_t* /* data */,
                                            size_t /* data_size */,
                                            void* /* user_data */);

FLUTTER_EXPORT
FlutterEngineResult FlutterEngineRegisterImageDecoder(
    FLUTTER_API_SYMBOL(FlutterEngine) engine,
    FlutterImageDecoderCallback callback,
    void* user_data,
    int32_t priority);
```

