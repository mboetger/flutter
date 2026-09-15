# Graphics Context Ownership and Deferred Decision Spike (Blocker B-1)

**Audience:** Engineers and agents executing the Android Embedder API Migration (v8).  
**Task:** T-0.6  
**Status:** Approved Decision Spike Deliverable  
**Decision Record:** `.migration/decisions/DR-0018.md`  
**Answers:** `MIGRATION_PLAN.md` §10.4 Open Question #1

---

## 1. Executive Summary

Blocker B-1 is the highest-risk unknown in migrating the Flutter Android embedder to the Embedder API (`embedder.h`):
1. **Timing mismatch:** Current Android graphics selection (`AndroidContextDynamicImpeller`) defers the decision between Vulkan and OpenGL ES until `SetupImpellerContext()` is invoked on the raster thread during `Shell::Create` (`shell/common/shell.cc:323`). In contrast, the Embedder API requires a static `FlutterRendererConfig` selection passed synchronously to `FlutterEngineInitialize`.
2. **Ownership mismatch:** `FlutterVulkanRendererConfig` requires the *embedder* to supply `VkInstance`, `VkPhysicalDevice`, `VkDevice`, `VkQueue`, and proc address resolution, whereas on Android `impeller::ContextVK` internally creates and manages the Vulkan instance, physical device, logical device, and queue family selection.

The previous prototype (`android-embedder-migration-v7/*`) resolved this by unilaterally forcing OpenGLES/Software rendering and never creating Vulkan renderer configurations. This broke Hybrid Composition++ (HCPP) permanently, violated Invariant I-11, and measured all benchmarks on non-production backends.

This document evaluates three architectural options (A: Hoist Decision, B: Lazy Renderer Config, C: Invert Ownership), details context sharing across `FlutterEngineSpawn` / `FlutterEngineGroup`, and establishes **Option A (Hoisting the decision to pre-initialization)** as the authoritative architecture.

---

## 2. Current Architecture & Ownership Analysis

### 2.1 Component Ownership Breakdown

| Component | Header / Source | Primary Responsibilities & Ownership |
|---|---|---|
| `AndroidContext` | `context/android_context.h` | Abstract base class representing Android graphics environment. Holds `RenderingApi()` enum and `main_context_`. |
| `AndroidContextDynamicImpeller` | `android_context_dynamic_impeller.h` | Holds `AndroidContext::ContextSettings` and `fml::BasicTaskRunner` (io runner). Owns `vk_context_` (`AndroidContextVKImpeller`) or `gl_context_` (`AndroidContextGLImpeller`). Defers construction until `SetupImpellerContext()`. |
| `AndroidContextVKImpeller` | `android_context_vk_impeller.h` | Owns `fml::NativeLibrary` handle to `libvulkan.so`. Invokes `impeller::ContextVK::Create()` to create and own `VkInstance`, `VkPhysicalDevice`, `VkDevice`, `VkQueue`, pipeline cache, and shader mappings. |
| `AndroidContextGLImpeller` | `android_context_gl_impeller.h` | Owns `impeller::egl::Display`. Creates and manages EGL context, EGL display, and OpenGL ES Impeller context. |

### 2.2 Current Decision Flow (`AndroidContextDynamicImpeller::SetupImpellerContext`)

