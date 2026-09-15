# Android Embedder Test Gap Inventory

**Audience:** Engineers and agents executing the Android Embedder API Migration (v8).  
**Task:** T-0.1  
**Status:** Canonical Baseline Inventory

---

## 1. Executive Summary

Prior to refactoring `shell/platform/android` onto the Embedder API (`embedder.h`), we must establish an authoritative inventory of what is and is not tested. On the iOS equivalent of this migration, thread priority, task runner affinity, and surface teardown gaps were discovered only after code was migrated. In the Flutter Android embedder v7 attempt, platform views and external texture implementations were deleted without CI failure because the tests were deleted alongside the code and the remaining suite was never wired into CI.

This document inventories the existing test suites exercising `shell/platform/android`, evaluates coverage across twelve critical subsystems, identifies testing seams (host unit, device unit, Robolectric, E2E), and prioritizes the top 10 gaps to be closed in tasks T-0.2 through T-0.5.

---

## 2. Existing Test Suite Inventory

### 2.1 Native C++ Unit Tests (`flutter_shell_native_unittests`)
Defined in `shell/platform/android/BUILD.gn`:
- `android_context_gl_impeller_unittests.cc`: OpenGL Impeller context initialization.
- `android_context_gl_unittests.cc`: Legacy Skia OpenGL context (only when `!slimpeller`).
- `android_image_generator_unittests.cc`: Native image decoding via `AndroidImageGenerator`.
- `android_shell_holder_unittests.cc`: ShellHolder creation, platform message pass-through, merged vs unmerged UI/Platform thread runner matching.
- `apk_asset_provider_unittests.cc`: Asset reading from APK zip archives.
- `flutter_shell_native_unittests.cc`: Test runner entrypoint and environment initialization.
- `image_lru_unittests.cc`: LRU cache eviction logic for hardware buffers/textures.
- `platform_view_android_jni_impl_unittests.cc`: JNI exception handling and boundary conversion.
- `platform_view_android_unittests.cc`: Contains only one test (`DISABLED_SelectsVulkanBasedOnApiLevel`), marked disabled.

*Observation:* Native C++ unit tests primarily cover utilities, assets, and LRU caching. Core lifecycle, threading priority, compositor coordination, and surface teardown are completely absent.

### 2.2 Java / Robolectric Unit Tests (`shell/platform/android/test/`)
Over 110 test files executed via `./gradlew test`:
- `io/flutter/embedding/android/`:
  - `FlutterViewTest.java`, `FlutterSurfaceViewTest.java`, `FlutterTextureViewTest.java`: Surface callback wiring and view detachment.
  - `FlutterActivityTest.java`, `FlutterFragmentTest.java`, `FlutterActivityAndFragmentDelegateTest.java`: Activity/Fragment lifecycle integration.
  - `KeyboardManagerTest.java`, `AndroidTouchProcessorTest.java`: Key and touch event handling.
- `io/flutter/plugin/platform/`:
  - `PlatformViewsControllerTest.java`, `PlatformViewsController2Test.java`, `PlatformViewsControllerDelegatorTest.java`: Platform view hierarchy, virtual display, texture layer, and hybrid composition coordination on the Java side.
  - `ImageReaderPlatformViewRenderTargetTest.java`, `SurfaceTexturePlatformViewRenderTargetTest.java`: Render target adapters.
- `io/flutter/view/`:
  - `AccessibilityBridgeTest.java`: TalkBack semantics and virtual view hierarchy.
  - `VsyncWaiterTest.java`: Android Choreographer frame callback coordination.
- `io/flutter/embedding/engine/`:
  - `FlutterEngineGroupTest.java`: Multiple engine lifecycle on JVM.

*Observation:* The Robolectric suite provides high-fidelity coverage of Java state machines and Android framework interactions, but cannot verify native POSIX thread properties, GPU contexts, Vulkan objects, or C++ destructor ordering.

