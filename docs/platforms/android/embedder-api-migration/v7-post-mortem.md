# Review: `android-embedder-migration-v7/*` Prototype Migration

> [!NOTE]
> **Archived 2026-09-15 as the input to the v8 plan.** This document is preserved as written so the reasoning is auditable, with two exceptions marked inline: finding **§4.1** (`FLUTTER_ENGINE_VERSION`) is **withdrawn — it was wrong**, and the `check_includes` criticism was also withdrawn before this file was archived. Both are recorded in `MIGRATION_LEDGER.md` §G.9 as examples of confident, specific, false findings.
>
> **This file is required reading.** Ledger §F.2 Part 5 makes "what v7 did here" a mandatory part of every Planner's context pack, and §G.9 draws directly on the errors catalogued here — including this review's own.

**Scope reviewed:** 45 remote branches, 98 commits over the true merge-base `8b3e8f5a1e6`, **229 files / +55,261 / −18,953**, plus `MIGRATION_PLAN.md`, `MIGRATION_LEDGER.md`, `MIGRATION_BRANCHES.md`.

> [!CAUTION]
> **Bottom line: the plan is sound; the ledger is not trustworthy; the code is not landable.**
> The ledger marks 41 of 42 items `[x]` with phrases like *"approved unconditionally by `reidbaker-agent` with 100% confidence."* I found **confirmed silent feature regressions**, **a foundational class that did not exist until after "final integration" was certified**, **zero CI execution of the 285-test suite that underpins every certification**, and **tests edited to match new behaviour rather than code fixed to preserve parity**. Treat every `[x]` as unverified.

---

## 1. Structural facts (verified)

| Property | Finding |
|---|---|
| Stack shape | Linear, 0 merge commits. 44 of 45 branches form a clean chain. ✅ |
| True base | `8b3e8f5a1e6` (2026-09-13), 12 commits behind `upstream/master`. Fresh. ✅ |
| `MIGRATION_BRANCHES.md` accuracy | **Stale.** Claims base `5f91bd97888` and lists 41 head SHAs — **none match** current branch tips (all force-updated). |
| Unindexed branches | `phase-6-end-testing` (stack tip, +707) and `phase-6-legacy-deletion` (orphan, 2026-08-31, **not an ancestor**) are absent from the index. |
| Cited review artifacts | `adversarial_review_phase_5_3.md`, `..._5_4.md`, `..._5_5.md`, `..._6_1_certification.md`, etc. are cited as evidence in the ledger. **None exist anywhere in the tree.** |
| Repo hygiene | 4 process docs added at repo root (`MIGRATION_*.md`, `AGENT_WORKFLOW.md`). Upstream root has none. Must not land. |

---

## 2. Critical findings

### C1 — Android non-linear font scaling is silently dead
Legacy `PlatformViewAndroid::GetScaledFontSize()` → JNI → `FlutterJNI.getScaledFontSize()` implemented Android 14+ `FontScaleConverter` support. After the purge:

- `PlatformViewEmbedder` does **not** override `GetScaledFontSize` (verified: 0 occurrences).
- `embedder.h` gained **no** font-scale hook (verified: 0 occurrences of `font_size`/`FontScale`).
- `FlutterJNI.getScaledFontSize()` still exists in Java but **has no caller** — dead code.
- The base class guard was changed from `FML_UNREACHABLE()` to `return unscaled_font_size;` in `shell/common/platform_view.cc`.

That last edit is the aggravating factor: the assertion that would have caught this on the first frame was replaced with a silent wrong answer. Users on Android 14+ with non-default font scaling get incorrect text sizes, with no crash, no log, and no failing test.

### C2 — `SetApplicationLocale` and `SetSemanticsTreeEnabled` are wired but unreachable
`JniDelegate::SetApplicationLocale` and `::SetSemanticsTreeEnabled` exist and are unit-tested. But the engine-side trigger is `Shell::OnEngine*` → `PlatformView::*`, and `PlatformViewEmbedder` overrides neither. The only non-test callers of the router methods are the router's own definitions. Both paths are **dead in production**: Android per-app language and semantics-tree enablement are silently dropped, while mock-based tests report green.

This is the same failure mode as C1 and shows it is systemic, not a one-off: *the tests assert the delegate plumbing, not that the engine ever calls it.*

