# Blocker B-4 Architecture: Dynamic Thread Merging C-ABI Extension

## Status & Decision Record
- **Decision:** DR-0021 — Extend the Embedder API with dynamic raster thread merging capabilities to support Hybrid Composition (HC) on Android without degrading other platforms.
- **Author:** Matt Boetger
- **Reviewers:** Chris Bracken (Engine Threading), iOS/macOS/Windows embedder owners
- **Task:** T-0.13 (Spike & C-ABI Specification), targeting implementation in T-1.18

---

## 1. Executive Summary & Problem Context

Hybrid Composition (HC) on Android delegates drawing of native `View` instances by interleaving Flutter-rendered overlay layers with Android platform views. In order to avoid race conditions, tearing, and deadlocks between Android's Main (UI) thread and the Flutter Raster thread, Flutter's engine dynamically merges the Raster task runner onto the Platform task runner while platform views are actively composed.

In the legacy engine architecture:
- `AndroidExternalViewEmbedder::SupportsDynamicThreadMerging()` returns `true`.
- When platform layers are present during `PostPrerollAction`, the embedder issues `raster_thread_merger->MergeWithLease(10)` and returns `PostPrerollResult::kSkipAndRetryFrame`.
- Once merged, `FlutterViewBeginFrame` and `FlutterViewEndFrame` JNI calls are guaranteed to run on Android's UI thread (`raster_thread_merger->IsOnPlatformThread()`).

In contrast, the Embedder API (`embedder.h`) and `EmbedderExternalViewEmbedder` currently have **no mechanism** to query, request, or coordinate raster thread merging. `SupportsDynamicThreadMerging()` is not overridden (defaulting to `false`).

Attempting to run Hybrid Composition without dynamic thread merging results in catastrophic failures:
1. **JNI Thread Affinity Crashes:** Android View operations performed during frame begin/end execute on the background raster thread instead of the Android UI thread, triggering `CalledFromWrongThreadException` or silent state corruption in the Android window manager.
2. **Deadlock / Watchdog ANR:** The platform thread attempts to process touch events or view hierarchy mutations while waiting on rasterizer locks held by the raster thread, leading to Choreographer frame drops and 5-second Application Not Responding (ANR) watchdog timeouts.
3. **Visual Desynchronization & Tearing:** Unmerged asynchronous rendering creates multiple frames of drift between native `SurfaceView` updates and Flutter overlays, producing flickering black bars and out-of-order draws.

**Conclusion:** HC cannot work through the Embedder API without dynamic thread merging. Per Decision DR-0021, the Embedder API must be extended.

---

## 2. Empirical Characterization of Lease Dynamics

To determine whether the Embedder API needs to expose frame-based lease management or a simplified boolean toggle, we characterized `RasterThreadMerger` during real-world scroll interactions with embedded platform views:

### 2.1 Steady-State Scroll Behavior
- During active scrolling with platform views visible in the viewport, every layer tree contains platform view layers (`FrameHasPlatformLayers() == true`).
- Consequently, `AndroidExternalViewEmbedder::PostPrerollAction()` calls `ExtendLeaseTo(10)` on **every single frame**.
- Each frame execution calls `RasterThreadMerger::DecrementLease()`, reducing the remaining lease count from 10 to 9, but the subsequent frame immediately resets the lease back to 10.
- **Finding:** While platform views remain visible, threads remain continuously merged. There is **zero toggle flapping** between merged and unmerged states during active scrolling.

### 2.2 View Departure and Hysteresis
- When the user scrolls past the last platform view or the view is dismissed, `FrameHasPlatformLayers()` becomes `false`.
- The embedder returns `PostPrerollResult::kSuccess` without calling `ExtendLeaseTo()`.
- For the next 10 consecutive frames, `DecrementLease()` decrements the lease: 9, 8, ..., 1, 0.
- On the 10th frame after view departure, the lease count reaches 0, `UnMergeNowIfLastOne()` executes on the raster thread, and the raster thread unmerges back to independent execution.
- **Finding:** The 10-frame lease provides essential **hysteresis**. If an app temporarily has a 1-2 frame gap without platform layers (such as during dynamic view recycling or transient animations), the threads remain merged, preventing expensive thread join/split overhead.

**Design Decision:** The C-ABI must support lease-based merging (`MergeWithLease` / `ExtendLeaseTo` with a frame count) rather than a single-frame boolean toggle, preserving this proven hysteresis mechanism.

---

## 3. Interaction with Static Merging (`--merged-platform-ui-thread`)

Flutter supports running with statically merged platform and UI/raster threads via `--merged-platform-ui-thread` or `kMergeAfterLaunch` (`switches.cc`).

In `RasterThreadMerger`:
```cpp
if (platform_id == raster_id) {
  // Statically merged: task queues are identical.
}
```
When task queues are statically merged:
- `RasterThreadMerger::MergeWithLease`, `ExtendLeaseTo`, `DecrementLease`, and `UnMergeNowIfLastOne` are all immediate no-ops.
- `IsMerged()` unconditionally returns `true`.
- `IsOnPlatformThread()` unconditionally returns `true`.

**Finding:** Dynamic thread merging composes seamlessly with static thread merging. If an embedder enables dynamic merging on an engine that is already statically merged, all dynamic lease operations are safe no-ops, maintaining complete behavioral correctness.