### 2.3 Integration and Devicelab Tests
- `dev/integration_tests/android_engine_test/`:
  - `lib/platform_view/`: `virtual_display_platform_view_main.dart`, `texture_layer_hybrid_composition_platform_view_main.dart`, `hybrid_composition_platform_view_main.dart`.
  - `lib/hcpp/`: 14 tests for Hybrid Composition++ (Vulkan API 34+).
  - `lib/external_texture/`: `surface_texture_smiley_face_main.dart`, `surface_producer_smiley_face_main.dart`.
- `dev/devicelab/`:
  - `android_views`, `hybrid_android_views_integration_test`: End-to-end platform view integration.
  - `platform_views_scroll_perf__timeline_summary`, `platform_views_scroll_perf_impeller__timeline_summary`: Scroll performance benchmarks.
  - `android_lifecycles_test`, `android_choreographer_do_frame_test`, `android_semantics_integration_test`, `android_display_cutout`.

---

## 3. Subsystem Coverage Analysis (12 Subsystems)

| # | Subsystem | Verdict | Current Coverage | Key Missing Coverage | Reachable From |
|---|---|---|---|---|---|
| 1 | **Surface lifecycle** | **Partially Tested** | Java `SurfaceHolder.Callback` covered in Robolectric. | Native `NotifyCreated`, `NotifySurfaceWindowChanged`, `NotifyDestroyed`, GPU resource release under ASan, mid-frame resize, idempotency. | On-device unit / Host unit |
| 2 | **Thread creation, affinity, and priority** | **Partially Tested** | Merged vs unmerged UI/platform runners asserted in native. | Explicit `setpriority()` values (`AndroidPlatformThreadConfigSetter`), IO thread `kNormal` vs `kBackground` discrepancy, thread affinity for vsync/semantics/textures. | Host unit / Device unit |
| 3 | **Dynamic thread merging** | **Untested** | None. E2E exercises HC, but not merger state machine. | `SupportsDynamicThreadMerging()` on `AndroidExternalViewEmbedder`, 10-frame lease renewal in `PostPrerollAction`, merge/unmerge transitions. | Host unit / Device unit |
| 4 | **Platform message thread affinity** | **Partially Tested** | Java handler dispatch tested. | Native thread affinity check (`DoesHandlePlatformMessageOnPlatformThread` returning `false` vs Embedder `true`), background `TaskQueue` thread affinity. | Host unit / Device unit |
| 5 | **External texture lifetime** | **Partially Tested** | `ImageLRU` unit test; Java render targets in Robolectric; E2E smiley face. | Destruction order, GL texture detach, Vulkan backing store release, UV transform matrix preservation (`SkM44`). | Device unit / Host unit |
| 6 | **Vsync and frame pacing** | **Partially Tested** | Java `VsyncWaiterTest` in Robolectric; `android_choreographer_do_frame_test` in devicelab. | Native `VsyncWaiterAndroid` lifecycle, JNI callback registration, timebase conversions. | Device unit / Robolectric |
| 7 | **App lifecycle state transitions** | **Tested** | `FlutterActivityTest`, `FlutterFragmentTest`, devicelab `android_lifecycles_test`. | Edge cases around rapid background/foreground during active rasterization. | Robolectric / E2E |
| 8 | **Viewport metrics** | **Partially Tested** | Basic insets in Robolectric; `android_display_cutout` in devicelab. | All 30 parameters of `FlutterJNI.nativeSetViewportMetrics` arriving intact at `flutter::ViewportMetrics` (padding, gesture insets, touch slop, display features, corner radii). | Host unit / Robolectric |
| 9 | **Semantics/accessibility field coverage** | **Partially Tested** | `AccessibilityBridgeTest` in Robolectric; `android_semantics_integration_test`. | Native C++ field mapping fidelity (`maxValueLength`, `traversalParent`, `linkUrl`, `locale`, etc.). | Host unit / Robolectric |
| 10 | **Engine spawn / `FlutterEngineGroup`** | **Partially Tested** | `FlutterEngineGroupTest` in Robolectric. | Native shared graphics context, thread runner sharing, and shell spawn under ASan. | Host unit / Device unit |
| 11 | **Deferred components** | **Partially Tested** | Mock in Robolectric; integration tests in dev/integration_tests/. | Native embedder routing via `RequestDartDeferredLibrary`. | Robolectric / Device unit |
| 12 | **`DartCallbackCache` across restarts** | **Untested** | None in Android embedder test suite. | Cache serialization, file persistence, and entrypoint resolution across process restart. | Host unit / Robolectric |

