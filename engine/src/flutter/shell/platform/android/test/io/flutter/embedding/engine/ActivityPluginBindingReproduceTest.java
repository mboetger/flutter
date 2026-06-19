// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import androidx.annotation.NonNull;
import androidx.fragment.app.FragmentActivity;
import androidx.lifecycle.Lifecycle;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.android.ExclusiveAppComponent;
import io.flutter.embedding.engine.loader.FlutterLoader;
import io.flutter.embedding.engine.plugins.FlutterPlugin;
import io.flutter.embedding.engine.plugins.activity.ActivityAware;
import io.flutter.embedding.engine.plugins.activity.ActivityPluginBinding;
import io.flutter.plugin.platform.PlatformViewsController;
import io.flutter.plugin.platform.PlatformViewsController2;
import io.flutter.plugin.platform.PlatformViewsControllerDelegator;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class ActivityPluginBindingReproduceTest {

  @Test
  public void testActivityPluginBindingSupportsFragmentActivity() throws Exception {
    Context context = mock(Context.class);
    FlutterEngine flutterEngine = mock(FlutterEngine.class);
    FlutterLoader flutterLoader = mock(FlutterLoader.class);

    PlatformViewsController platformViewsController = mock(PlatformViewsController.class);
    when(flutterEngine.getPlatformViewsController()).thenReturn(platformViewsController);
    PlatformViewsController2 platformViewsController2 = mock(PlatformViewsController2.class);
    when(flutterEngine.getPlatformViewsController2()).thenReturn(platformViewsController2);
    PlatformViewsControllerDelegator platformViewsControllerDelegator =
        mock(PlatformViewsControllerDelegator.class);
    when(flutterEngine.getPlatformViewsControllerDelegator())
        .thenReturn(platformViewsControllerDelegator);

    FlutterEngineConnectionRegistry registry =
        new FlutterEngineConnectionRegistry(context, flutterEngine, flutterLoader, null);

    FakeActivityAwarePlugin plugin = new FakeActivityAwarePlugin();
    registry.add(plugin);

    // Scenario 1: Attached activity is a FragmentActivity.
    FragmentActivity fragmentActivity = mock(FragmentActivity.class);
    when(fragmentActivity.getIntent()).thenReturn(mock(Intent.class));
    ExclusiveAppComponent<Activity> appComponent1 = mock(ExclusiveAppComponent.class);
    when(appComponent1.getAppComponent()).thenReturn(fragmentActivity);

    Lifecycle lifecycle = mock(Lifecycle.class);
    registry.attachToActivity(appComponent1, lifecycle);

    ActivityPluginBinding binding = plugin.binding;
    FragmentActivity result = binding.getFragmentActivity();
    assertEquals("Should return the FragmentActivity instance", fragmentActivity, result);

    registry.detachFromActivity();

    // Scenario 2: Attached activity is a standard Activity (not a FragmentActivity).
    Activity standardActivity = mock(Activity.class);
    when(standardActivity.getIntent()).thenReturn(mock(Intent.class));
    ExclusiveAppComponent<Activity> appComponent2 = mock(ExclusiveAppComponent.class);
    when(appComponent2.getAppComponent()).thenReturn(standardActivity);

    registry.attachToActivity(appComponent2, lifecycle);

    FragmentActivity resultForStandardActivity = plugin.binding.getFragmentActivity();
    assertNull(
        "Should return null when the activity is not a FragmentActivity",
        resultForStandardActivity);
  }

  private static class FakeActivityAwarePlugin implements FlutterPlugin, ActivityAware {
    ActivityPluginBinding binding;

    @Override
    public void onAttachedToEngine(@NonNull FlutterPluginBinding binding) {}

    @Override
    public void onDetachedFromEngine(@NonNull FlutterPluginBinding binding) {}

    @Override
    public void onAttachedToActivity(@NonNull ActivityPluginBinding binding) {
      this.binding = binding;
    }

    @Override
    public void onDetachedFromActivityForConfigChanges() {}

    @Override
    public void onReattachedToActivityForConfigChanges(@NonNull ActivityPluginBinding binding) {
      this.binding = binding;
    }

    @Override
    public void onDetachedFromActivity() {
      this.binding = null;
    }
  }
}
