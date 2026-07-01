// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static io.flutter.embedding.android.FlutterActivityLaunchConfigs.EXTRA_CACHED_ENGINE_ID;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.os.Looper;
import android.content.Intent;
import androidx.annotation.NonNull;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.FlutterInjector;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.embedding.engine.FlutterEngineCache;
import io.flutter.embedding.engine.dart.DartExecutor;
import io.flutter.embedding.engine.plugins.activity.ActivityControlSurface;
import io.flutter.embedding.engine.renderer.FlutterRenderer;
import io.flutter.embedding.engine.renderer.FlutterUiDisplayListener;
import io.flutter.embedding.engine.systemchannels.AccessibilityChannel;
import io.flutter.embedding.engine.systemchannels.BackGestureChannel;
import io.flutter.embedding.engine.systemchannels.LifecycleChannel;
import io.flutter.embedding.engine.systemchannels.LocalizationChannel;
import io.flutter.embedding.engine.systemchannels.MouseCursorChannel;
import io.flutter.embedding.engine.systemchannels.NavigationChannel;
import io.flutter.embedding.engine.systemchannels.PlatformChannel;
import io.flutter.embedding.engine.systemchannels.ScribeChannel;
import io.flutter.embedding.engine.systemchannels.SensitiveContentChannel;
import io.flutter.embedding.engine.systemchannels.SettingsChannel;
import io.flutter.embedding.engine.systemchannels.SystemChannel;
import io.flutter.embedding.engine.systemchannels.TextInputChannel;
import io.flutter.plugin.localization.LocalizationPlugin;
import io.flutter.plugin.platform.PlatformViewsController;
import io.flutter.plugin.platform.PlatformViewsController2;
import io.flutter.plugin.platform.PlatformViewsControllerDelegator;
import java.util.ArrayList;
import java.util.List;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

@RunWith(AndroidJUnit4.class)
public class FlutterResumeFlickerReproduceTest {
  private final Context ctx = ApplicationProvider.getApplicationContext();
  private FlutterEngine mockFlutterEngine;
  private FlutterRenderer mockFlutterRenderer;
  private final List<FlutterUiDisplayListener> flutterUiDisplayListeners = new ArrayList<>();

  @Before
  public void setUp() {
    FlutterInjector.reset();
    flutterUiDisplayListeners.clear();
    
    mockFlutterRenderer = mock(FlutterRenderer.class);
    mockFlutterEngine = mockFlutterEngine(mockFlutterRenderer);
    
    FlutterEngineCache.getInstance().put("my_cached_engine", mockFlutterEngine);
  }

  @After
  public void tearDown() {
    FlutterEngineCache.getInstance().remove("my_cached_engine");
    FlutterInjector.reset();
  }

  @Test
  public void testStopAndResume_convertsToImageViewToAvoidFlicker() {
    Intent intent = FlutterActivity.createDefaultIntent(ctx)
        .putExtra(EXTRA_CACHED_ENGINE_ID, "my_cached_engine");
        
    ActivityController<FlutterActivity> activityController =
        Robolectric.buildActivity(FlutterActivity.class, intent);
    
    activityController.create();
    activityController.start();
    activityController.resume();

    FlutterActivity activity = activityController.get();
    FlutterView flutterView = (FlutterView) activity.findViewById(FlutterActivity.FLUTTER_VIEW_ID);

    // Initially, there should be no overlay image surface.
    assertNull(flutterView.getCurrentImageSurface());

    // Simulate going to background (stop)
    activityController.pause();
    activityController.stop();

    // Assert that when stopped, we have created an overlay FlutterImageView to preserve the last frame.
    // If this fails, it means we don't convert to FlutterImageView on stop, which leads to a black
    // or launch screen flicker when the surface is recreated on resume.
    FlutterImageView imageView = flutterView.getCurrentImageSurface();
    assertNotNull("FlutterImageView overlay should be present when stopped to prevent flicker",
        imageView);
    verify(mockFlutterRenderer).swapSurface(imageView.getSurface());

    // Simulate returning to foreground (start, resume)
    activityController.restart();
    activityController.start();
    activityController.resume();

    // On resume, the overlay FlutterImageView should still be present until the first frame is rendered.
    assertNotNull("FlutterImageView overlay should still be present on resume before first frame",
        flutterView.getCurrentImageSurface());

    // Now simulate the first frame being rendered.
    // Note: We copy the list to avoid ConcurrentModificationException because invoking the
    // listener might cause it to be removed.
    for (FlutterUiDisplayListener listener : new ArrayList<>(flutterUiDisplayListeners)) {
      listener.onFlutterUiDisplayed();
    }

    // Idle the main looper to allow the delayed releaseImageView() to run.
    shadowOf(Looper.getMainLooper()).idle();

    // Verify that the overlay has been removed after the first frame is rendered.
    assertNull("FlutterImageView overlay should be removed after first frame is rendered",
        flutterView.getCurrentImageSurface());

    // Clean up
    activityController.destroy();
  }

  @Test
  public void testDestroyBeforeFirstFrame_removesListenerToAvoidLeak() {
    Context ctx = ApplicationProvider.getApplicationContext();
    Intent intent = FlutterActivity.createDefaultIntent(ctx)
        .putExtra(EXTRA_CACHED_ENGINE_ID, "my_cached_engine");
        
    ActivityController<FlutterActivity> activityController =
        Robolectric.buildActivity(FlutterActivity.class, intent);
     
    activityController.create();
    activityController.start();
    activityController.resume();

    // Go to background
    activityController.pause();
    activityController.stop();

    // Resume
    activityController.restart();
    activityController.start();
    activityController.resume();

    // Destroy the activity before the first frame is rendered
    activityController.destroy();

    // Verify that the listener was removed from the renderer
    assertTrue("Listener should be removed on destroy to prevent memory leaks",
        flutterUiDisplayListeners.isEmpty());
  }