---

## 4. Top 10 Ranked Test Gaps

Ranked by *(Migration Risk × Current Coverage Absence)*:

1. **Dynamic Thread Merging (`SupportsDynamicThreadMerging` & Lease Policy)**
   - *Risk:* CRITICAL (Blocker B-4). Moving HC to the Embedder API without thread merging breaks overlay synchronization and platform view rendering.
   - *Absence:* Completely untested in unit tests.
   - *Target Task:* **T-0.3** (characterization), **T-0.13** (spike), **T-1.18** (API extension).

2. **Surface Lifecycle & GPU Teardown (`NotifyDestroyed` / `NotifyCreated`)**
   - *Risk:* CRITICAL (Blocker B-2). Embedder API expects static renderer configuration; Android destroys and recreates surfaces dynamically across Activity transitions.
   - *Absence:* Untested in native C++; no ASan verification for GPU resource leaks.
   - *Target Task:* **T-0.2** (characterization), **T-0.7** (spike), **T-1.2**.

3. **Viewport Metrics 30-Parameter Fidelity**
   - *Risk:* HIGH (Parity Gap). Silently dropping parameters zeros `MediaQuery.padding`, gesture insets, touch slop, and corner radii.
   - *Absence:* 0 of 30 parameters asserted for arrival in native `ViewportMetrics`.
   - *Target Task:* **T-0.4** (characterization), **T-1.8**.

4. **Platform Message Thread Affinity & Background TaskQueues**
   - *Risk:* HIGH (Blocker B-3). Embedder API defaults to handling on platform thread (`true`), while Android returns `false`, causing subtle concurrency bugs and breaking background channels.
   - *Absence:* Untested in native C++.
   - *Target Task:* **T-0.4** (characterization), **T-0.8** (spike), **T-1.13**.

5. **Thread Creation, Priority, and Affinity (`setpriority` & IO Thread)**
   - *Risk:* HIGH. Android sets IO thread to `kNormal` while engine default is `kBackground`. Moving to engine-managed threads regresses IO priority silently.
   - *Absence:* Unasserted in native unit tests.
   - *Target Task:* **T-0.3** (characterization), **T-1.12**.

6. **External Texture Lifetime & UV Transform Matrix**
   - *Risk:* HIGH (Blocker B-5). Silently breaking `SurfaceTexture` breaks VD and video/camera plugins. Missing UV transform matrix breaks rotated/flipped textures.
   - *Absence:* Native texture destruction and matrix forwarding untested in C++.
   - *Target Task:* **T-0.14** (inventory), **T-1.9**, **T-1.10**, **T-2.10**.

7. **Semantics & Accessibility Field Coverage**
   - *Risk:* MEDIUM-HIGH. Missing fields in `FlutterSemanticsNode2` silently degrade TalkBack navigation and screen readers.
   - *Absence:* Native field mapping untested.
   - *Target Task:* **T-1.5** (Embedder API semantics field expansion).

8. **Engine Spawn & Shared Graphics Context**
   - *Risk:* MEDIUM-HIGH (Blocker B-1 interaction). Shared `AndroidContext` across spawned shells must not corrupt graphics state or double-free contexts.
   - *Absence:* Native shared context unexercised in unit tests.
   - *Target Task:* **T-0.6** (spike), **T-1.11**.

9. **`DartCallbackCache` Persistence Across Restarts**
   - *Risk:* MEDIUM. Background alarms, geofencing, and work managers rely on callback handles surviving process death.
   - *Absence:* Untested in Android embedder test suite.
   - *Target Task:* **T-1.14** (accepted gap for Stage 0; scheduled for Stage 1 decoupling).

10. **Vestigial Rendering Configurations (`kSoftware`, `kSkiaOpenGLES`)**
    - *Risk:* MEDIUM. Unnecessary complexity and maintenance burden during refactoring.
    - *Absence:* Legacy backends partially untested.
    - *Target Task:* **T-0.5** (delete vestigial rendering configuration).

---

## 5. Summary & Hand-off