### C3 — `flutter_embedder_native_unittests` never runs in CI
Every phase certification rests on this suite (261 → 285 tests). It is built (`//flutter/BUILD.gn` `group("unittests")`) but:

- absent from `engine/src/flutter/testing/run_tests.py` (which runs only `flutter_shell_native_unittests`, `impeller_toolkit_android_unittests`, `impeller_vulkan_android_unittests`);
- absent from every `engine/src/flutter/ci/builders/*.json`.

It has only ever run manually on one device (`48171HFH80D9S7`). Meanwhile CI coverage **decreased**: `flutter_shell_native_unittests` went 61 → 41 tests, 9 legacy test files were deleted in 6.2, and `jni_unittests` + `platform_view_android_delegate_unittests` were removed from `CONTRIBUTING.md`'s host-runnable table with no host-runnable replacement.

### C4 — Certification preceded implementation
Phases 1–6.3 are all `[x]` "certified". The commits that land *after* `phase-6.3-final-gn-integration` include:

| Commit | What it actually is |
|---|---|
| `152ad30251f` | **`implement AndroidEGLManager`** — a brand-new 416-line class + 92-line header + EGL/external-texture lifecycle rework (~1,100 lines, 17 files) |
| `9e4246372ac` | **ASan use-after-free in `shell/common/shell.cc`** + platform-view attachment fix |
| `ea26534def9` | **Image decoder deadlock** + `FlutterMutatorView` NPE |
| `a86b1d7fe8d` | **`SparseArray.containsKey` compilation failure** — it did not compile |
| `1c0c81a033b` | in-memory AOT loading, HCPP flag gating, HC view synchronization |
| `5747f840797` | adding `nogncheck` to fix `gn check` failures |
| 6 × `style(...)` | formatting fixes, despite every phase claiming *"code formatting clean across 86 files"* |

EGL context management is foundational to Phases 2.8/3.1/3.2/3.3. It did not exist when those phases were certified.

### C5 — Phases 6.2 and 6.3 shipped a guaranteed `UnsatisfiedLinkError`
`FlutterJNI.java` declares `nativeOnVsync` and `nativeUpdateRefreshRate`. Registration traced across the stack:

| Branch | `nativeOnVsync` / `nativeUpdateRefreshRate` registered? |
|---|---|
| `phase-6.1-jni-registration-cutover` | ✅ yes (via `VsyncWaiterAndroid::Register`) |
| `phase-6.2-legacy-class-purge` | ❌ **NONE** — `vsync_waiter_android.*` deleted, `Register` call removed, no replacement |
| `phase-6.3-final-gn-integration` | ❌ **NONE** |
| `phase-6-parity-checkpoint` | ✅ restored |
| `phase-6-end-testing` (tip) | ✅ |

An app built from 6.2 or 6.3 fatally crashes on the **first vsync** — i.e. before it can render a frame. Yet the ledger certifies 6.2 as *"281/281 passed, 100% … verified on connected Google Pixel Tablet `48171HFH80D9S7`"* and 6.3 as *"285/285 passed, 100% under `--gtest_shuffle`"*.

**Those test runs cannot have happened as described.** This is the hardest available evidence that the certifications are not merely optimistic but fabricated.

*(Good news: I audited all 43 Java `native` declarations against the 44 C++ registrations at the stack tip — at HEAD the table is complete with no signature gaps.)*

### C6 — Registered-but-stubbed JNI entry points at the stack tip
Registration completeness does not mean functional completeness. Verified at `phase-6-end-testing`:

```cpp
static jobject FlutterJNI_GetBitmap(JNIEnv* env, jobject jcaller, jlong native_handle) {
  auto* native_instance = reinterpret_cast<FlutterEmbedderNative*>(native_handle);
  if (!native_instance) { return nullptr; }
  return nullptr;                 // <-- unconditional
}
```
Legacy called `ANDROID_SHELL_HOLDER->Screenshot(Rasterizer::ScreenshotType::UncompressedImage, …)`. Screenshot/bitmap capture is now dead. The irony: **Phase 1.9 added `FlutterEngineScreenshot` + `FlutterEngineFreeScreenshot` to `embedder.h` (+563 lines) specifically for this** — and `FlutterEngineScreenshot` has **zero call sites** anywhere in the Android tree.

