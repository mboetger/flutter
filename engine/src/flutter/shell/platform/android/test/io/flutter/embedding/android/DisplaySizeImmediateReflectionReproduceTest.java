// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static org.mockito.Mockito.anyFloat;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.util.DisplayMetrics;
import androidx.test.core.app.ActivityScenario;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.embedding.engine.FlutterJNI;
import io.flutter.embedding.engine.loader.FlutterLoader;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@RunWith(AndroidJUnit4.class)
public class DisplaySizeImmediateReflectionReproduceTest {
  @Mock FlutterJNI mockFlutterJni;
  @Mock FlutterLoader mockFlutterLoader;

  @Before
  public void setUp() {
    MockitoAnnotations.openMocks(this);
    when(mockFlutterJni.isAttached()).thenReturn(true);
  }

  @Test
  public void onConfigurationChanged_reflectsDensityChangeImmediately() {
    try (ActivityScenario<Activity> scenario = ActivityScenario.launch(Activity.class)) {
      scenario.onActivity(
          activity -> {
            final float oldDensity = 1.5f;
            final float newDensity = 2.0f;

            StaleDensityContextWrapper contextWrapper = new StaleDensityContextWrapper(activity, oldDensity);

            FlutterView flutterView = new FlutterView(contextWrapper);
            FlutterEngine flutterEngine = spy(new FlutterEngine(activity, mockFlutterLoader, mockFlutterJni));

            flutterView.attachToFlutterEngine(flutterEngine);

            // Trigger configuration change where density is updated to newDensity
            Configuration newConfig = new Configuration();
            newConfig.densityDpi = (int) (newDensity * 160);

            flutterView.onConfigurationChanged(newConfig);

            // Verify that updateDisplayMetrics was called on the engine with the new density immediately.
            verify(flutterEngine, atLeastOnce()).updateDisplayMetrics(anyFloat(), anyFloat(), eq(newDensity));
          });
    }
  }

  private static class StaleDensityContextWrapper extends ContextWrapper {
    private final float density;
    private Resources spyResources;

    public StaleDensityContextWrapper(Context base, float density) {
      super(base);
      this.density = density;
    }

    @Override
    public Resources getResources() {
      if (spyResources == null) {
        Resources baseResources = super.getResources();
        spyResources = spy(baseResources);
        DisplayMetrics displayMetrics = new DisplayMetrics();
        displayMetrics.setTo(baseResources.getDisplayMetrics());
        displayMetrics.density = density;
        displayMetrics.densityDpi = (int) (density * 160);
        doReturn(displayMetrics).when(spyResources).getDisplayMetrics();
      }
      return spyResources;
    }
  }
}
