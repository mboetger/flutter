// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.view;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.provider.Settings;
import android.view.View;
import android.view.ViewParent;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.systemchannels.AccessibilityChannel;
import io.flutter.plugin.platform.PlatformViewsAccessibilityDelegate;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class AccessibilityBridgeFirebaseTestLabTest {

  private Context context;
  private View mockRootView;
  private AccessibilityChannel mockChannel;
  private AccessibilityManager mockManager;
  private AccessibilityViewEmbedder mockViewEmbedder;
  private PlatformViewsAccessibilityDelegate mockDelegate;
  private AccessibilityBridge accessibilityBridge;

  @Before
  public void setUp() {
    context = ApplicationProvider.getApplicationContext();
    mockRootView = mock(View.class);
    when(mockRootView.getContext()).thenReturn(context);
    mockChannel = mock(AccessibilityChannel.class);
    mockManager = mock(AccessibilityManager.class);
    when(mockManager.isEnabled()).thenReturn(false);
    mockViewEmbedder = mock(AccessibilityViewEmbedder.class);
    mockDelegate = mock(PlatformViewsAccessibilityDelegate.class);
  }

  @After
  public void tearDown() {
    if (accessibilityBridge != null) {
      accessibilityBridge.release();
    }
    // Clear the setting so it doesn't leak to other tests
    Settings.System.putString(context.getContentResolver(), "firebase.test.lab", null);
  }

  private void createBridge() {
    accessibilityBridge = new AccessibilityBridge(
        mockRootView,
        mockChannel,
        mockManager,
        context.getContentResolver(),
        mockViewEmbedder,
        mockDelegate
    );
  }

  @Test
  public void itForcesAccessibilityEnabledWhenRunningInFirebaseTestLab() {
    Settings.System.putString(context.getContentResolver(), "firebase.test.lab", "true");

    createBridge();

    assertTrue(accessibilityBridge.isAccessibilityEnabled());
    verify(mockChannel).onAndroidAccessibilityEnabled();
  }

  @Test
  public void itDoesNotForceAccessibilityEnabledWhenNotInFirebaseTestLab() {
    Settings.System.putString(context.getContentResolver(), "firebase.test.lab", "false");

    createBridge();

    assertFalse(accessibilityBridge.isAccessibilityEnabled());
    verify(mockChannel, never()).onAndroidAccessibilityEnabled();
  }

  @Test
  public void itDoesNotForceAccessibilityEnabledWhenSettingIsNull() {
    // Ensure it is null (default)
    Settings.System.putString(context.getContentResolver(), "firebase.test.lab", null);

    createBridge();

    assertFalse(accessibilityBridge.isAccessibilityEnabled());
    verify(mockChannel, never()).onAndroidAccessibilityEnabled();
  }

  @Test
  public void itSendsAccessibilityEventsWhenRunningInFirebaseTestLab() {
    Settings.System.putString(context.getContentResolver(), "firebase.test.lab", "true");

    ViewParent mockParent = mock(ViewParent.class);
    when(mockRootView.getParent()).thenReturn(mockParent);

    createBridge();

    accessibilityBridge.sendAccessibilityEvent(0, AccessibilityEvent.TYPE_VIEW_CLICKED);

    verify(mockParent)
        .requestSendAccessibilityEvent(eq(mockRootView), any(AccessibilityEvent.class));
  }

  @Test
  public void itDoesNotSendAccessibilityEventsWhenNotInFirebaseTestLabAndDisabled() {
    Settings.System.putString(context.getContentResolver(), "firebase.test.lab", "false");

    ViewParent mockParent = mock(ViewParent.class);
    when(mockRootView.getParent()).thenReturn(mockParent);

    createBridge();

    accessibilityBridge.sendAccessibilityEvent(0, AccessibilityEvent.TYPE_VIEW_CLICKED);

    verify(mockParent, never())
        .requestSendAccessibilityEvent(any(View.class), any(AccessibilityEvent.class));
  }
}