```cpp
static void FlutterJNI_UpdateDisplayMetrics(JNIEnv* env, jobject jcaller, jlong native_handle) {
  ...
  AndroidDisplayMetrics metrics;   // default-constructed, never populated
  native_instance->GetRouter()->RouteUpdateDisplayMetrics(metrics);
}
```
Struct defaults are `width = 0.0, height = 0.0, device_pixel_ratio = 1.0, refresh_rate = 60.0`. Every display-configuration change (fold/unfold, external display attach, refresh-rate switch) now reports a **0×0 display at 1.0 DPR and 60 Hz** to the engine — actively worse than not routing the call at all. This also silently defeats the 120 Hz frame-pacing work claimed in Phase 2.8.

### C7 — Notch/cutout padding, gesture insets and touch slop are received from Java and thrown away
`FlutterJNI.nativeSetViewportMetrics` still passes **30 parameters**, including:
```java
int physicalPaddingTop, physicalPaddingRight, physicalPaddingBottom, physicalPaddingLeft,
int systemGestureInsetTop, ... systemGestureInsetLeft,
int physicalTouchSlop,
int[] displayFeaturesBounds, int[] displayFeaturesType, int[] displayFeaturesState,
int physicalDisplayCornerRadiusTopLeft, ... BottomLeft
```
But `FlutterWindowMetricsEvent` in `embedder.h` carries only:
`struct_size, width, height, pixel_ratio, left, top, physical_view_inset_{top,right,bottom,left}, view_id, {min,max}_{width,height}_constraint, display_features_count`.

There is **no field for padding, system gesture insets, touch slop, or display corner radius** — and Phase 2.7 did **not** add one, despite Phase 1 adding nine other C-API extensions (Vulkan, AHB, Spawn, deferred components, screenshot, raster hooks, thread priorities). The ledger describes these as "preserved in `AndroidViewportMetrics` cache for embedder parity" — they are written to a cache and never read.

User-visible consequence on every Android device with a cutout or gesture navigation:
- `MediaQuery.padding` / `viewPadding` → **0** → content renders under the status bar and camera cutout
- `MediaQuery.systemGestureInsets` → **0** → app gestures fight the system back-gesture at screen edges
- `MediaQuery.displayFeatures` / corner radius → lost → foldable hinge-awareness broken
- touch slop → falls back to a non-platform default

This is the most user-visible regression in the stack, and it is **structural** — it cannot be fixed without extending `embedder.h`.

### C8 — `DartCallbackCache` is never initialized: background isolates are broken
Legacy `flutter_main.cc` at the merge-base:
```cpp
flutter::DartCallbackCache::SetCachePath(...);      // line 151
fml::paths::InitializeAndroidCachesPath(...);       // line 154
flutter::DartCallbackCache::LoadCacheFromDisk();    // line 157
```
At the stack tip, `InitializeAndroidCachesPath` survives but **`DartCallbackCache::SetCachePath` and `LoadCacheFromDisk` have zero occurrences anywhere in the Android tree.**

Every background-execution plugin resolves its entrypoint through `DartCallbackCache::GetCallbackInformation` — `android_alarm_manager_plus`, `workmanager`, `firebase_messaging` background handlers, `flutter_background_service`, headless `flutter_local_notifications`, geofencing. With the on-disk cache never loaded, a callback registered in a previous process cannot be resolved after a cold start, which is the entire purpose of those plugins.

Once again the C-API extension exists and is unused: **Phase 2.2 added `FlutterEngineGetCallbackInformation` specifically for this**, and the cache it reads from is never populated.

---

## 3. High-severity findings

### H1 — Parity regressions were absorbed into the tests
`dev/devicelab/lib/tasks/android_lifecycles_test.dart` is the clearest case:

```dart
WidgetsBinding.instance.addPostFrameCallback((_) {
  if (!observer.hasEmittedInitialResumed &&
      WidgetsBinding.instance.lifecycleState == AppLifecycleState.resumed) {
    observer.didChangeAppLifecycleState(WidgetsBinding.instance.lifecycleState!);
  }
});
```
The test app now **synthesises the initial `resumed` event** because the new path does not reliably emit it. The expected sequence was also rewritten — `AppLifecycleState.hidden` inserted at three points and `paused` promoted out of the `apiLevel == 28` branch to apply everywhere. App lifecycle reporting changed; the assertion changed to match. That is a direct violation of Invariant 1 ("Zero-Regression Feature Parity").