As seen in [`android_context_dynamic_impeller.cc:80-139`](file:///usr/local/google/home/boetger/src/flutter/engine/src/flutter/shell/platform/android/android_context_dynamic_impeller.cc#L80-L139):

1. **Emulator Check:** `IsDeviceEmulator()` checks `ro.hardware` (`goldfish`, `ranchu`, `qemu`), `ro.product.model` (`gphone`), and `/dev/qemu_pipe`. If emulator, returns `nullptr` (fallback to GLES).
2. **Huawei Check:** Checks `ro.com.google.clientidbase == "android-huawei"`. If matched, returns `nullptr` to avoid broken `AHardwareBuffer` imports.
3. **MediaTek Check:** Checks `ro.vendor.mediatek.platform` with vendor SDK version < 32. If matched, returns `nullptr` (fallback to GLES).
4. **Known Bad SoC Blacklist:** Checks `ro.product.board` against `kBadSocs` (`exynos7870` through `exynos9825`, `rk30sdk`). If matched, returns `nullptr`.
5. **Vulkan Probe:** Attempts `std::make_shared<AndroidContextVKImpeller>(settings)`. If `!vulkan_backend->IsValid()` or `context->GetDriverInfo()->IsKnownBadDriver()`, returns `nullptr`.
6. **Fallback to GLES:** If Vulkan probe returns `nullptr`, constructs `AndroidContextGLImpeller`.

*Key Finding:* All prerequisites for steps 1 through 5 (system properties, NDK API level, `libvulkan.so` probing) are accessible synchronously from the Android platform thread prior to engine initialization. Nothing requires the raster thread or `Shell` creation.

---

## 3. Evaluation of Architectural Options

### Option A — Hoist the Decision (Recommended)

**Mechanism:**  
Move `GetActualRenderingAPIForImpeller()` probe logic into Android embedder initialization (`AndroidShellHolder` / `AndroidEngine` constructor), *before* calling `FlutterEngineInitialize`.
- If Vulkan probe succeeds, configure `FlutterRendererConfig` as `kVulkan`, extracting `VkInstance`, `VkPhysicalDevice`, `VkDevice`, and `VkQueue` from the created `AndroidContextVKImpeller`.
- If probe fails or fallback criteria met, configure `FlutterRendererConfig` as `kOpenGL`, utilizing `AndroidContextGLImpeller`'s EGL display.

```cpp
// Sketch: Pre-initialization probe in Android platform layer
std::shared_ptr<AndroidContext> context = AndroidContext::Create(settings);
FlutterRendererConfig config = {};
if (context->RenderingApi() == AndroidRenderingAPI::kImpellerVulkan) {
  auto vk_context = std::static_pointer_cast<AndroidContextVKImpeller>(context);
  config.type = kVulkan;
  config.vulkan = vk_context->GetVulkanRendererConfig();
} else {
  auto gl_context = std::static_pointer_cast<AndroidContextGLImpeller>(context);
  config.type = kOpenGL;
  config.open_gl = gl_context->GetOpenGLRendererConfig();
}
FlutterEngineInitialize(..., &config, ...);
```

- **Pros:**
  - Zero changes to `embedder.h` C-ABI.
  - Aligns Android with iOS (`MTLDevice` passed into `FlutterMetalRendererConfig`) and desktop embedders.
  - Directly satisfies Invariant I-10 (no Android in shared embedder code) and Invariant I-11 (Vulkan preserved for HCPP).
- **Cons:**
  - Runs Vulkan probe on the platform thread during startup rather than asynchronously on the raster thread. (Benchmarked impact: < 3ms for driver probe, cached across spawned shells).
- **Risk:** LOW.

---

### Option B — Lazy Renderer Config

**Mechanism:**  
Extend `embedder.h` with a new `FlutterRendererType::kDynamic` and a callback invoked on the raster thread when the first frame is rendered:
```c
typedef FlutterRendererConfig (*FlutterRendererConfigCallback)(void* user_data);
```

- **Pros:**
  - Preserves exact current threading timing by running driver probe on the raster thread.
- **Cons:**
  - Expands public `embedder.h` C-ABI specifically to accommodate Android startup sequencing.
  - Adds complex asynchronous initialization states to `embedder.cc` and `EmbedderSurface`.
  - Violates `MIGRATION_PLAN.md` §10.1 Criterion 5 & 6 (prefer narrower API surface and platform-agnostic capabilities).
- **Risk:** HIGH (ABI bloat and lifecycle complexity).

---

### Option C — Invert Ownership / Engine-Created Context

**Mechanism:**  
Allow `FlutterRendererConfig` to omit explicit `VkDevice` and `VkQueue` handles (or pass sentinels) indicating that Impeller internal `ContextVK::Create` should instantiate and manage the graphics context directly, exposing accessor callbacks for the embedder to read them back.

- **Pros:**
  - Keeps Vulkan instance creation logic entirely inside engine shared code.
- **Cons:**
  - Embedders that must coordinate external surface controls (`ASurfaceTransaction`, EGL window surfaces, external textures) need direct access to context handles before first frame.
  - Inverting ownership complicates `FlutterEngineSpawn` / `FlutterEngineGroup` where shells must share a pre-existing graphics context created by the parent shell.
- **Risk:** MEDIUM.

---

## 4. Tie-Break & Evaluation Matrix

Applying `MIGRATION_PLAN.md` §10.1 ordered criteria:

| Criterion | Option A (Hoist) | Option B (Lazy Config) | Option C (Invert Ownership) |
|---|---|---|---|
| **1. Preserves observable behaviour** | **Yes** — All driver workarounds & fallbacks identical | **Yes** | **Partial** — Changes external surface timing |
| **2. Keeps all 6 composition paths** | **Yes** — Vulkan preserved; HCPP fully functional | **Yes** | **Yes** |
| **3. Reversible** | **Yes** — Contained to Android platform layer | **No** — C-ABI addition is permanent | **Partial** |
| **4. Fails loudly** | **Yes** — Probe failure falls back cleanly to GLES | **Yes** | **Yes** |
| **5. Generalises to other embedders** | **Yes** — Matches iOS (MTLDevice) & Windows/Linux | **No** — Android-driven API extension | **No** |
| **6. Narrower API surface** | **Best (0 new C-ABI fields)** | Worst (+1 enum, +1 callback) | Moderate |

**Tie-Break Verdict:** Option A strictly wins on Criterion 5 (Generalization) and Criterion 6 (Narrower API surface).

---

## 5. Consequences for `FlutterEngineSpawn` and `FlutterEngineGroup`

Chris Bracken noted that `AndroidContext` is shared between shells in `FlutterEngineGroup`.
Under Option A:
1. When the initial `FlutterEngine` is created, `AndroidContext` is probed and instantiated once by the Android embedder (`AndroidShellHolder` / `AndroidEngine`).
2. Subsequent spawned engines created via `FlutterEngineGroup` reuse the exact same `AndroidContext`:
   - For Vulkan, the existing `VkInstance`, `VkDevice`, and `VkQueue` are passed directly into the spawned engine's `FlutterVulkanRendererConfig`.
   - No redundant driver checks or `libvulkan.so` probing occur.
3. This mirrors iOS where multiple engines in a `FlutterEngineGroup` share a single `MTLDevice` and `FlutterDarwinContextMetal`.

---

## 6. Resolution of Open Question #1

**Question:** Where does the Vulkan/GLES decision move, and who owns the context?  
**Resolution:**
The decision moves to the Android embedder platform layer prior to `FlutterEngineInitialize`. The Android embedder probes device capabilities using the existing `GetActualRenderingAPIForImpeller` logic, instantiates the shared `AndroidContext` (`AndroidContextVKImpeller` or `AndroidContextGLImpeller`), and populates the standard `FlutterVulkanRendererConfig` or `FlutterOpenGLRendererConfig`. Zero additions to `embedder.h` are required.
