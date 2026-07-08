// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import androidx.annotation.NonNull;
import androidx.test.core.app.ActivityScenario;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.embedding.engine.FlutterJNI;
import io.flutter.embedding.engine.loader.FlutterLoader;
import io.flutter.embedding.engine.plugins.FlutterPlugin;
import io.flutter.embedding.engine.plugins.activity.ActivityAware;
import io.flutter.embedding.engine.plugins.activity.ActivityPluginBinding;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@RunWith(AndroidJUnit4.class)
public class FlutterViewActivityAttachmentReproduceTest {
  private final Context ctx = ApplicationProvider.getApplicationContext();
  @Mock FlutterJNI mockFlutterJni;
  @Mock FlutterLoader mockFlutterLoader;
  private AutoCloseable mockitoSession;

  @Before
  public void setUp() {
    mockitoSession = MockitoAnnotations.openMocks(this);
    when(mockFlutterJni.isAttached()).thenReturn(true);
  }

  @After
  public void tearDown() throws Exception {
    if (mockitoSession != null) {
      mockitoSession.close();
    }
  }

  @Test
  public void testActivityAwarePluginGetsAttachedToActivityWhenUsingFlutterView() {
    // Use ActivityScenario to guarantee clean teardown and lifecycle management of the activity.
    try (ActivityScenario<Activity> scenario = ActivityScenario.launch(Activity.class)) {
      scenario.onActivity(
          activity -> {
            // 2. Create FlutterEngine.
            FlutterEngine flutterEngine = new FlutterEngine(ctx, mockFlutterLoader, mockFlutterJni);

            // 3. Register a plugin implementing FlutterPlugin and ActivityAware.
            FakeActivityAwarePlugin plugin = new FakeActivityAwarePlugin();
            flutterEngine.getPlugins().add(plugin);

            // 4. Create FlutterView with the Activity context.
            FlutterView flutterView = new FlutterView(activity);

            // 5. Attach the FlutterView to the FlutterEngine.
            flutterView.attachToFlutterEngine(flutterEngine);

            // 6. Assert that the plugin has been attached to the Activity.
            assertNotNull(
                "Expected ActivityAware plugin to be attached to Activity", plugin.binding);
          });
    }
  }

  private static class FakeActivityAwarePlugin implements FlutterPlugin, ActivityAware {
    public ActivityPluginBinding binding;

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