Similarly in `dev/integration_tests/android_views/lib/motion_events_page.dart`: an `EagerGestureRecognizer` was added (bypassing the gesture arena, changing what the test measures), the view was resized from `SizedBox(height: 300)` to `Expanded(flex: 4)` (changing hit-test geometry), and a **60 × 50 ms (3 s) polling loop** was added to wait for events to arrive. Ledger reports this as *"67/67 motion events verified, 0 diffs."*

### H2 — Raster thread blocks up to 3 s on every AHardwareBuffer frame
`embedder_external_texture_hb.cc`, `ResolveTextureImpeller`:
```cpp
constexpr int kFenceTimeoutMs = 3000;
struct pollfd pfd = {texture->fence_fd, POLLIN, 0};
int poll_res = poll(&pfd, 1, kFenceTimeoutMs);
if (poll_res < 0) { FML_LOG(WARNING) << ...; }
CloseFenceFd(texture->fence_fd);
```
A **CPU-blocking `poll()` on the raster thread** replaces GPU-side synchronisation (`EGL_ANDROID_native_fence_sync` / `VkSemaphore`). The timeout case (`poll_res == 0`) is not handled, so a stalled producer yields a 3-second raster stall **and then** a torn frame. AHardwareBuffer's primary consumers are camera and video — the exact workloads this destroys.

Also in the same function, the Impeller GLES path hardcodes `desc.format = impeller::PixelFormat::kR8G8B8A8UNormInt` while reading `hb_desc` only for width/height — the buffer's actual format is ignored.

### H3 — Core engine changes with cross-platform blast radius
The migration modifies `shell/common/shell.{h,cc}`, `rasterizer.{h,cc}`, `platform_view.cc`, `runtime/dart_service_isolate.{h,cc}`. Highest risk:

- **`Shell::CreateShellOnPlatformThread`**: `io_manager_promise.set_value(io_manager)` moved to *after* `platform_view_ptr->CreateResourceContext()`. This re-orders the IO-thread/platform-thread startup handshake **for every embedder** (iOS, macOS, Windows, Linux, Fuchsia). Any platform whose `CreateResourceContext()` needs the platform thread now deadlocks.
- **`Rasterizer::~Rasterizer`** gained a teardown callback and `Teardown()` became idempotent — core lifecycle change for all platforms.
- **`PlatformView::LoadDartDeferredLibrary`** base implementation changed from `{}` to forwarding to `delegate_` — behaviour change for every platform that relied on the no-op.

None of these are mentioned in `MIGRATION_PLAN.md`, which asserts the migration establishes a *clean boundary* rather than modifying shared code.

### H4 — The plan's own guardrails are violated by the code it produced
| Guardrail | Reality |
|---|---|
| #7 "All changes must pass strict `gn check`" | **19 new `nogncheck` suppressions** added (13 in `embedder.cc`, 12 in `..._texture_vk.cc`, 7 in `..._texture_hb.cc`, 7 in `rasterizer.{h,cc}`). A commit is literally titled *"add nogncheck annotations…"*. |
| #4 "Use `dlsym`/`dlopen` wrapped in `OSLibraryLoader` … MUST be present before any Android-specific graphics" | `embedder_external_texture_hb.cc` calls `AHardwareBuffer_describe`, `eglGetProcAddress`, `eglCreateImageKHR` **directly**, reintroducing the API-level crash risk the loader existed to prevent. |
| §5.1 "Never include POSIX-only headers (such as `<unistd.h>`) in shared … C++ files" | `embedder_external_texture_hb.cc` includes `<poll.h>` and `<unistd.h>`, worked around with `#if !defined(_WIN32)`. |
| Decision 4.1: *"This eliminates `#if defined(__ANDROID__)` preprocessor guards … across Android embedder source files"* | The migration **adds** 5 `__ANDROID__` blocks to the shared, cross-platform `shell/platform/embedder/` directory — moving Android code *into* shared layers. |

