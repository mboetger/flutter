// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.plugin.platform;

import static android.os.Looper.getMainLooper;
import static io.flutter.Build.API_LEVELS;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.annotation.TargetApi;
import android.app.Presentation;
import android.content.ComponentCallbacks2;
import android.content.Context;
import android.graphics.SurfaceTexture;
import android.os.Handler;
import android.view.Surface;
import android.view.View;
import android.view.ViewTreeObserver;
import androidx.annotation.NonNull;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.view.TextureRegistry;
import io.flutter.view.TextureRegistry.SurfaceTextureEntry;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowDialog;
import org.robolectric.shadows.ShadowLooper;

@RunWith(AndroidJUnit4.class)
@TargetApi(API_LEVELS.API_29)
public class VirtualDisplayControllerTest {
  @Implements(Presentation.class)
  public static class ShadowPresentation extends ShadowDialog {
    private boolean isShowing = false;

    public ShadowPresentation() {}

    @Implementation
    protected void show() {
      isShowing = true;
    }

    @Implementation
    protected void dismiss() {
      isShowing = false;
    }

    @Implementation
    protected boolean isShowing() {
      return isShowing;
    }
  }

  static class FakeView extends View {
    final List<OnAttachStateChangeListener> attachListeners = new ArrayList<>();

    FakeView(Context context) {
      super(context);
    }

    @Override
    public void addOnAttachStateChangeListener(OnAttachStateChangeListener listener) {
      super.addOnAttachStateChangeListener(listener);
      attachListeners.add(listener);
    }

    @Override
    public void removeOnAttachStateChangeListener(OnAttachStateChangeListener listener) {
      super.removeOnAttachStateChangeListener(listener);
      attachListeners.remove(listener);
    }

    @Override
    public boolean post(Runnable action) {
      new Handler(getMainLooper()).post(action);
      return true;
    }

    @Override
    public boolean postDelayed(Runnable action, long delayMillis) {
      new Handler(getMainLooper()).postDelayed(action, delayMillis);
      return true;
    }

    void triggerAttach() {
      List<OnAttachStateChangeListener> copy = new ArrayList<>(attachListeners);
      for (OnAttachStateChangeListener listener : copy) {
        listener.onViewAttachedToWindow(this);
      }
    }

    void triggerDraw() {
      try {
        ViewTreeObserver observer = getViewTreeObserver();
        List<ViewTreeObserver.OnDrawListener> listeners = new ArrayList<>();
        findDrawListeners(observer, listeners, 0);
        for (ViewTreeObserver.OnDrawListener listener : listeners) {
          listener.onDraw();
        }
      } catch (Exception e) {
        throw new RuntimeException(e);
      }
    }

    private void findDrawListeners(
        Object obj, List<ViewTreeObserver.OnDrawListener> out, int depth) {
      if (obj == null || depth > 3) return;
      if (obj instanceof ViewTreeObserver.OnDrawListener) {
        out.add((ViewTreeObserver.OnDrawListener) obj);
        return;
      }
      if (obj instanceof Iterable) {
        for (Object item : (Iterable<?>) obj) {
          findDrawListeners(item, out, depth + 1);
        }
        return;
      }
      if (obj instanceof Object[]) {
        for (Object item : (Object[]) obj) {
          findDrawListeners(item, out, depth + 1);
        }
        return;
      }
      Class<?> clazz = obj.getClass();
      while (clazz != null && clazz != Object.class) {
        for (java.lang.reflect.Field field : clazz.getDeclaredFields()) {
          if (java.lang.reflect.Modifier.isStatic(field.getModifiers())
              || field.getType().isPrimitive()) {
            continue;
          }
          field.setAccessible(true);
          try {
            Object val = field.get(obj);
            if (val != obj && val != this && !(val instanceof Context) && !(val instanceof View)) {
              findDrawListeners(val, out, depth + 1);
            }
          } catch (Exception ignored) {
          }
        }
        clazz = clazz.getSuperclass();
      }
    }
  }

  static class FakePlatformView implements PlatformView {
    final FakeView view;
    int attachCalls = 0;
    int detachCalls = 0;

    FakePlatformView(Context context) {
      view = new FakeView(context);
    }

    @Override
    public View getView() {
      return view;
    }

    @Override
    public void onFlutterViewAttached(@NonNull View flutterView) {
      attachCalls++;
    }

    @Override
    public void onFlutterViewDetached() {
      detachCalls++;
    }

    @Override
    public void dispose() {}
  }

  private VirtualDisplayController createController(
      Context context, PlatformView view, PlatformViewRenderTarget renderTarget) {
    return VirtualDisplayController.create(
        context,
        new AccessibilityEventsDelegate(),
        view,
        renderTarget,
        1080,
        1920,
        0,
        null,
        null);
  }

