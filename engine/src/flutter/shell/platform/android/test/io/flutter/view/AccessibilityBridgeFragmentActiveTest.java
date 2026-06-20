// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.view;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;
import androidx.fragment.app.FragmentActivity;
import androidx.test.core.app.ActivityScenario;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.Build.API_LEVELS;
import io.flutter.embedding.android.FlutterFragment;
import io.flutter.embedding.android.FlutterView;
import io.flutter.embedding.android.TransparencyMode;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.embedding.engine.FlutterJNI;
import io.flutter.embedding.engine.loader.FlutterLoader;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowAccessibilityManager;

@Config(sdk = API_LEVELS.API_28)
@RunWith(AndroidJUnit4.class)
public class AccessibilityBridgeFragmentActiveTest {

  private final Context ctx = ApplicationProvider.getApplicationContext();

  @Test
  public void testTalkbackRectangleBoundsInFragment() {
    // 1. Mock FlutterJNI and FlutterEngine to avoid native crashes
    FlutterJNI mockFlutterJni = mock(FlutterJNI.class);
    when(mockFlutterJni.isAttached()).thenReturn(true);
    FlutterLoader mockFlutterLoader = mock(FlutterLoader.class);
    FlutterEngine flutterEngine = new FlutterEngine(ctx, mockFlutterLoader, mockFlutterJni);

    try {
      // 2. Launch a real FragmentActivity using ActivityScenario
      try (ActivityScenario<FragmentActivity> scenario = ActivityScenario.launch(FragmentActivity.class)) {
        scenario.onActivity(activity -> {
          // Enable accessibility in Robolectric shadows
          ShadowAccessibilityManager shadowAccessibilityManager = Shadows.shadowOf(
              (AccessibilityManager) activity.getSystemService(Context.ACCESSIBILITY_SERVICE)
          );
          shadowAccessibilityManager.setEnabled(true);
          shadowAccessibilityManager.setTouchExplorationEnabled(true);

          // 3. Create a parent container FrameLayout with an offset/margin
          FrameLayout container = new FrameLayout(activity);
          FrameLayout.LayoutParams containerParams = new FrameLayout.LayoutParams(
              ViewGroup.LayoutParams.MATCH_PARENT,
              ViewGroup.LayoutParams.MATCH_PARENT
          );
          // Set padding/margin on the container to position it away from (0,0)
          container.setPadding(100, 200, 0, 0);
          activity.setContentView(container, containerParams);

          // 4. Create a real FlutterView and add it to the container
          FlutterView flutterView = new FlutterView(activity);
          FrameLayout.LayoutParams flutterViewParams = new FrameLayout.LayoutParams(
              300,
              400
          );
          container.addView(flutterView, flutterViewParams);

          // 5. Attach FlutterView to the engine
          flutterView.attachToFlutterEngine(flutterEngine);

          // 6. Force layout pass so coordinates on screen are populated
          container.measure(
              View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY),
              View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY)
          );
          container.layout(0, 0, 1000, 1000);

          // Verify the view is indeed laid out and has correct location on screen
          int[] locationOnScreen = new int[2];
          flutterView.getLocationOnScreen(locationOnScreen);
          assertEquals(100, locationOnScreen[0]);
          assertEquals(256, locationOnScreen[1]);

          // Get the AccessibilityBridge from FlutterView
          AccessibilityBridge accessibilityBridge = (AccessibilityBridge) flutterView.getAccessibilityNodeProvider();
          assertNotNull("AccessibilityBridge should not be null", accessibilityBridge);

          // 7. Simulate a semantics update using AccessibilityBridgeTest helper classes
          // to avoid brittle, manual ByteBuffer generation.
          AccessibilityBridgeTest helper = new AccessibilityBridgeTest();
          AccessibilityBridgeTest.TestSemanticsNode rootNode = helper.new TestSemanticsNode();
          rootNode.id = 0;
          rootNode.left = 0.0f;
          rootNode.top = 0.0f;
          rootNode.right = 300.0f;
          rootNode.bottom = 400.0f;

          AccessibilityBridgeTest.TestSemanticsUpdate update = rootNode.toUpdate();
          update.sendUpdateToBridge(accessibilityBridge);

          // 8. Retrieve AccessibilityNodeInfo for root (0)
          AccessibilityNodeInfo nodeInfo = accessibilityBridge.createAccessibilityNodeInfo(0);
          assertNotNull("AccessibilityNodeInfo for root should not be null", nodeInfo);

          // 9. Verify the bounds in screen of the root node
          // Local bounds: (0, 0, 300, 400)
          // FlutterView is at (100, 256) on screen (200px padding + 56px window/status bar decoration)
          // So bounds in screen should be (100, 256, 400, 656)
          Rect boundsInScreen = new Rect();
          nodeInfo.getBoundsInScreen(boundsInScreen);

          assertEquals("Bounds in screen left should match view screen position", locationOnScreen[0], boundsInScreen.left);
          assertEquals("Bounds in screen top should match view screen position", locationOnScreen[1], boundsInScreen.top);
          assertEquals("Bounds in screen right should match view screen position + width", locationOnScreen[0] + 300, boundsInScreen.right);
          assertEquals("Bounds in screen bottom should match view screen position + height", locationOnScreen[1] + 400, boundsInScreen.bottom);
        });
      }
    } finally {
      // Ensure the FlutterEngine is properly destroyed to avoid memory and resource leaks.
      flutterEngine.destroy();
    }
  }

  @Test
  public void testDefaultFlutterFragmentIsOpaque() {
    // By default, FlutterFragment must be opaque so that the underlying FlutterSurfaceView
    // does not call setZOrderOnTop(true), which would obscure the TalkBack focus rectangle.
    FlutterFragment fragment = FlutterFragment.createDefault();
    assertEquals(TransparencyMode.opaque, fragment.getTransparencyMode());
  }
}
