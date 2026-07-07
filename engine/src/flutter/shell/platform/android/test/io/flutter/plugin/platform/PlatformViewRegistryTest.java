// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.plugin.platform;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.view.View;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class PlatformViewRegistryTest {

  @Test
  public void testRegisterViewFactoryWithLambda() {
    PlatformViewRegistryImpl registry = new PlatformViewRegistryImpl();

    // This should compile and execute correctly if the simplified lambda registration API is supported.
    // Currently, it will fail to compile because PlatformViewRegistry does not have a method matching
    // (String, PlatformViewFactoryCallback).
    boolean result = registry.registerViewFactory("test-view-type", (context, viewId, args) -> {
      return new PlatformView() {
        @Override
        public View getView() {
          return new View(context);
        }

        @Override
        public void dispose() {}
      };
    });

    assertTrue(result);

    // Retrieve the factory and verify it behaves correctly when invoked
    PlatformViewFactory factory = registry.getFactory("test-view-type");
    assertNotNull(factory);

    Context context = ApplicationProvider.getApplicationContext();
    PlatformView platformView = factory.create(context, 123, null);
    assertNotNull(platformView);
    assertNotNull(platformView.getView());
    assertEquals(context, platformView.getView().getContext());
  }
}
