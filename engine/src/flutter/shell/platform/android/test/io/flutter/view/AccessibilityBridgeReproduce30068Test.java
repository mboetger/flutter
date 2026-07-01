// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.view;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;
import android.view.ViewParent;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.systemchannels.AccessibilityChannel;
import io.flutter.plugin.platform.PlatformViewsAccessibilityDelegate;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class AccessibilityBridgeReproduce30068Test extends AccessibilityBridgeTest {

  @SuppressWarnings("deprecation")
  @Test
  public void testEmbeddedViewAccessibilityEventBeforeSemanticsTree() {
    // 1. Setup the bridge with a real AccessibilityViewEmbedder.
    View mockRootView = mock(View.class);
    Context context = mock(Context.class);
    when(mockRootView.getContext()).thenReturn(context);
    when(context.getPackageName()).thenReturn("test");

    // Mock the parent of rootAccessibilityView to avoid NPE when sending events.
    ViewParent mockParent = mock(ViewParent.class);
    when(mockRootView.getParent()).thenReturn(mockParent);

    // We use a real AccessibilityViewEmbedder.
    AccessibilityViewEmbedder accessibilityViewEmbedder =
        new AccessibilityViewEmbedder(mockRootView, 65536);

    PlatformViewsAccessibilityDelegate mockDelegate = mock(PlatformViewsAccessibilityDelegate.class);
    AccessibilityChannel mockChannel = mock(AccessibilityChannel.class);

    AccessibilityBridge accessibilityBridge = setUpBridge(
        mockRootView,
        mockChannel,
        null, // accessibilityManager
        null, // contentResolver
        accessibilityViewEmbedder,
        mockDelegate
    );

    // 2. Create a mock embedded view with a mock provider and node info.
    View mockEmbeddedView = mock(View.class);
    AccessibilityNodeProvider mockProvider = mock(AccessibilityNodeProvider.class);
    AccessibilityNodeInfo embeddedNodeInfo = AccessibilityNodeInfo.obtain();
    embeddedNodeInfo.setClassName("android.view.View");

    when(mockEmbeddedView.getAccessibilityNodeProvider()).thenReturn(mockProvider);
    // The source ID is set to 1 in step 3, so we expect a query for ID 1.
    when(mockProvider.createAccessibilityNodeInfo(1)).thenReturn(embeddedNodeInfo);

    AccessibilityNodeInfo nodeInfo = null;
    try {
      // 3. Create an accessibility event originating from the embedded view.
      AccessibilityEvent event = AccessibilityEvent.obtain();
      // We must set the source of the event so that reflection accessors can extract the source ID.
      event.setSource(mockEmbeddedView, 1);

      // 4. Send the event to the bridge.
      // This will allocate a flutterId on the fly and cache the mapping.
      accessibilityBridge.externalViewRequestSendAccessibilityEvent(mockEmbeddedView, mockEmbeddedView, event);

      // Retrieve the allocated flutterId using the embedder's mapping.
      Integer flutterId = accessibilityViewEmbedder.getRecordFlutterId(mockEmbeddedView, event);
      assertNotNull("Expected a flutterId to be allocated for the event", flutterId);

      // 5. Query the bridge for the node info of this flutterId.
      // This should return the node, but currently it returns null because the bounds are not known.
      nodeInfo = accessibilityBridge.createAccessibilityNodeInfo(flutterId);

      // We assert that it is NOT null. This assertion should FAIL, reproducing the bug.
      assertNotNull("AccessibilityNodeInfo should not be null even if bounds are not yet known", nodeInfo);
    } finally {
      if (nodeInfo != null) {
        nodeInfo.recycle();
      }
      embeddedNodeInfo.recycle();
    }
  }

  @SuppressWarnings("deprecation")
  @Test
  public void testOnAccessibilityHoverEventBeforeSemanticsTree() {
    // 1. Setup the bridge with a real AccessibilityViewEmbedder.
    View mockRootView = mock(View.class);
    Context context = mock(Context.class);
    when(mockRootView.getContext()).thenReturn(context);
    when(context.getPackageName()).thenReturn("test");

    ViewParent mockParent = mock(ViewParent.class);
    when(mockRootView.getParent()).thenReturn(mockParent);

    AccessibilityViewEmbedder accessibilityViewEmbedder =
        new AccessibilityViewEmbedder(mockRootView, 65536);

    PlatformViewsAccessibilityDelegate mockDelegate = mock(PlatformViewsAccessibilityDelegate.class);
    AccessibilityChannel mockChannel = mock(AccessibilityChannel.class);

    AccessibilityBridge accessibilityBridge = setUpBridge(
        mockRootView,
        mockChannel,
        null, // accessibilityManager
        null, // contentResolver
        accessibilityViewEmbedder,
        mockDelegate
    );

    // 2. Create a mock embedded view.
    View mockEmbeddedView = mock(View.class);
    AccessibilityNodeProvider mockProvider = mock(AccessibilityNodeProvider.class);
    when(mockEmbeddedView.getAccessibilityNodeProvider()).thenReturn(mockProvider);

    // 3. Simulate an event to register the embedded view in the embedder.
    AccessibilityEvent event = AccessibilityEvent.obtain();
    event.setSource(mockEmbeddedView, 1);
    accessibilityBridge.externalViewRequestSendAccessibilityEvent(mockEmbeddedView, mockEmbeddedView, event);

    Integer flutterId = accessibilityViewEmbedder.getRecordFlutterId(mockEmbeddedView, event);
    assertNotNull("Expected a flutterId to be allocated", flutterId);

    // 4. Call the embedder's hover event directly to verify it handles null bounds.
    // We use a real MotionEvent via Robolectric's MotionEvent.obtain.
    android.view.MotionEvent hoverEvent = android.view.MotionEvent.obtain(
        /* downTime */ 0,
        /* eventTime */ 0,
        android.view.MotionEvent.ACTION_HOVER_ENTER,
        /* x */ 0,
        /* y */ 0,
        /* metaState */ 0
    );

    try {
      // This should return false and NOT throw NullPointerException.
      boolean handled = accessibilityViewEmbedder.onAccessibilityHoverEvent(flutterId, hoverEvent);
      org.junit.Assert.assertFalse("Hover event should not be handled when bounds are unknown", handled);
    } finally {
      hoverEvent.recycle();
    }
  }
}