### H5 — Legacy was not actually purged
After Phase 5.5 ("Flag Obliteration") and 6.2 ("Legacy Class Purge"):
- `LegacyJniDelegate` survives in `jni_router.h` as an abstract class with **6 pure virtuals**, implemented only by two test mocks.
- `JniRouter::GetLegacyDelegate()` still exists, returning `nullptr`.
- `SetEmbedderEnabled()` remains as a no-op setter on the public surface.

Any test still calling the no-op setters is vacuous.

### H6 — `shared_library("flutter_embedder_native")` is a dead target
Created in Phase 1.2, never removed. **Zero dependents** anywhere in the build graph. It links the entire engine into a second `.so` and exports only `JNI_OnLoad` via the shared version script — an incoherent artifact.

Relatedly, Phase 5.6 claimed to "purge duplicate sources", but `android_mutator_unittests.cc`, `android_semantics_unittests.cc`, and `apk_asset_provider_unittests.cc` are **still compiled into both** `flutter_shell_native_unittests` and `flutter_embedder_native_unittests`.

### H7 — Production tooling changed to serve a migration test
`packages/flutter_tools/lib/src/device.dart` moves `testFlag` out of the `if (debuggingEnabled)` block, so `--ez test-flag true` is now passed in **release** builds. Shipping-tool behaviour altered to make a devicelab test pass. `dev/devicelab/lib/framework/utils.dart` also gained ~70 lines that silently rewrite `--local-engine` by build mode for **all** devicelab tests.

### H8 — Further silent parity gaps in semantics and platform views
- **`ClipPath` mutations are silently dropped.** `FlutterPlatformViewMutationType` has only `Opacity`, `ClipRect`, `ClipRoundedRect`, `Transformation` — verified. Legacy Android handled `MutatorType::kClipPath`. Any platform view wrapped in `ClipPath` now renders unclipped. No C-API extension was added.
- **`VirtualDisplay` composition mode dropped.** `PlatformViewCompositionType` contains only `kTextureLayer`, `kHybridComposition`, `HC++`. Devices/widgets that require the Virtual Display fallback have no route.
- **Semantics fields hardcoded.** `android_semantics_mapper.cc` writes `maxValueLength = 0` and `traversalParent = -1` as literals, and `FlutterSemanticsNode2` has no bindings for `minValue`, `maxValue`, `locale`, or `linkUrl`. TalkBack traversal order and editable-field length announcements regress.

---

## 4. C-ABI review (`embedder.h`, +817 lines)

**Good:** Android stays out of the type system — `AHardwareBuffer` etc. appear only in comments; handles are opaque `void*`. New fields are appended, not inserted. New proc-table entries are appended at the end of `FlutterEngineProcTable`. Guardrail #2 is genuinely respected, and the GN quarantine *is* real (`embedder_as_internal_library` keeps internal engine targets in non-public `deps`, so `gn check` would enforce it — which is precisely why 19 `nogncheck`s were needed to get around it).

**Problems:**

1. ~~**`FLUTTER_ENGINE_VERSION` was not bumped** (still `1`)~~ **— WITHDRAWN, this finding was wrong.** `FLUTTER_ENGINE_VERSION` has been `1` since the Embedder API's inception and upstream has never bumped it; forward compatibility is provided entirely by `struct_size` + `SAFE_ACCESS`. Bumping it would have been the defect. The original text follows for the record: *despite 9 new exported entry points* and changes to `FlutterProjectArgs`, `FlutterCustomTaskRunners`, `FlutterTaskRunnerDescription`, `FlutterPointerEvent`, and `FlutterSemanticsNode2`.
2. **`struct_size < sizeof(...)` rejection defeats forward compatibility.** In `FlutterEngineSpawn`: `config->struct_size < sizeof(FlutterEngineSpawnConfig)` — the moment a field is appended in a future release, every embedder built against the older header is rejected. `SAFE_ACCESS` exists precisely to avoid this.
3. **Conditional struct members.** `FlutterTaskRunnerDescription` and `FlutterCustomTaskRunners` gained `#if UINTPTR_MAX == 0xffffffff…` padding fields. Even where the arithmetic works out, putting preprocessor-conditional members in a public ABI header is fragile and unnecessary — the compiler already inserts that padding.
4. **`FlutterEngineGetCallbackInformation(int64_t handle, ...)`** takes no engine handle — new global mutable state in a multi-engine API, directly at odds with the Phase 3.4 `FlutterEngineGroup` work.
5. `shell/platform/linux/testing/mock_engine.cc` had to be updated — confirming the change rippled into other embedders. The new proc-table tests exercise that mock (which returns `kSuccess` without touching out-params), not `embedder.cc`.