  @Test
  @Config(minSdk = API_LEVELS.API_29, maxSdk = API_LEVELS.API_29, shadows = {ShadowPresentation.class})
  public void resize_continualPipModeSwitching_onAndroid10() {
    Context context = ApplicationProvider.getApplicationContext();
    FakePlatformView platformView = new FakePlatformView(context);
    final Surface surface = mock(Surface.class);
    when(surface.isValid()).thenReturn(true);
    final SurfaceTexture surfaceTexture = mock(SurfaceTexture.class);
    final SurfaceTextureEntry surfaceTextureEntry = mock(SurfaceTextureEntry.class);
    when(surfaceTextureEntry.surfaceTexture()).thenReturn(surfaceTexture);
    when(surfaceTexture.isReleased()).thenReturn(false);

    final SurfaceTexturePlatformViewRenderTarget renderTarget =
        new SurfaceTexturePlatformViewRenderTarget(surfaceTextureEntry) {
          @Override
          protected Surface createSurface() {
            return surface;
          }
        };

    VirtualDisplayController controller = createController(context, platformView, renderTarget);
    assertNotNull(controller);
    assertEquals(1080, controller.getRenderTargetWidth());
    assertEquals(1920, controller.getRenderTargetHeight());

    simulatePipResizingLoop(controller, platformView);
  }

  private void simulatePipResizingLoop(
      VirtualDisplayController controller, FakePlatformView platformView) {
    for (int i = 0; i < 5; i++) {
      final AtomicBoolean pipCallbackRan = new AtomicBoolean(false);
      controller.resize(300, 500, () -> pipCallbackRan.set(true));
      platformView.view.triggerAttach();
      platformView.view.triggerDraw();
      ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

      assertEquals(300, controller.getRenderTargetWidth());
      assertEquals(500, controller.getRenderTargetHeight());
      assertTrue(pipCallbackRan.get());

      final AtomicBoolean fgCallbackRan = new AtomicBoolean(false);
      controller.resize(1080, 1920, () -> fgCallbackRan.set(true));
      platformView.view.triggerAttach();
      platformView.view.triggerDraw();
      ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

      assertEquals(1080, controller.getRenderTargetWidth());
      assertEquals(1920, controller.getRenderTargetHeight());
      assertTrue(fgCallbackRan.get());
    }
  }

  @Test
  @SuppressWarnings({"deprecation", "removal"})
  @Config(minSdk = API_LEVELS.API_29, maxSdk = API_LEVELS.API_29, shadows = {ShadowPresentation.class})
  public void onTrimMemory_and_resetSurface_duringPipTransition_onAndroid10() {
    Context context = ApplicationProvider.getApplicationContext();
    FakePlatformView platformView = new FakePlatformView(context);
    final AtomicInteger createSurfaceCalls = new AtomicInteger(0);
    final Surface surface1 = mock(Surface.class);
    final Surface surface2 = mock(Surface.class);
    when(surface1.isValid()).thenReturn(true);
    when(surface2.isValid()).thenReturn(true);

    final SurfaceTexture surfaceTexture = mock(SurfaceTexture.class);
    final SurfaceTextureEntry surfaceTextureEntry = mock(SurfaceTextureEntry.class);
    when(surfaceTextureEntry.surfaceTexture()).thenReturn(surfaceTexture);
    when(surfaceTexture.isReleased()).thenReturn(false);

    ArgumentCaptor<TextureRegistry.OnTrimMemoryListener> trimCaptor =
        ArgumentCaptor.forClass(TextureRegistry.OnTrimMemoryListener.class);

    final SurfaceTexturePlatformViewRenderTarget renderTarget =
        new SurfaceTexturePlatformViewRenderTarget(surfaceTextureEntry) {
          @Override
          protected Surface createSurface() {
            int count = createSurfaceCalls.incrementAndGet();
            return count == 1 ? surface1 : surface2;
          }
        };

    verify(surfaceTextureEntry).setOnTrimMemoryListener(trimCaptor.capture());
    VirtualDisplayController controller = createController(context, platformView, renderTarget);
    assertEquals(1, createSurfaceCalls.get());

    controller.clearSurface();
    trimCaptor.getValue().onTrimMemory(ComponentCallbacks2.TRIM_MEMORY_COMPLETE);
    controller.resetSurface();

    assertEquals(2, createSurfaceCalls.get());
    verify(surface1, times(1)).release();
  }

