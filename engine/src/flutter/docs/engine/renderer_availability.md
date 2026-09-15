# Renderer Availability and Surface Lifecycle Architecture (Blocker B-2)

**Audience:** Engineers and agents executing the Android Embedder API Migration (v8).  
**Task:** T-0.7  
**Status:** Approved Architecture Spike Deliverable  
**Decision Record:** `.migration/decisions/DR-0019.md`  
**Answers:** `MIGRATION_PLAN.md` §10.4 Open Question #2

---

## 1. Executive Summary

Blocker B-2 addresses the fundamental mismatch between Android's dynamic window lifecycle and the Embedder API's static initialization model:
- The Embedder API (`embedder.h`) configures graphics rendering once at `FlutterEngineInitialize` via `FlutterRendererConfig`, with no first-class concept of an operating system surface that is created, destroyed, and recreated across application lifecycle transitions.
- On Android, `ANativeWindow` instances are transient. Whenever an Activity backgrounds, a configuration change occurs, or a `FlutterView` is detached from the window manager, the Android OS invokes `surfaceDestroyed()`. If an engine continues rendering or holds references to destroyed window surfaces after `surfaceDestroyed()` returns, the OS raises fatal faults (`SIGSEGV` or `EGL_BAD_NATIVE_WINDOW`).
- iOS encounters an analogous requirement: when an app enters the background, iOS Jetsam terminates processes that attempt to perform GPU operations (`0x8badf00d`), managed in engine internals via `Shell::SetGpuAvailability`.

This spike resolves Blocker B-2 by establishing that **renderer configuration and surface availability are orthogonal axes**, proposes a unified cross-platform API for `embedder.h`, and maps the delivery into concrete Stage 1 tasks.

---

## 2. Android Surface Lifecycle State Transitions

Ground truth verified in `shell/platform/android/platform_view_android.cc:184-234` and locked down by characterization tests in `platform_view_android_surface_lifecycle_unittests.cc` (T-0.2):

| Transition / Method | Thread Called | Engine Actions & Synchronization Contract |
|---|---|---|
| `NotifyCreated(window)` | Platform Thread | 1. Installs first frame callback.<br>2. Posts synchronous task to raster thread: `surface->SetNativeWindow(window)`.<br>3. Invokes `Shell::OnPlatformViewCreated(surface)`.<br>4. Re-enables thread merger and schedules frame on UI thread. |
| `NotifySurfaceWindowChanged(window)` | Platform Thread | 1. Posts synchronous task to raster thread: `surface->TeardownOnScreenContext()`.<br>2. Sets new `ANativeWindow` on the raster thread.<br>3. Calls `PlatformView::ScheduleFrame()`. |
| `NotifyDestroyed()` | Platform Thread | 1. Calls `Shell::OnPlatformViewDestroyed()`.<br>2. Disables raster/platform thread merger.<br>3. Flushes and tears down rasterizer surface (`rasterizer->Teardown()`).<br>4. Drains `SkiaUnrefQueue` / GPU resources on IO thread.<br>5. Synchronously destroys on-screen EGL/Vulkan context on raster thread.<br>6. **Hard Contract:** Synchronous completion before `surfaceDestroyed()` returns to the Android OS. |
| Idempotent Double `NotifyCreated` | Platform Thread | Replaces current native window handle and reschedules frame without crashing. |
| Idempotent Double `NotifyDestroyed` | Platform Thread | Second invocation is a safe no-op. |

---

## 3. iOS `SetGpuAvailability` Comparison

In the internal engine (`shell/common/shell.h:64` and `shell.cc:2434`), `Shell::SetGpuAvailability` provides three states:
- `kAvailable`: GPU operations permitted. `is_gpu_disabled_sync_switch_` is false.
- `kFlushAndMakeUnavailable`: Synchronously drains pending GPU deletions on IO runner, then sets `is_gpu_disabled_sync_switch_ = true`.
- `kUnavailable`: Blocks raster thread from submitting commands to GPU driver.

On iOS (`FlutterEngine.mm:857, 1543`):
- iOS does not destroy the `CAMetalLayer` window surface on backgrounding.
- iOS invokes `SetGpuAvailability(kFlushAndMakeUnavailable)` to avoid Jetsam terminations while backgrounded.

---

## 4. Orthogonality: Configuration vs Availability

**Finding:** The renderer configuration itself does **not** need to be replaced mid-life:
- **Renderer Configuration (Static):** The `VkInstance`, `VkPhysicalDevice`, `VkDevice`, and `VkQueue` (or `EGLDisplay`, `EGLContext`, and `MTLDevice`) persist throughout the engine's lifetime.
- **Surface Attachment (Dynamic):** The presentation targets (`ANativeWindow`, `VkSurfaceKHR`, `EGLSurface`, `CAMetalLayer`) attach and detach dynamically.
- **GPU Execution Switch (Dynamic):** Process permission to submit work to the GPU driver toggles on background/foreground transitions.

Attempting to replace the entire `FlutterRendererConfig` mid-engine lifecycle is an anti-pattern that creates unnecessary context destruction. Treating surface availability and GPU execution permissions as separate orthogonal controls cleanly resolves the requirements of both Android and iOS.

---

## 5. Proposed Shared C-ABI Surface (`embedder.h`)

To be implemented in Stage 1 without OS-specific headers (satisfying Invariants I-9 and I-10):

```c
/// GPU availability states for FlutterEngineSetGpuAvailability.
typedef enum {
  kFlutterGpuAvailabilityAvailable = 0,
  kFlutterGpuAvailabilityFlushAndMakeUnavailable = 1,
  kFlutterGpuAvailabilityUnavailable = 2,
} FlutterGpuAvailability;

/// Sets the GPU availability for the engine.
///
/// Embedders should invoke this when transitioning between foreground and
/// background states to prevent background GPU execution violations.
FLUTTER_EXPORT
FlutterEngineResult FlutterEngineSetGpuAvailability(
    FlutterEngine engine,
    FlutterGpuAvailability availability);

/// Notifies the engine that a presentation surface has been created for a view.
FLUTTER_EXPORT
FlutterEngineResult FlutterEngineNotifySurfaceCreated(
    FlutterEngine engine,
    FlutterViewId view_id);

/// Synchronously notifies the engine that the presentation surface for a view
/// is about to be destroyed.
///
/// The engine will flush pending raster work and destroy on-screen resources
/// before this function returns.
FLUTTER_EXPORT
FlutterEngineResult FlutterEngineNotifySurfaceDestroyed(
    FlutterEngine engine,
    FlutterViewId view_id);
```

---

## 6. Stage 1 Task Mapping

This spike maps directly into two discrete Stage 1 tasks:
1. **T-1.1:** Add `FlutterEngineSetGpuAvailability` to `embedder.h`, `embedder.cc`, and proc table, with host unit tests in `embedder_unittests.cc` (shared with iOS).
2. **T-1.2:** Add `FlutterEngineNotifySurfaceCreated` and `FlutterEngineNotifySurfaceDestroyed` to `embedder.h`, connecting them to `Shell::OnPlatformViewCreated` and `Shell::OnPlatformViewDestroyed` to guarantee synchronous surface teardown.

---

## 7. Resolution of Open Question #2

**Question:** Is renderer availability generalized with iOS, or handled Android-side?  
**Resolution:**  
Generalized with iOS. `FlutterEngineSetGpuAvailability` exposes the shared `Shell::SetGpuAvailability` abstraction across all platforms, while `FlutterEngineNotifySurfaceDestroyed` provides the synchronous presentation surface teardown required by Android's `surfaceDestroyed` contract.
