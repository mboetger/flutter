# Platform Message Thread Affinity and Background TaskQueues (Blocker B-3)

## 1. Executive Summary

In Flutter, communication between Dart and the host platform occurs via platform channels.
Historically, the Embedder API assumed that all incoming platform messages should be delivered to the embedder on the platform thread (`PlatformViewEmbedder::DoesHandlePlatformMessageOnPlatformThread() == true`).
However, both the Android and iOS embedders intentionally return `false` (`PlatformMessageHandlerAndroid::DoesHandlePlatformMessageOnPlatformThread() == false`, `PlatformMessageHandlerIos::DoesHandlePlatformMessageOnPlatformThread() == false`).

This spike (Task T-0.8 / Blocker B-3) investigates the divergence, analyzes its implications for performance and the public background `TaskQueue` feature, evaluates three resolution options, and establishes the architectural path forward: **Option B (Embedder API configuration knob / extended platform message callback)**.

---

## 2. Current State and Ground Truth

### 2.1 Android Architecture
In `shell/platform/android/platform_message_handler_android.h:23`:
```cpp
bool DoesHandlePlatformMessageOnPlatformThread() const override {
  return false;
}
```
When Dart sends a platform message, the engine invokes `Shell::OnEngineHandlePlatformMessage` on the UI thread (`shell/common/shell.cc:1535`). Because `DoesHandlePlatformMessageOnPlatformThread()` returns `false`, `Shell` immediately calls `PlatformMessageHandlerAndroid::HandlePlatformMessage` directly on the UI thread without posting a task to the platform task runner.

`PlatformMessageHandlerAndroid::HandlePlatformMessage` performs a JNI call directly on the UI thread into `FlutterJNI.handlePlatformMessage` (`FlutterJNI.java:1146-1159`), which passes the message to `DartMessenger.handleMessageFromDart` (`DartMessenger.java:330`).

