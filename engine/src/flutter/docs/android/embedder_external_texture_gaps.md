# Blocker B-5 Architecture: Android External Texture Inventory & Embedder API Gaps

## Status & Decision Record
- **Decision:** DR-0022 — Extend the Embedder API with 4x4 UV transformation matrix support and Vulkan external texture callbacks to preserve Android Virtual Display (VD), Texture Layer Hybrid Composition (TLHC), and plugin video/camera rendering.
- **Author:** Matt Boetger
- **Reviewers:** Android Graphics Team, Engine Architecture
- **Task:** T-0.14 (Spike & Gap Inventory), seeding Stage 1 tasks T-1.19, T-1.20, and T-1.21

---

## 1. Executive Summary & Context

In Flutter on Android, external textures serve two critical rendering responsibilities:
1. **Platform View Composition:** Both Virtual Display (VD) and Texture Layer Hybrid Composition (TLHC) render Android views into an offscreen buffer that is sampled as an external texture during Flutter scene composition.
2. **Plugin Ecosystem Media Rendering:** Public plugin APIs (`TextureRegistry.createSurfaceTexture()`, `createSurfaceProducer()`, and `createImageTexture()`) allow hardware-accelerated video players (`video_player_android`), camera previews (`camera_android`), maps (`google_maps_flutter_android`), and webviews (`webview_flutter_android`) to render directly into Flutter without CPU copies.

In the v7 attempt, both external texture implementations were deleted and replaced with a dummy `std::map` insert in `RegisterSurfaceTexture` that never created an engine texture. As a result, VD, TLHC, and all plugin video/camera streams were silently broken without failing tests because the test suite was simultaneously deleted.

This inventory audits all six native Android external texture implementations against the existing Embedder API (`embedder.h`), identifies missing C-ABI constructs, documents Java-side public API requirements, and defines the remediation tasks for Stage 1.

---

## 2. Technical Audit of the Six Implementations

| Implementation Class | Source File | Rendering API | Buffer Source | Engine Texture Representation |
|---|---|---|---|---|
| `SurfaceTextureExternalTextureGLSkia` | `surface_texture_external_texture_gl_skia.cc` | OpenGLES (Skia) | `android.graphics.SurfaceTexture` | `GrBackendTexture` (`GL_TEXTURE_EXTERNAL_OES`) |
| `SurfaceTextureExternalTextureGLImpeller` | `surface_texture_external_texture_gl_impeller.cc` | OpenGLES (Impeller) | `android.graphics.SurfaceTexture` | `impeller::TextureGLES` (`kTextureExternalOES`) |
| `SurfaceTextureExternalTextureVKImpeller` | `surface_texture_external_texture_vk_impeller.cc` | Vulkan (Impeller) | `android.graphics.SurfaceTexture` | `glvk::Trampoline` -> `impeller::TextureVK` |
| `ImageExternalTextureGLSkia` | `image_external_texture_gl_skia.cc` | OpenGLES (Skia) | `android.media.Image` / `AHardwareBuffer` | `EGLImageKHR` -> `GrBackendTexture` |
| `ImageExternalTextureGLImpeller` | `image_external_texture_gl_impeller.cc` | OpenGLES (Impeller) | `android.media.Image` / `AHardwareBuffer` | `EGLImageKHR` -> `impeller::TextureGLES` |
| `ImageExternalTextureVKImpeller` | `image_external_texture_vk_impeller.cc` | Vulkan (Impeller) | `android.media.Image` / `AHardwareBuffer` | `VkImage` (`VK_ANDROID_external_memory_android_hardware_buffer`) |

### 2.1 JNI Dependencies
The native external texture implementations depend directly on the following JNI methods declared in `shell/platform/android/jni/platform_view_android_jni.h`:
- `SurfaceTextureAttachToGLContext(jobject, jint texture_id)`: Binds the Android `SurfaceTexture` to the active OpenGL texture name.
- `SurfaceTextureUpdateTexImage(jobject)`: Extracts the latest image frame from the image stream and updates the texture.
- `SurfaceTextureGetTransformMatrix(jobject, jfloatArray)`: Fills a 16-element float array representing the 4x4 UV transformation matrix (`SkM44`).
- `SurfaceTextureDetachFromGLContext(jobject)`: Detaches the `SurfaceTexture` from the OpenGL context.
- `ImageProducerTextureEntryAcquireLatestImage(jobject)`: Retrieves an `android.media.Image` from an `ImageReader` or `SurfaceProducer`.
- `ImageGetHardwareBuffer(jobject)`: Obtains the native `AHardwareBuffer*` from the `android.media.Image`.
- `HardwareBufferClose(jobject)`: Decrements the refcount and closes the hardware buffer handle.

