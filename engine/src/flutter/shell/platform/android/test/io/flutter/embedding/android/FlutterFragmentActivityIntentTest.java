// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static io.flutter.embedding.android.FlutterActivityLaunchConfigs.DART_ENTRYPOINT_META_DATA_KEY;
import static io.flutter.embedding.android.FlutterActivityLaunchConfigs.DART_ENTRYPOINT_URI_META_DATA_KEY;
import static io.flutter.embedding.android.FlutterActivityLaunchConfigs.HANDLE_DEEPLINKING_META_DATA_KEY;
import static io.flutter.embedding.android.FlutterActivityLaunchConfigs.INITIAL_ROUTE_META_DATA_KEY;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import androidx.annotation.Nullable;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.FlutterInjector;
import io.flutter.embedding.android.FlutterActivityLaunchConfigs.BackgroundMode;
import io.flutter.embedding.engine.FlutterJNI;
import io.flutter.plugins.GeneratedPluginRegistrant;
import java.util.ArrayList;
import java.util.Arrays;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

@RunWith(AndroidJUnit4.class)
public class FlutterFragmentActivityIntentTest {
  private final Context ctx = ApplicationProvider.getApplicationContext();

  @Before
  public void setUp() {
    FlutterInjector.reset();
    GeneratedPluginRegistrant.clearRegisteredEngines();
    FlutterJNI mockFlutterJNI = mock(FlutterJNI.class);
    when(mockFlutterJNI.isAttached()).thenReturn(true);
    FlutterJNI.Factory mockFlutterJNIFactory = mock(FlutterJNI.Factory.class);
    when(mockFlutterJNIFactory.provideFlutterJNI()).thenReturn(mockFlutterJNI);
    FlutterInjector.setInstance(
        new FlutterInjector.Builder().setFlutterJNIFactory(mockFlutterJNIFactory).build());
  }

  @After
  public void tearDown() {
    GeneratedPluginRegistrant.clearRegisteredEngines();
    FlutterInjector.reset();
  }

  @Test
  public void itCreatesDefaultIntentWithExpectedDefaults() {
    Intent intent = FlutterFragmentActivity.createDefaultIntent(ctx);
    ActivityController<TestFlutterFragmentActivity> activityController =
        Robolectric.buildActivity(TestFlutterFragmentActivity.class, intent);
    TestFlutterFragmentActivity activity = activityController.get();

    assertEquals("main", activity.getDartEntrypointFunctionName());
    assertNull(activity.getDartEntrypointLibraryUri());
    assertNull(activity.getDartEntrypointArgs());
    assertEquals("/", activity.getInitialRoute());
    assertTrue(activity.shouldAttachEngineToActivity());
    assertNull(activity.getCachedEngineId());
    assertTrue(activity.shouldDestroyEngineWithHost());
    assertEquals(BackgroundMode.opaque, activity.getBackgroundMode());
    assertEquals(RenderMode.surface, activity.getRenderMode());
  }

  @Test
  public void itCreatesNewEngineIntentWithRequestedSettings() {
    Intent intent =
        FlutterFragmentActivity.withNewEngine()
            .initialRoute("/custom/route")
            .dartEntrypointArgs(new ArrayList<String>(Arrays.asList("foo", "bar")))
            .backgroundMode(BackgroundMode.transparent)
            .build(ctx);
    ActivityController<TestFlutterFragmentActivity> activityController =
        Robolectric.buildActivity(TestFlutterFragmentActivity.class, intent);
    TestFlutterFragmentActivity activity = activityController.get();

    assertEquals("/custom/route", activity.getInitialRoute());
    assertNotNull(activity.getDartEntrypointArgs());
    assertArrayEquals(
        new String[] {"foo", "bar"}, activity.getDartEntrypointArgs().toArray());
    assertTrue(activity.shouldAttachEngineToActivity());
    assertNull(activity.getCachedEngineId());
    assertTrue(activity.shouldDestroyEngineWithHost());
    assertEquals(BackgroundMode.transparent, activity.getBackgroundMode());
    assertEquals(RenderMode.texture, activity.getRenderMode());
  }