`DartMessenger` inspects the registered `HandlerInfo` for the channel:
- If a custom `TaskQueue` was assigned (e.g. via `BinaryMessenger.makeBackgroundTaskQueue()`), the message runnable is dispatched directly to that queue's background `ExecutorService` (`DartMessenger.java:326`).
- If no custom queue was assigned, it is dispatched to `platformTaskQueue` (which posts to Android's main looper).

**Key observation:** Under the current architecture, background channel messages **never touch the Android main (platform) thread**. They transition directly:
`Dart (UI thread) -> Engine Shell (UI thread) -> PlatformMessageHandlerAndroid (UI thread) -> JNI -> Java Background Executor`.

Ground truth characterization is locked down in `platform_message_affinity_unittests.cc` (added in T-0.4).

### 2.2 iOS Architecture
In `shell/platform/darwin/ios/platform_message_handler_ios.mm:96`:
```objc
bool PlatformMessageHandlerIos::DoesHandlePlatformMessageOnPlatformThread() const {
  return false;
}
```
In `PlatformMessageHandlerIos::HandlePlatformMessage`:
```objc
if (handler_info.task_queue) {
  [handler_info.task_queue dispatch:run_handler];
} else {
  dispatch_async(dispatch_get_main_queue(), run_handler);
}
```
iOS behaves identically to Android: incoming platform messages are inspected on the UI thread, and if a custom `task_queue` is configured, the handler block is dispatched directly to that queue without touching `dispatch_get_main_queue()`.

### 2.3 The Embedder API Discrepancy
In `shell/platform/embedder/platform_view_embedder.cc:34`:
```cpp
virtual void HandlePlatformMessage(std::unique_ptr<PlatformMessage> message) {
  platform_task_runner_->PostTask(fml::MakeCopyable(
      [parent = parent_, message = std::move(message)]() mutable {
        if (parent) {
          parent->HandlePlatformMessage(std::move(message));
        }
      }));
}

virtual bool DoesHandlePlatformMessageOnPlatformThread() const {
  return true;
}
```
In `PlatformViewEmbedder`, `EmbedderPlatformMessageHandler` unconditionally posts every platform message to `platform_task_runner_`. The public `FlutterPlatformMessageCallback` in `embedder.h:2581` is therefore always invoked on the platform task runner.

---

## 3. What `false` Buys Android and iOS

Returning `false` provides two critical architectural capabilities:

1. **Zero Main-Thread Contention for Background Channels:**
   Flutter plugins that process high-bandwidth or computationally heavy data (such as image processing, camera frames, SQLite queries, network responses, and cryptography) register background task queues via `BinaryMessenger.makeBackgroundTaskQueue()`. Because `DoesHandlePlatformMessageOnPlatformThread()` is `false`, this traffic never enters the Android main Looper or iOS main runloop. The main thread remains dedicated to UI rendering, input handling, and view animations.

2. **Halved Latency and No Head-of-Line Blocking:**
   Hopping from UI thread -> Platform thread -> Background worker adds an extra event loop dispatch cycle and thread context switch. Worse, if the platform main thread is busy (executing Android view layout, measure passes, inflating views, or responding to OS callbacks), background channel execution is stalled behind the main thread work, defeating the entire purpose of offloading work to a background thread.

### What Would Break if Android Naively Conformed to `true`
If Android conformed to `PlatformViewEmbedder`'s `true`:
- Every background channel invocation would hop through the Android main looper.
- Heavy background channel traffic would induce frame drops (jank) on the main thread.
- Plugins relying on asynchronous background queues for fast responsiveness would suffer latency spikes.
- This constitutes a clear violation of **Criterion 1 (Preserve observable behavior exactly)**.

---

## 4. Evaluation of Resolution Options

| Criterion | Option A: Android conforms to `true` | Option B: Embedder API configuration knob | Option C: API default changes to `false` |
|---|---|---|---|
| **1. Preserves observable behavior** | **FAIL** — degrades background task queues; hops through main thread | **PASS** — preserves existing Android & iOS dispatch model exactly | **PASS** — preserves Android & iOS dispatch model |
| **2. Keeps composition modes working** | Neutral | Neutral | Neutral |
| **3. Reversible** | Hard (re-architects Java & native channel routing) | **PASS** — clean configuration flag | Hard (alters desktop assumptions) |
| **4. Fails loudly rather than degrading** | **FAIL** — silently adds main-thread latency and jank | **PASS** — explicit opt-in | **FAIL** — desktop embedders break silently if expecting main thread |
| **5. Generalizes to other embedders** | No | **PASS** — directly reusable by iOS (`FlutterEmbedderAPIBridge`) | No |
| **6. Narrower API surface / smaller diff** | 0 new API | Minimal additive field in `FlutterCustomTaskRunners` / `FlutterProjectArgs` | 0 new API (but alters shared semantics) |

### Option A — Android conforms to `true`
**Verdict: REJECTED.**
Forcing all platform messages onto the platform thread before dispatching to Java breaks the design contract of `BinaryMessenger.TaskQueue`. It degrades real-world app performance and alters thread affinity observable by tests.

### Option C — Change Embedder API default to `false`
**Verdict: REJECTED.**
Existing desktop embedders (Linux GTK, Windows Win32, macOS) and embedded systems embedders assume that `platform_message_callback` is called on the platform thread. Changing the default behavior globally would break thread safety in existing embedders that do not thread-hop in their callback.

### Option B — Embedder API configuration knob / extended handler
**Verdict: RECOMMENDED.**
The Embedder API should allow embedders to declare their platform message handling capabilities during engine initialization or custom task runner setup.
Specifically, by declaring that the embedder handles platform messages directly from calling threads (or UI thread), `PlatformViewEmbedder` configures its `PlatformMessageHandler` to return `false` and invokes the callback directly on the thread of arrival.

---

## 5. Proposed C-ABI Specification (Stage 1 / Task T-1.13)

### 5.1 Callback Signature
To allow embedders to receive messages directly on the engine thread and dispatch appropriately, `FlutterProjectArgs` is extended:

```c
/// Callback invoked when the engine delivers a platform message from Dart.
/// When does_handle_platform_messages_on_platform_thread is false, this
/// callback may be invoked from any thread (typically the engine UI thread).
/// Embedders are responsible for dispatching to the appropriate task runner
/// or background worker pool.
typedef void (*FlutterPlatformMessageCallback2)(
    const FlutterPlatformMessage* message,
    void* user_data);
```

### 5.2 Extension to `FlutterCustomTaskRunners` or `FlutterProjectArgs`
Appended to `FlutterProjectArgs`:

```c
  /// When true (or zero-initialized default for legacy embedders), the engine
  /// routes all platform messages to the platform task runner before invoking
  /// platform_message_callback.
  ///
  /// When false, the engine invokes platform_message_callback2 (or
  /// platform_message_callback) directly on the originating thread
  /// (typically the UI task runner), allowing the embedder to route
  /// directly to background worker queues without platform thread hopping.
  bool does_handle_platform_messages_on_platform_thread;
```

### 5.3 `PlatformViewEmbedder` Implementation Mechanics
When `does_handle_platform_messages_on_platform_thread` is `false`:
1. `EmbedderPlatformMessageHandler::DoesHandlePlatformMessageOnPlatformThread()` returns `false`.
2. `EmbedderPlatformMessageHandler::HandlePlatformMessage()` directly calls the embedder callback without posting to `platform_task_runner_`.
3. The embedder (Android / iOS) inspects the channel, matches the channel to its registered `TaskQueue`, and dispatches either to the platform main thread or the background executor.

---

## 6. Interaction with `BinaryMessenger.TaskQueue`

On Android:
`DartMessenger` maintains a mapping of channel names to `HandlerInfo`.
When a message arrives on the UI thread via `FlutterPlatformMessageCallback2`:
1. The Android embedder JNI wrapper reads the channel string.
2. It looks up the `HandlerInfo` in `DartMessenger`.
3. If `handlerInfo.taskQueue` is a background queue, the task is dispatched directly to that queue's executor pool.
4. If not, the task is dispatched to `platformTaskQueue` (Android main Looper).
5. When the handler completes its reply, `FlutterEngineSendPlatformMessageResponse` is called from whatever thread executed the handler. `FlutterEngineSendPlatformMessageResponse` is already thread-safe.

This completely preserves existing Android behavior, eliminates all extra thread hops, and keeps background channel tasks completely off the main thread.

---

## 7. Cross-Platform Alignment with iOS

iOS (`FlutterEmbedderAPIBridge`) faces the exact same challenge:
`PlatformMessageHandlerIos` dispatches to `task_queue` or `dispatch_get_main_queue()`.
By providing this knob in `embedder.h`, both Android and iOS can share the identical mechanism in Stage 1 without needing platform-specific hacks.

Per §D.9, an entry is logged in `.migration/HUMAN_REVIEW_REQUIRED.md` for iOS owner sign-off.