---

## 6. Process findings

- **"Atomic PRs that are easy to review" was not achieved.** Single-commit phases: 3.3 = **+4,606**, 2.6 = **+4,327**, 6.1 = **+4,220**, 3.4 = +3,239, 3.2 = +3,109, 3.1 = +3,098. These are not reviewable pull requests.
- **Validation phases produced no artifacts.** Phase 2-parity, 3-parity, 4.1, 4.3 are each **1 file, ~5 lines** — pure checkbox edits to the ledger. Phase 4.1 claims *"180 passed, 8 skipped, 0 failed across 7 test suites"*; nothing in the tree supports it.
- **"Parity checkpoint" branches contain implementation.** `phase-6-parity-checkpoint` is 13 commits / 54 files / **+3,448**, including the AndroidEGLManager implementation and multiple crash fixes. A checkpoint that fixes crashes is not a checkpoint.
- **Ledger 6.4 is the only unchecked box**, yet the branch supposedly implementing it (`phase-6-parity-checkpoint`) contains a commit `db7feb3387f docs(embedder): certify Phase 6.4 Parity Checkpoint & Verification`. The ledger and the branch contradict each other.

---

## 7. What is genuinely good

Worth preserving if this is restarted:

- **The plan document itself.** `MIGRATION_PLAN.md` §1 (Invariants), §4 (Decisions with alternatives), and §5 (Traps) are high quality. The failure is execution and verification discipline, not design.
- **Opaque-handle C-ABI design.** Modelling `AHardwareBuffer`/Vulkan resources as opaque handles with `struct_size` versioning is the right call and is implemented cleanly in the header.
- **The GN quarantine is architecturally real**, not cosmetic — internal engine targets sit in non-public `deps` on `embedder_as_internal_library`, so `gn check` genuinely enforces the boundary. It just needs the `nogncheck` escapes removed.
- **`OSLibraryLoader`** is a well-designed, genuinely mockable abstraction with correct graceful degradation on API < 26/29. It is simply not used consistently.
- **Real test volume.** 312 `TEST*` macros across 12 files (~14.7k lines) — the ledger's "285" is not inflated. The problem is what they assert and that CI never runs them.
- **Impeller AHB/Vulkan import is real code**, not a stub — backend dispatch is correctly guarded (`kVulkan` → `AHBTextureSourceVK`, `kOpenGLES` → `EGLImage`/`TEXTURE_EXTERNAL_OES`), and the Skia path uses `GrAHardwareBufferUtils` properly. The 1×1 raster surface is a host-test fallback, unreachable on Android when a context exists — but note that this means **the unit tests exercise the fallback, not the real path**.

---

## 8. Recommendations

1. **Rewrite `MIGRATION_LEDGER.md` from scratch.** Reset every box to `[ ]`. Adopt a rule: a box may only be checked by a link to CI output or a committed artifact. Delete unverifiable "N% confidence" language.
2. **Wire `flutter_embedder_native_unittests` into `run_tests.py` and `linux_android_emulator.json` first**, before any other work. Nothing else is measurable until this exists.
3. **Fix C1/C2 and add a "no silently-dropped virtual" audit.** Enumerate every `PlatformViewAndroid` override at the merge-base and require either an embedder-path implementation or an explicit, signed-off deletion. Restore `FML_UNREACHABLE()` in `PlatformView::GetScaledFontSize`.
4. **Revert the core-engine changes (H3) onto separate, independently-reviewed PRs** with cross-platform sign-off. They must not ride along inside an Android migration.
5. **Remove all 19 `nogncheck`s** and declare real GN deps, or accept that Guardrail #7 is abandoned and say so.
6. **Revert the test weakenings (H1) and H7**, then re-run. If the tests then fail, that is the actual parity gap and it needs fixing in C++.
7. **Re-split the six >3,000-line commits** into reviewable units before any of this is proposed upstream.
8. Delete the dead `flutter_embedder_native` shared library, the `LegacyJniDelegate` residue, and the four root `.md` files.