  @Test
  public void itCreatesNewEngineInGroupIntentWithRequestedSettings() {
    Intent intent =
        FlutterFragmentActivity.withNewEngineInGroup("my_cached_engine_group")
            .dartEntrypoint("custom_entrypoint")
            .initialRoute("/custom/route")
            .backgroundMode(BackgroundMode.transparent)
            .build(ctx);
    ActivityController<TestFlutterFragmentActivity> activityController =
        Robolectric.buildActivity(TestFlutterFragmentActivity.class, intent);
    TestFlutterFragmentActivity activity = activityController.get();

    assertEquals("my_cached_engine_group", activity.getCachedEngineGroupId());
    assertEquals("custom_entrypoint", activity.getDartEntrypointFunctionName());
    assertEquals("/custom/route", activity.getInitialRoute());
    assertTrue(activity.shouldAttachEngineToActivity());
    assertTrue(activity.shouldDestroyEngineWithHost());
    assertNull(activity.getCachedEngineId());
    assertEquals(BackgroundMode.transparent, activity.getBackgroundMode());
    assertEquals(RenderMode.texture, activity.getRenderMode());
  }

  @Test
  public void itCreatesCachedEngineIntentThatDoesNotDestroyTheEngine() {
    Intent intent =
        FlutterFragmentActivity.withCachedEngine("my_cached_engine")
            .destroyEngineWithActivity(false)
            .build(ctx);
    ActivityController<TestFlutterFragmentActivity> activityController =
        Robolectric.buildActivity(TestFlutterFragmentActivity.class, intent);
    TestFlutterFragmentActivity activity = activityController.get();

    assertTrue(activity.shouldAttachEngineToActivity());
    assertEquals("my_cached_engine", activity.getCachedEngineId());
    assertFalse(activity.shouldDestroyEngineWithHost());
  }

  @Test
  public void itParsesDartEntrypointFromMetadata() throws PackageManager.NameNotFoundException {
    Intent intent = FlutterFragmentActivity.createDefaultIntent(ctx);
    ActivityController<TestFlutterFragmentActivity> activityController =
        Robolectric.buildActivity(TestFlutterFragmentActivity.class, intent);
    TestFlutterFragmentActivity activity = spy(activityController.get());

    Bundle bundle = new Bundle();
    bundle.putString(DART_ENTRYPOINT_META_DATA_KEY, "custom_entrypoint");
    when(activity.getMetaData()).thenReturn(bundle);

    assertEquals("custom_entrypoint", activity.getDartEntrypointFunctionName());
  }

  @Test
  public void itParsesDartEntrypointLibraryUriFromMetadata() throws PackageManager.NameNotFoundException {
    Intent intent = FlutterFragmentActivity.createDefaultIntent(ctx);
    ActivityController<TestFlutterFragmentActivity> activityController =
        Robolectric.buildActivity(TestFlutterFragmentActivity.class, intent);
    TestFlutterFragmentActivity activity = spy(activityController.get());

    Bundle bundle = new Bundle();
    bundle.putString(DART_ENTRYPOINT_URI_META_DATA_KEY, "package:foo/bar.dart");
    when(activity.getMetaData()).thenReturn(bundle);

    assertEquals("package:foo/bar.dart", activity.getDartEntrypointLibraryUri());
  }

  @Test
  public void itParsesInitialRouteFromMetadata() throws PackageManager.NameNotFoundException {
    // We must not pass the extra initial route in the intent, so getInitialRoute falls back to metadata.
    Intent intent = new Intent(ctx, TestFlutterFragmentActivity.class);
    ActivityController<TestFlutterFragmentActivity> activityController =
        Robolectric.buildActivity(TestFlutterFragmentActivity.class, intent);
    TestFlutterFragmentActivity activity = spy(activityController.get());

    Bundle bundle = new Bundle();
    bundle.putString(INITIAL_ROUTE_META_DATA_KEY, "/metadata/route");
    when(activity.getMetaData()).thenReturn(bundle);

    assertEquals("/metadata/route", activity.getInitialRoute());
  }

