package io.flutter.embedding.android;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;
import androidx.annotation.NonNull;
import androidx.lifecycle.Lifecycle;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.FlutterInjector;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.embedding.engine.FlutterShellArgs;
import io.flutter.embedding.engine.dart.DartExecutor;
import io.flutter.embedding.engine.loader.FlutterLoader;
import io.flutter.embedding.engine.plugins.activity.ActivityControlSurface;
import io.flutter.embedding.engine.renderer.FlutterRenderer;
import io.flutter.embedding.engine.systemchannels.AccessibilityChannel;
import io.flutter.embedding.engine.systemchannels.BackGestureChannel;
import io.flutter.embedding.engine.systemchannels.LifecycleChannel;
import io.flutter.embedding.engine.systemchannels.LocalizationChannel;
import io.flutter.embedding.engine.systemchannels.MouseCursorChannel;
import io.flutter.embedding.engine.systemchannels.NavigationChannel;
import io.flutter.embedding.engine.systemchannels.ScribeChannel;
import io.flutter.embedding.engine.systemchannels.SettingsChannel;
import io.flutter.embedding.engine.systemchannels.SystemChannel;
import io.flutter.embedding.engine.systemchannels.TextInputChannel;
import io.flutter.plugin.localization.LocalizationPlugin;
import io.flutter.plugin.platform.PlatformViewsController;
import io.flutter.plugin.platform.PlatformViewsController2;
import io.flutter.plugin.platform.PlatformViewsControllerDelegator;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class FlutterLifecycleKeyguardReproduceTest {
  private final Context ctx = ApplicationProvider.getApplicationContext();
  private FlutterEngine mockFlutterEngine;
  private FlutterRenderer mockRenderer;
  private FlutterActivityAndFragmentDelegate.Host mockHost;

  @Before
  @SuppressWarnings("deprecation")
  public void setup() {
    FlutterInjector.reset();
    FlutterLoader mockFlutterLoader = mock(FlutterLoader.class);
    when(mockFlutterLoader.findAppBundlePath()).thenReturn("default_flutter_assets/path");
    FlutterInjector.setInstance(
        new FlutterInjector.Builder().setFlutterLoader(mockFlutterLoader).build());

    mockFlutterEngine = mockFlutterEngine();
    mockRenderer = mockFlutterEngine.getRenderer();

    mockHost = mock(FlutterActivityAndFragmentDelegate.Host.class);
    when(mockHost.getContext()).thenReturn(ctx);
    when(mockHost.getLifecycle()).thenReturn(mock(Lifecycle.class));
    when(mockHost.getFlutterShellArgs()).thenReturn(new FlutterShellArgs(new String[] {}));
    when(mockHost.getDartEntrypointFunctionName()).thenReturn("main");
    when(mockHost.getInitialRoute()).thenReturn("/");
    when(mockHost.getRenderMode()).thenReturn(RenderMode.surface);
    when(mockHost.getTransparencyMode()).thenReturn(TransparencyMode.transparent);
    when(mockHost.provideFlutterEngine(any(Context.class))).thenReturn(mockFlutterEngine);
    when(mockHost.shouldAttachEngineToActivity()).thenReturn(true);
    when(mockHost.shouldDestroyEngineWithHost()).thenReturn(true);
    
    // Enable the new flag to retain view visibility and avoid trimming memory
    when(mockHost.shouldRetainViewVisibilityOnStop()).thenReturn(true);
  }

  @Test
  public void testViewVisibilityAndRendererTrimOnStopWhenRetained() {
    FlutterActivityAndFragmentDelegate delegate = new FlutterActivityAndFragmentDelegate(mockHost);
    delegate.onAttach(ctx);
    delegate.onCreateView(null, null, null, 0, true);
    delegate.onStart();
    delegate.onResume();

    // Verify initial state is VISIBLE
    assertEquals(View.VISIBLE, delegate.flutterView.getVisibility());

    // Simulate transitioning to background (keyguard showing)
    delegate.onPause();
    delegate.onStop();

    // We expect the view to remain VISIBLE because shouldRetainViewVisibilityOnStop() is true.
    // This will FAIL on the current codebase because onStop() unconditionally sets visibility to GONE.
    assertEquals(View.VISIBLE, delegate.flutterView.getVisibility());

    // We also expect that onTrimMemory is NOT called on the renderer.
    // This will FAIL on the current codebase because onStop() unconditionally calls onTrimMemory.
    verify(mockRenderer, never()).onTrimMemory(anyInt());
  }

  @NonNull
  private FlutterEngine mockFlutterEngine() {
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

    FlutterRenderer renderer = mock(FlutterRenderer.class);
    when(engine.getRenderer()).thenReturn(renderer);

    when(engine.getSettingsChannel()).thenReturn(fakeSettingsChannel);
    when(engine.getSystemChannel()).thenReturn(mock(SystemChannel.class));
    when(engine.getTextInputChannel()).thenReturn(mock(TextInputChannel.class));
    when(engine.getScribeChannel()).thenReturn(mock(ScribeChannel.class));

    return engine;
  }
}