---

## 4. Final C-ABI Specification

The following C-ABI definitions will be added to `shell/platform/embedder/embedder.h` in Task T-1.18:

```c
/// Opaque handle to the engine-owned raster thread merger.
/// Lifetime: Valid ONLY for the duration of the callback to which it is passed.
/// Embedders MUST NOT store or retain this pointer across callback invocations.
typedef struct _FlutterRasterThreadMerger* FlutterRasterThreadMergerRef;

/// Result returned from FlutterPostPrerollCallback to guide frame scheduling.
typedef enum {
  /// The frame may proceed to rasterization normally.
  kFlutterPostPrerollResultSuccess,
  /// The frame composition changed; resubmit the frame immediately.
  kFlutterPostPrerollResultResubmitFrame,
  /// Threads were just merged; cancel current rasterization and retry on the merged thread.
  kFlutterPostPrerollResultSkipAndRetryFrame,
} FlutterPostPrerollResult;

/// Threading state provided to compositor lifecycle callbacks.
typedef struct {
  /// Size of this struct in bytes. Must be initialized to sizeof(FlutterFrameThreadingInfo).
  size_t struct_size;

  /// Handle to the raster thread merger. NULL if the engine was not initialized
  /// with dynamic thread merging support or if task runners are statically merged.
  FlutterRasterThreadMergerRef thread_merger;

  /// True if the callback is executing on the platform task runner.
  bool is_on_platform_thread;

  /// Reserved for user context pointer.
  void* user_data;
} FlutterFrameThreadingInfo;

/// Callback invoked after scene preroll to allow the embedder to inspect composition
/// layers and request thread merging if platform views are present.
typedef FlutterPostPrerollResult (*FlutterPostPrerollCallback)(
    const FlutterFrameThreadingInfo* threading_info);

/// Callback invoked before and after rasterizing a compositor frame.
typedef void (*FlutterCompositorFrameCallback)(
    const FlutterFrameThreadingInfo* threading_info);
```

### 4.1 `FlutterCompositor` Struct Extensions
Appended to `FlutterCompositor`:

```c
  /// Declares whether this compositor supports dynamic merging of the raster
  /// and platform threads. Read once during FlutterEngineInitialize.
  /// If true, post_preroll_callback must also be provided.
  bool supports_dynamic_thread_merging;

  /// Invoked after preroll to determine if thread merging is needed.
  /// Required if supports_dynamic_thread_merging is true.
  FlutterPostPrerollCallback post_preroll_callback;

  /// Invoked prior to rasterizing the frame.
  FlutterCompositorFrameCallback begin_frame_callback;

  /// Invoked after completing frame rasterization and recycling surfaces.
  FlutterCompositorFrameCallback end_frame_callback;
```

### 4.2 Engine Proc Table Additions
Appended to the proc table:

```c
/// Returns true if the raster and platform threads are currently merged.
bool FlutterRasterThreadMergerIsMerged(FlutterRasterThreadMergerRef merger);

/// Returns true if caller is executing on the platform thread.
bool FlutterRasterThreadMergerIsOnPlatformThread(FlutterRasterThreadMergerRef merger);

/// Merges the raster thread onto the platform thread with the specified lease term in frames.
void FlutterRasterThreadMergerMergeWithLease(FlutterRasterThreadMergerRef merger,
                                             size_t lease_term_frames);

/// Extends the current merged lease to the specified frame count.
void FlutterRasterThreadMergerExtendLeaseTo(FlutterRasterThreadMergerRef merger,
                                           size_t lease_term_frames);
```

### 4.3 Validation & Error Contracts (Invariant I-1)
- If an embedder provides `supports_dynamic_thread_merging = true` but leaves `post_preroll_callback = NULL`, `FlutterEngineInitialize` fails immediately with `kInvalidArguments`. No silent fallbacks.
- If `supports_dynamic_thread_merging = false` (the default via zero-initialization), `EmbedderExternalViewEmbedder` does not create a merger, preserving the exact current desktop behavior.

---

## 5. Cross-Platform Impact & Inertness

- **iOS / macOS / Windows / Linux / Fuchsia:** All existing embedders zero-initialize `FlutterCompositor`. As `supports_dynamic_thread_merging` is `false`, zero behavior changes occur on any desktop or iOS target.
- **Host Unit Test (T-1.18 Specification):**
  - Test name: `EmbedderTest.CompositorDynamicThreadMerging` in `shell/platform/embedder/tests/embedder_unittests.cc`.
  - The test sets up an in-process engine with `supports_dynamic_thread_merging = true`, passes mock `post_preroll_callback`, `begin_frame_callback`, and `end_frame_callback`, and renders 15 frames:
    - Frames 1-3: No platform views -> verify `is_merged == false`.
    - Frame 4: Returns `kFlutterPostPrerollResultSkipAndRetryFrame` with `MergeWithLease(10)`. Verify frame retries and executes merged on platform thread.
    - Frames 5-8: Calls `ExtendLeaseTo(10)`. Verify `is_merged == true` continuously.
    - Frames 9-18: No calls to `ExtendLeaseTo`. Verify `is_merged` remains true for 10 frames and unmerges on frame 19.
  - This provides automated non-Android host test coverage without requiring Android device hardware.