  @Test
  public void itParsesAppBundlePathInDebugModeWithActionRun() {
    Intent intent = new Intent(Intent.ACTION_RUN);
    intent.setData(android.net.Uri.parse("file:///path/to/bundle"));

    ActivityController<TestFlutterFragmentActivity> activityController =
        Robolectric.buildActivity(TestFlutterFragmentActivity.class, intent);
    TestFlutterFragmentActivity activity = spy(activityController.get());

    // Force the activity to report as debuggable.
    activity.getApplicationInfo().flags |= ApplicationInfo.FLAG_DEBUGGABLE;

    assertEquals("file:///path/to/bundle", activity.getAppBundlePath());
  }

  @Test
  public void itDoesNotParseAppBundlePathWhenNotDebuggable() {
    Intent intent = new Intent(Intent.ACTION_RUN);
    intent.setData(android.net.Uri.parse("file:///path/to/bundle"));

    ActivityController<TestFlutterFragmentActivity> activityController =
        Robolectric.buildActivity(TestFlutterFragmentActivity.class, intent);
    TestFlutterFragmentActivity activity = spy(activityController.get());

    activity.getApplicationInfo().flags &= ~ApplicationInfo.FLAG_DEBUGGABLE;

    assertNull(activity.getAppBundlePath());
  }

  @Test
  public void itParsesShouldHandleDeeplinkingFromMetadata() throws PackageManager.NameNotFoundException {
    Intent intent = FlutterFragmentActivity.createDefaultIntent(ctx);
    ActivityController<TestFlutterFragmentActivity> activityController =
        Robolectric.buildActivity(TestFlutterFragmentActivity.class, intent);
    TestFlutterFragmentActivity activity = spy(activityController.get());

    Bundle bundle = new Bundle();
    bundle.putBoolean(HANDLE_DEEPLINKING_META_DATA_KEY, true);
    when(activity.getMetaData()).thenReturn(bundle);

    assertTrue(activity.shouldHandleDeeplinking());
  }

  @Test
  public void itFallsBackToDefaultsWhenMetadataIsNull() throws PackageManager.NameNotFoundException {
    Intent intent = new Intent(ctx, TestFlutterFragmentActivity.class);
    ActivityController<TestFlutterFragmentActivity> activityController =
        Robolectric.buildActivity(TestFlutterFragmentActivity.class, intent);
    TestFlutterFragmentActivity activity = spy(activityController.get());

    when(activity.getMetaData()).thenReturn(null);

    assertEquals("main", activity.getDartEntrypointFunctionName());
    assertNull(activity.getDartEntrypointLibraryUri());
    assertNull(activity.getInitialRoute());
    assertTrue(activity.shouldHandleDeeplinking());
  }

  @Test
  public void itFallsBackToDefaultsWhenMetadataThrows() throws PackageManager.NameNotFoundException {
    Intent intent = new Intent(ctx, TestFlutterFragmentActivity.class);
    ActivityController<TestFlutterFragmentActivity> activityController =
        Robolectric.buildActivity(TestFlutterFragmentActivity.class, intent);
    TestFlutterFragmentActivity activity = spy(activityController.get());

    when(activity.getMetaData()).thenThrow(new PackageManager.NameNotFoundException());

    assertEquals("main", activity.getDartEntrypointFunctionName());
    assertNull(activity.getDartEntrypointLibraryUri());
    assertNull(activity.getInitialRoute());
    assertFalse(activity.shouldHandleDeeplinking());
  }

  static class TestFlutterFragmentActivity extends FlutterFragmentActivity {
    @Nullable
    @Override
    public Bundle getMetaData() throws PackageManager.NameNotFoundException {
      return super.getMetaData();
    }

    @Override
    public String getInitialRoute() {
      return super.getInitialRoute();
    }

    @Override
    public String getCachedEngineId() {
      return super.getCachedEngineId();
    }

    @Override
    public String getCachedEngineGroupId() {
      return super.getCachedEngineGroupId();
    }

    @Override
    public BackgroundMode getBackgroundMode() {
      return super.getBackgroundMode();
    }

    @Override
    public RenderMode getRenderMode() {
      return super.getRenderMode();
    }

    @Override
    public String getAppBundlePath() {
      return super.getAppBundlePath();
    }

    @Override
    public boolean shouldHandleDeeplinking() {
      return super.shouldHandleDeeplinking();
    }
  }
}