The gaps identified above directly motivate the Stage 0 characterization tasks:
- **T-0.2**: Covers Gap #2 (Surface lifecycle native characterization).
- **T-0.3**: Covers Gap #1 & #5 (Dynamic thread merging & thread priority characterization).
- **T-0.4**: Covers Gap #3 & #4 (Viewport metrics & platform message thread affinity characterization).
- **T-0.5**: Covers Gap #10 (Deletion of vestigial configurations).

All tasks in Stage 0 have clear, prioritized test objectives based on this inventory.

---

## 6. Candidate Vestigial Rendering Configuration Analysis (T-0.5)

Per `MIGRATION_LEDGER.md` §T-0.5, every rendering configuration surviving into the Embedder API migration must be designed for, carried, and verified. We analyzed all four candidate configurations present in `AndroidRenderingAPI` (`shell/platform/android/android_rendering_selector.h:11`):

### 6.1 `kSoftware`
- **Selection Mechanism:** Selected in `FlutterMain::SelectedRenderingAPI` when `settings.enable_software_rendering` is true (via `--enable-software-rendering`, `FlutterEngineFlags.ENABLE_SOFTWARE_RENDERING`, or `FlutterShellArgs.ARG_ENABLE_SOFTWARE_RENDERING`).
- **Implementation:** Backed by `AndroidSurfaceSoftware` (`shell/platform/android/android_surface_software.cc`), which acquires `ANativeWindow` buffers and uses Skia software rasterization (`SkSurface::MakeRasterDirect`).
- **Reachability & Dependencies:** Used in headless Android test environments, emulator test fixtures (e.g. `FlutterActivityTestRule.java`), and Robolectric tests where GPU emulation is unavailable or flaking.
- **Decision:** **Retain.** Deleting `kSoftware` would break testing environments and violates Flutter's breaking change process for user-reachable flags. Tracked for removal when emulator test fixtures migrate fully to Vulkan/SwiftShader.

### 6.2 `kSkiaOpenGLES`
- **Selection Mechanism:** Selected in `FlutterMain::SelectedRenderingAPI` when Impeller is disabled via `--no-enable-impeller` or `AndroidManifest.xml` meta-data, or automatically when `api_level < kMinimumAndroidApiLevelForImpeller` (API < 29), or when running on Vivante GPUs (`IsVivante()`).
- **Implementation:** Backed by `AndroidContextGLSkia` and `AndroidSurfaceGLSkia`.
- **Reachability & Dependencies:** Highly reachable in production. Flutter's current minimum supported Android API level is 21 (Android 5.0 Lollipop). All devices running Android API levels 21 through 28 (Lollipop through Pie) do not enable Impeller by default and fall back directly to `kSkiaOpenGLES`.
- **Decision:** **Retain.** Deleting `kSkiaOpenGLES` immediately causes all Android devices below API 29 to fail to initialize rendering, violating Invariant I-1 (no silent fallbacks). Tracked for removal when Flutter officially raises `minSdkVersion` to 29 and concludes the Impeller opt-out deprecation period.

### 6.3 `kImpellerAutoselect`
- **Selection Mechanism:** Selected in `FlutterMain::SelectedRenderingAPI` when `settings.enable_impeller` is true, `api_level >= 29`, and `!IsVivante()`.
- **Implementation:** Backed by `AndroidContextDynamicImpeller` and `AndroidSurfaceDynamicImpeller`.
- **Reachability & Dependencies:** Default production path for modern Android devices.
- **Decision:** **Retain.** Interacts directly with Blocker B-1 (deferred graphics context resolution).

### 6.4 `kMergeAfterLaunch`
- **Selection Mechanism:** Reached via the `--merged-platform-ui-thread=mergeAfterLaunch` engine switch.
- **Implementation:** Managed through `switches.cc` and `Shell::Create`.
- **Reachability & Dependencies:** Relied upon by specific internal Flutter clients.
- **Decision:** **Retain.** Maintained on the engine-switch route per `MIGRATION_PLAN.md` §4; do not delete.

**Outcome for T-0.5:** Zero configurations deleted on this branch. Findings recorded in `DR-0017`.

