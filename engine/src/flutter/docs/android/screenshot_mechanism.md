# Android Platform Screenshot Mechanism

This document records the architectural decision and design for frame capture (screenshots) in the Flutter Android embedder.

## Background & Problem

Android's `FlutterJNI.getBitmap()` exposes bitmap capture to Java (`FlutterRenderer.getBitmap()`), which is heavily used by debug tooling, automated tests, and integration test frameworks.

Historically, this implementation routed into core engine internals:
```
FlutterJNI.getBitmap()
  └── AndroidShellHolder::Screenshot(...)
        └── flutter::Shell::Screenshot(...)
              └── flutter::Rasterizer::ScreenshotLastLayerTree(...)
```

In an earlier attempt to migrate `shell/platform/android` to the public Embedder C-API (`embedder.h`), a proposal was made to add `FlutterEngineScreenshot` and `FlutterEngineFreeScreenshot` (+563 lines) to `embedder.h`. That approach was never completed; `FlutterJNI.getBitmap()` was stubbed out to return `nullptr`, which silently broke screenshotting for tests and tools.

## Architectural Decision

**Screenshot capture must NOT enter the Embedder C-API.**

The governing architectural principle of the Flutter Embedder API is:
> *Platform-agnostic features belong in `embedder.h`; platform-specific affordances belong in platform code.*

Screenshotting is a platform-specific debugging and testing capability. It is not part of the core frame rendering pipeline, nor is it needed by embedders that do not expose platform-level bitmap capture.

### Comparison: macOS Screenshot Mechanism

The macOS desktop embedder (`shell/platform/darwin/macos/framework/Source/FlutterEngine.mm`, lines 1450–1522) demonstrates how to handle screenshots cleanly in platform code without engine APIs:
1. The platform handles a `"flutter/screenshot"` platform method channel call on the main thread.
2. It queries the platform view's front surface: `viewController.flutterView.surfaceManager.frontSurfaces.firstObject`.
3. It accesses the underlying platform surface (`IOSurfaceRef`).
4. It calls `IOSurfaceLock(ioSurface, kIOSurfaceLockReadOnly, nil)` and retrieves the raw pixel pointer via `IOSurfaceGetBaseAddress(ioSurface)`.
5. It copies tightly packed rows into a byte buffer and calls `IOSurfaceUnlock(...)`.
6. The entire operation is performed in platform code; the macOS embedder never calls `Shell::Screenshot` or `Rasterizer::ScreenshotLastLayerTree`.

## Android Platform Architecture

Android adopts the same principle, reading back pixels directly from the platform-owned rendering surface:

```
FlutterJNI.getBitmap()
  └── AndroidShellHolder::Screenshot(...)
        └── PlatformViewAndroid::Screenshot()
              └── [TaskRunners::GetRasterTaskRunner()]
                    └── AndroidSurface::Screenshot()
                          ├── AndroidSurfaceSoftware::Screenshot()
                          ├── AndroidSurfaceGLSkia::Screenshot()
                          ├── AndroidSurfaceGLImpeller::Screenshot()
                          └── AndroidSurfaceDynamicImpeller::Screenshot()
```

### Threading Model
`FlutterJNI.getBitmap()` is invoked on the Android platform thread (`FlutterJNI.ensureRunningOnPlatformThread()`). However, GPU and raster resources owned by `AndroidSurface` must only be accessed on the raster thread (`task_runners_.GetRasterTaskRunner()`).

To coordinate safely without race conditions:
1. `PlatformViewAndroid::Screenshot()` creates an `fml::AutoResetWaitableEvent latch`.
2. A task is dispatched to `task_runners_.GetRasterTaskRunner()` via `fml::TaskRunner::RunNowOrPostTask`.
3. The raster thread invokes `android_surface_->Screenshot()` and signals the latch.
4. The platform thread blocks on `latch.Wait()` and packages the resulting pixel buffer into an Android direct `ByteBuffer` and `android.graphics.Bitmap`.

### Surface Implementations

1. **`AndroidSurfaceSoftware`**:
   Reads directly from `sk_surface_` via `sk_surface_->readPixels(...)` with `SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, kPremul_SkAlphaType)`.
2. **`AndroidSurfaceGLSkia` & `AndroidSurfaceGLImpeller`**:
   Binds the on-screen EGL surface via `MakeCurrent()`, retrieves the dimensions, and issues `glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, ...)`. Because OpenGL framebuffers store pixels bottom-to-top while Android's `Bitmap` expects top-to-bottom, the rows are vertically inverted before returning.
3. **`AndroidSurfaceDynamicImpeller`**:
   Delegates to the active backing surface (`vulkan_surface_` or `gl_surface_`).
4. **`AndroidSurfaceMock`**:
   Exposes `MOCK_METHOD(Screenshot, Screenshot, (), (override));` for unit testing without hardware or emulator dependencies.

### Decoupling `Rasterizer`
By having `PlatformViewAndroid` and `AndroidSurface` own pixel readback:
- No internal engine headers (`flutter/shell/common/rasterizer.h`, `flutter/shell/common/shell.h`) are introduced into `AndroidSurface`.
- Android platform code does not reach into `Rasterizer`.
- Future migration steps can replace `AndroidShellHolder`'s internal engine usage without breaking `FlutterView` screenshotting.