### 2.2 Native Buffer & HardwareBuffer Interop
- **EGL Import (`ImageExternalTextureGL*`):** Uses `eglGetNativeClientBufferANDROID(hardware_buffer)` and `eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID, client_buffer, ...)` to produce an `EGLImageKHR`, which is bound to a texture via `glEGLImageTargetTexture2DOES`.
- **Vulkan Import (`ImageExternalTextureVKImpeller`):** Uses the `VK_ANDROID_external_memory_android_hardware_buffer` extension. It queries memory properties via `vkGetAndroidHardwareBufferPropertiesANDROID`, allocates `VkDeviceMemory` with `VkImportAndroidHardwareBufferInfoANDROID`, and binds it to a dedicated `VkImage`.
- **Image Lifecycle & LRU (`image_lru.cc`):** To avoid frame starvation and excessive allocation, `ImageLRU` retains a pool of hardware buffers and sync fences (`EGLSyncKHR` / `VkFence`), recycling buffers once the GPU has completed sampling.

---

## 3. Embedder API Expressiveness Gaps

Audit of `shell/platform/embedder/embedder.h` reveals two major gaps that prevent Android external textures from working correctly:

### 3.1 Gap G-1: Absence of UV Transformation Matrix in `FlutterOpenGLTexture`
- `FlutterOpenGLTexture` currently defines:
  ```c
  typedef struct {
    uint32_t target;
    uint32_t name;
    uint32_t format;
    void* user_data;
    VoidCallback destruction_callback;
    size_t width;
    size_t height;
  } FlutterOpenGLTexture;
  ```
- **The Problem:** Android `SurfaceTexture` streams (camera, video, and Virtual Displays) output an affine UV transformation matrix (`SurfaceTexture.getTransformMatrix(float[16])`). This matrix accounts for:
  1. Camera sensor rotation (90°, 180°, 270° orientation corrections).
  2. Vertical axis inversion (OpenGL coordinate origin at bottom-left vs Android at top-left).
  3. Video cropping / non-power-of-two texture coordinate scaling.
- **Impact:** Without passing this 4x4 matrix into the engine, texture sampling defaults to identity coordinates `(u, v) in [0, 1]`, rendering camera streams upside-down and video frames cropped or distorted.
- **Resolution:** Extend `FlutterOpenGLTexture` or introduce a versioned `FlutterExternalTextureFrame` that includes `double transformation[16]` (or `float transformation[16]`) in row-major order.

### 3.2 Gap G-2: Complete Absence of External Texture Callbacks in Vulkan
- `FlutterOpenGLRendererConfig` contains `gl_external_texture_frame_callback`.
- `FlutterVulkanRendererConfig` contains **zero** external texture callbacks (`vulkan_external_texture_frame_callback` does not exist in `embedder.h`).
- **The Problem:** When running Impeller Vulkan on Android, an embedder has no API to provide external textures (`FlutterVulkanImage` or imported `AHardwareBuffer`).
- **Resolution:** Add `vulkan_external_texture_frame_callback` to `FlutterVulkanRendererConfig`, returning a `FlutterVulkanImage` (or external buffer descriptor) and destruction callback.

### 3.3 Target Requirements: `GL_TEXTURE_EXTERNAL_OES`
- `FlutterOpenGLTexture::target` already accepts `GL_TEXTURE_EXTERNAL_OES` (0x8D65). The engine's `EmbedderExternalTextureGL` supports sampling external OES targets if passed by the embedder.

---

## 4. Public Java API Surface (`TextureRegistry`)

The following public Java contracts in `io.flutter.view.TextureRegistry` must remain 100% binary- and behaviorally-compatible throughout the migration:

1. **`SurfaceTextureEntry createSurfaceTexture()`**
   - Returns a `SurfaceTextureEntry` providing `SurfaceTexture surfaceTexture()` and `long id()`.
   - Used by legacy plugins and older Android versions (API < 29).
2. **`SurfaceProducer createSurfaceProducer()`**
   - Returns a `SurfaceProducer` providing `Surface getSurface()`, `setSize(int width, int height)`, `long id()`, and `void setCallback(Callback callback)`.
   - Used by modern plugins on API 29+ for low-latency zero-copy rendering via `ImageReader`.
3. **`ImageTextureEntry createImageTexture()`**
   - Returns an `ImageTextureEntry` accepting `pushImage(android.media.Image)` and `long id()`.
4. **`SurfaceProducer.Callback`**
   - `onSurfaceCreated()`: Invoked when the underlying native surface is ready.
   - `onSurfaceDestroyed()`: Invoked when the native surface has been torn down.

---

## 5. Stage 1 Task Mapping

Based on this inventory, the following Stage 1 tasks are scheduled to close Blocker B-5:

| Task ID | Title | Scope | Deliverable |
|---|---|---|---|
| **T-1.19** | C-ABI: External Texture UV Transform Matrix | Add 4x4 transformation matrix field to external texture C-ABI | `embedder.h`, `embedder_unittests.cc` |
| **T-1.20** | C-ABI: Vulkan External Texture Callback | Add `vulkan_external_texture_frame_callback` to `FlutterVulkanRendererConfig` | `embedder.h`, `embedder_unittests.cc` |
| **T-1.21** | Android `TextureRegistry` Embedder Adapter | Connect Java `TextureRegistry` implementations to the Embedder API external texture mechanics | `shell/platform/android/` |