  @Test
  @SuppressWarnings({"deprecation", "removal"})
  @Config(minSdk = API_LEVELS.API_29, maxSdk = API_LEVELS.API_29, shadows = {ShadowPresentation.class})
  public void onTrimMemory_background_and_resetSurface_duringPipTransition_onAndroid10() {
    Context context = ApplicationProvider.getApplicationContext();
    FakePlatformView platformView = new FakePlatformView(context);
    final AtomicInteger createSurfaceCalls = new AtomicInteger(0);
    final Surface surface1 = mock(Surface.class);
    final Surface surface2 = mock(Surface.class);
    when(surface1.isValid()).thenReturn(true);
    when(surface2.isValid()).thenReturn(true);

    final SurfaceTexture surfaceTexture = mock(SurfaceTexture.class);
    final SurfaceTextureEntry surfaceTextureEntry = mock(SurfaceTextureEntry.class);
    when(surfaceTextureEntry.surfaceTexture()).thenReturn(surfaceTexture);
    when(surfaceTexture.isReleased()).thenReturn(false);

    ArgumentCaptor<TextureRegistry.OnTrimMemoryListener> trimCaptor =
        ArgumentCaptor.forClass(TextureRegistry.OnTrimMemoryListener.class);

    final SurfaceTexturePlatformViewRenderTarget renderTarget =
        new SurfaceTexturePlatformViewRenderTarget(surfaceTextureEntry) {
          @Override
          protected Surface createSurface() {
            int count = createSurfaceCalls.incrementAndGet();
            return count == 1 ? surface1 : surface2;
          }
        };

    verify(surfaceTextureEntry).setOnTrimMemoryListener(trimCaptor.capture());
    VirtualDisplayController controller = createController(context, platformView, renderTarget);
    assertEquals(1, createSurfaceCalls.get());

    controller.clearSurface();
    trimCaptor.getValue().onTrimMemory(ComponentCallbacks2.TRIM_MEMORY_BACKGROUND);
    controller.resetSurface();

    assertEquals(2, createSurfaceCalls.get());
    verify(surface1, times(1)).release();
  }

  @Test
  @Config(minSdk = API_LEVELS.API_29, maxSdk = API_LEVELS.API_29, shadows = {ShadowPresentation.class})
  public void onFlutterViewAttached_invokesPlatformViewAttached() {
    Context context = ApplicationProvider.getApplicationContext();
    FakePlatformView platformView = new FakePlatformView(context);
    final Surface surface = mock(Surface.class);
    when(surface.isValid()).thenReturn(true);
    final SurfaceTexture surfaceTexture = mock(SurfaceTexture.class);
    final SurfaceTextureEntry surfaceTextureEntry = mock(SurfaceTextureEntry.class);
    when(surfaceTextureEntry.surfaceTexture()).thenReturn(surfaceTexture);
    when(surfaceTexture.isReleased()).thenReturn(false);

    final SurfaceTexturePlatformViewRenderTarget renderTarget =
        new SurfaceTexturePlatformViewRenderTarget(surfaceTextureEntry) {
          @Override
          protected Surface createSurface() {
            return surface;
          }
        };

    VirtualDisplayController controller = createController(context, platformView, renderTarget);
    View flutterView = new View(context);

    assertEquals(0, platformView.attachCalls);
    controller.onFlutterViewAttached(flutterView);
    assertEquals(1, platformView.attachCalls);

    assertEquals(0, platformView.detachCalls);
    controller.onFlutterViewDetached();
    assertEquals(1, platformView.detachCalls);
  }

  @Test
  @Config(minSdk = API_LEVELS.API_29, maxSdk = API_LEVELS.API_29, shadows = {ShadowPresentation.class})
  public void oldPluginThrowingAbstractMethodError_isHandledSafely() {
    Context context = ApplicationProvider.getApplicationContext();
    PlatformView oldPluginView = mock(PlatformView.class);
    when(oldPluginView.getView()).thenReturn(new View(context));
    org.mockito.Mockito.doThrow(new AbstractMethodError())
        .when(oldPluginView)
        .onFlutterViewAttached(any());
    org.mockito.Mockito.doThrow(new AbstractMethodError())
        .when(oldPluginView)
        .onFlutterViewDetached();
    org.mockito.Mockito.doThrow(new AbstractMethodError())
        .when(oldPluginView)
        .onInputConnectionLocked();
    org.mockito.Mockito.doThrow(new AbstractMethodError())
        .when(oldPluginView)
        .onInputConnectionUnlocked();

    final Surface surface = mock(Surface.class);
    when(surface.isValid()).thenReturn(true);
    final SurfaceTexture surfaceTexture = mock(SurfaceTexture.class);
    final SurfaceTextureEntry surfaceTextureEntry = mock(SurfaceTextureEntry.class);
    when(surfaceTextureEntry.surfaceTexture()).thenReturn(surfaceTexture);
    when(surfaceTexture.isReleased()).thenReturn(false);

    final SurfaceTexturePlatformViewRenderTarget renderTarget =
        new SurfaceTexturePlatformViewRenderTarget(surfaceTextureEntry) {
          @Override
          protected Surface createSurface() {
            return surface;
          }
        };

    VirtualDisplayController controller = createController(context, oldPluginView, renderTarget);
    View flutterView = new View(context);

    // These must not throw AbstractMethodError as reported in issue flutter/flutter#73149
    controller.onFlutterViewAttached(flutterView);
    controller.onFlutterViewDetached();
    controller.onInputConnectionLocked();
    controller.onInputConnectionUnlocked();
  }
}
