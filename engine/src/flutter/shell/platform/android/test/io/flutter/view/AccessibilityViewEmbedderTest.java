// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.view;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.Build.API_LEVELS;
import java.util.List;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

@RunWith(AndroidJUnit4.class)
@Config(sdk = API_LEVELS.API_28)
public class AccessibilityViewEmbedderTest {

  @Test
  @SuppressWarnings("deprecation")
  public void itCopiesAccessibilityActionsForEmbeddedView() {
    Context context = ApplicationProvider.getApplicationContext();
    View mockRootView = new View(context);

    AccessibilityViewEmbedder embedder = new AccessibilityViewEmbedder(mockRootView, 1000);

    // Create a mock embedded view
    View mockEmbeddedView = mock(View.class);
    when(mockEmbeddedView.getContext()).thenReturn(context);

    AccessibilityNodeInfo embeddedRootNode = null;
    AccessibilityNodeInfo flutterRootNode = null;

    try {
      // Create a real AccessibilityNodeInfo for the embedded view's root
      embeddedRootNode = AccessibilityNodeInfo.obtain();
      embeddedRootNode.setSource(mockEmbeddedView, View.NO_ID);
      // Add some standard actions
      embeddedRootNode.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_CLICK);
      embeddedRootNode.addAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_LONG_CLICK);

      // Add a custom action
      AccessibilityNodeInfo.AccessibilityAction customAction =
          new AccessibilityNodeInfo.AccessibilityAction(0x7f010001, "Custom Action");
      embeddedRootNode.addAction(customAction);

      // Stub the embedded view to return this node info
      when(mockEmbeddedView.createAccessibilityNodeInfo()).thenReturn(embeddedRootNode);

      // Call getRootNode
      Rect bounds = new Rect(0, 0, 100, 100);
      flutterRootNode = embedder.getRootNode(mockEmbeddedView, 99, bounds);

      assertNotNull(flutterRootNode);

      // Verify that the actions are copied
      List<AccessibilityNodeInfo.AccessibilityAction> actions = flutterRootNode.getActionList();
      assertNotNull(actions);

      assertTrue(
          "Should have click action",
          actions.contains(AccessibilityNodeInfo.AccessibilityAction.ACTION_CLICK));
      assertTrue(
          "Should have long click action",
          actions.contains(AccessibilityNodeInfo.AccessibilityAction.ACTION_LONG_CLICK));
      assertTrue("Should have custom action", actions.contains(customAction));
    } finally {
      if (embeddedRootNode != null) {
        embeddedRootNode.recycle();
      }
      if (flutterRootNode != null) {
        flutterRootNode.recycle();
      }
    }
  }
}