  @Test
  public void testStopDuringResume_stillPreservesImageView() {
    Context ctx = ApplicationProvider.getApplicationContext();
    Intent intent = FlutterActivity.createDefaultIntent(ctx)
        .putExtra(EXTRA_CACHED_ENGINE_ID, "my_cached_engine");
         
    ActivityController<FlutterActivity> activityController =
        Robolectric.buildActivity(FlutterActivity.class, intent);
     
    activityController.create();
    activityController.start();
    activityController.resume();

    FlutterActivity activity = activityController.get();
    FlutterView flutterView = (FlutterView) activity.findViewById(FlutterActivity.FLUTTER_VIEW_ID);

    // First stop
    activityController.pause();
    activityController.stop();
    assertNotNull(flutterView.getCurrentImageSurface());

    // Start resuming
    activityController.restart();
    activityController.start();
    activityController.resume();

    // Stop again BEFORE the first frame is rendered
    activityController.pause();
    activityController.stop();

    // The image view should still be active
    assertTrue("Should be using image view when stopped during resume",
        flutterView.isUsingImageView());
  }

  @NonNull
  private FlutterEngine mockFlutterEngine(FlutterRenderer renderer) {
    SettingsChannel fakeSettingsChannel = mock(SettingsChannel.class);
    SettingsChannel.MessageBuilder fakeMessageBuilder = mock(SettingsChannel.MessageBuilder.class);
    when(fakeMessageBuilder.setPlatformBrightness(any(SettingsChannel.PlatformBrightness.class)))
        .thenReturn(fakeMessageBuilder);
    when(fakeMessageBuilder.setTextScaleFactor(any(Float.class))).thenReturn(fakeMessageBuilder);
    when(fakeMessageBuilder.setDisplayMetrics(any())).thenReturn(fakeMessageBuilder);
    when(fakeMessageBuilder.setNativeSpellCheckServiceDefined(any(Boolean.class)))
        .thenReturn(fakeMessageBuilder);
    when(fakeMessageBuilder.setBrieflyShowPassword(any(Boolean.class)))
        .thenReturn(fakeMessageBuilder);
    when(fakeMessageBuilder.setUse24HourFormat(any(Boolean.class))).thenReturn(fakeMessageBuilder);
    when(fakeSettingsChannel.startMessage()).thenReturn(fakeMessageBuilder);

    FlutterEngine engine = mock(FlutterEngine.class);
    when(engine.getAccessibilityChannel()).thenReturn(mock(AccessibilityChannel.class));
    when(engine.getActivityControlSurface()).thenReturn(mock(ActivityControlSurface.class));
    when(engine.getDartExecutor()).thenReturn(mock(DartExecutor.class));
    when(engine.getLifecycleChannel()).thenReturn(mock(LifecycleChannel.class));
    when(engine.getLocalizationChannel()).thenReturn(mock(LocalizationChannel.class));
    when(engine.getLocalizationPlugin()).thenReturn(mock(LocalizationPlugin.class));
    when(engine.getMouseCursorChannel()).thenReturn(mock(MouseCursorChannel.class));
    when(engine.getNavigationChannel()).thenReturn(mock(NavigationChannel.class));
    when(engine.getBackGestureChannel()).thenReturn(mock(BackGestureChannel.class));
    when(engine.getPlatformViewsController()).thenReturn(mock(PlatformViewsController.class));
    when(engine.getPlatformViewsController2()).thenReturn(mock(PlatformViewsController2.class));
    when(engine.getPlatformViewsControllerDelegator())
        .thenReturn(mock(PlatformViewsControllerDelegator.class));
    when(engine.getRenderer()).thenReturn(renderer);
    when(engine.getSettingsChannel()).thenReturn(fakeSettingsChannel);
    when(engine.getSystemChannel()).thenReturn(mock(SystemChannel.class));
    when(engine.getTextInputChannel()).thenReturn(mock(TextInputChannel.class));
    when(engine.getScribeChannel()).thenReturn(mock(ScribeChannel.class));
    
    when(engine.getPlatformChannel()).thenReturn(mock(PlatformChannel.class));
    when(engine.getSensitiveContentChannel()).thenReturn(mock(SensitiveContentChannel.class));

    // Handle listener registration
    doAnswer(invocation -> {
      flutterUiDisplayListeners.add(invocation.getArgument(0));
      return null;
    }).when(renderer).addIsDisplayingFlutterUiListener(any());

    doAnswer(invocation -> {
      flutterUiDisplayListeners.remove(invocation.getArgument(0));
      return null;
    }).when(renderer).removeIsDisplayingFlutterUiListener(any());

    doAnswer(invocation -> {
      android.view.Surface surface = invocation.getArgument(0);
      if (surface != null && surface.isValid()) {
        try {
          android.graphics.Canvas canvas = surface.lockCanvas(null);
          if (canvas != null) {
            canvas.drawColor(android.graphics.Color.RED);
            surface.unlockCanvasAndPost(canvas);
          }
        } catch (Exception e) {
          e.printStackTrace();
        }
      }
      return null;
    }).when(renderer).swapSurface(any());

    return engine;
  }
}
