// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.plugin.platform;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.view.Window;
import io.flutter.Build.API_LEVELS;
import io.flutter.embedding.engine.systemchannels.PlatformChannel;
import java.util.ArrayList;
import java.util.List;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

@RunWith(RobolectricTestRunner.class)
public class PlatformPluginReproduceTest {
  private final PlatformChannel mockPlatformChannel = mock(PlatformChannel.class);

  @SuppressWarnings("deprecation")
  @Test
  @Config(sdk = API_LEVELS.API_29)
  public void testSystemUiOverlaysLayoutFlagsConsistency() {
    View fakeDecorView = mock(View.class);
    Window fakeWindow = mock(Window.class);
    Activity mockActivity = mock(Activity.class);
    when(fakeWindow.getDecorView()).thenReturn(fakeDecorView);
    when(mockActivity.getWindow()).thenReturn(fakeWindow);
    PlatformPlugin platformPlugin = new PlatformPlugin(mockActivity, mockPlatformChannel);

    ArgumentCaptor<Integer> flagsCaptor = ArgumentCaptor.forClass(Integer.class);

    // 1. Hide all overlays
    List<PlatformChannel.SystemUiOverlay> emptyOverlays = new ArrayList<>();
    platformPlugin.mPlatformMessageHandler.showSystemOverlays(emptyOverlays);

    // 2. Show only TOP_OVERLAYS (Status Bar)
    List<PlatformChannel.SystemUiOverlay> topOnly = new ArrayList<>();
    topOnly.add(PlatformChannel.SystemUiOverlay.TOP_OVERLAYS);
    platformPlugin.mPlatformMessageHandler.showSystemOverlays(topOnly);

    // 3. Show only BOTTOM_OVERLAYS (Navigation Bar)
    List<PlatformChannel.SystemUiOverlay> bottomOnly = new ArrayList<>();
    bottomOnly.add(PlatformChannel.SystemUiOverlay.BOTTOM_OVERLAYS);
    platformPlugin.mPlatformMessageHandler.showSystemOverlays(bottomOnly);

    // 4. Show all overlays
    List<PlatformChannel.SystemUiOverlay> allOverlays = new ArrayList<>();
    allOverlays.add(PlatformChannel.SystemUiOverlay.TOP_OVERLAYS);
    allOverlays.add(PlatformChannel.SystemUiOverlay.BOTTOM_OVERLAYS);
    platformPlugin.mPlatformMessageHandler.showSystemOverlays(allOverlays);

    // Verify setSystemUiVisibility was called 4 times and capture the flags.
    verify(fakeDecorView, times(4)).setSystemUiVisibility(flagsCaptor.capture());
    List<Integer> capturedFlags = flagsCaptor.getAllValues();

    int hideAllFlags = capturedFlags.get(0);
    int topOnlyFlags = capturedFlags.get(1);
    int bottomOnlyFlags = capturedFlags.get(2);
    int allFlags = capturedFlags.get(3);

    // Assert that LAYOUT_STABLE is always set.
    assertTrue((hideAllFlags & View.SYSTEM_UI_FLAG_LAYOUT_STABLE) != 0);
    assertTrue((topOnlyFlags & View.SYSTEM_UI_FLAG_LAYOUT_STABLE) != 0);
    assertTrue((bottomOnlyFlags & View.SYSTEM_UI_FLAG_LAYOUT_STABLE) != 0);
    assertTrue((allFlags & View.SYSTEM_UI_FLAG_LAYOUT_STABLE) != 0);

    // Assert that LAYOUT_FULLSCREEN is always set (stable layout for status bar).
    assertTrue((hideAllFlags & View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN) != 0);
    assertTrue((topOnlyFlags & View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN) != 0);
    assertTrue((bottomOnlyFlags & View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN) != 0);
    assertTrue((allFlags & View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN) != 0);

    // Assert that LAYOUT_HIDE_NAVIGATION should always be set (stable layout for navigation bar).
    // This is where it should fail for bottomOnlyFlags and allFlags.
    assertTrue((hideAllFlags & View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION) != 0);
    assertTrue((topOnlyFlags & View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION) != 0);

    // These two assertions are expected to FAIL in the buggy implementation.
    assertTrue("LAYOUT_HIDE_NAVIGATION should be set when only bottom overlay is shown",
        (bottomOnlyFlags & View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION) != 0);
    assertTrue("LAYOUT_HIDE_NAVIGATION should be set when all overlays are shown",
        (allFlags & View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION) != 0);
  }
}
