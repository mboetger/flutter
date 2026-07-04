// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.annotation.TargetApi;
import android.content.Context;
import android.view.View;
import androidx.activity.BackEventCompat;
import androidx.activity.OnBackPressedCallback;
import androidx.activity.OnBackPressedDispatcher;
import androidx.fragment.app.FragmentActivity;
import androidx.test.core.app.ActivityScenario;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.Build;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.embedding.engine.FlutterEngineCache;
import io.flutter.embedding.engine.FlutterJNI;
import io.flutter.embedding.engine.loader.FlutterLoader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.atomic.AtomicBoolean;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

@RunWith(AndroidJUnit4.class)
public class FlutterFragmentTest {
  private final Context ctx = ApplicationProvider.getApplicationContext();
  boolean isDelegateAttached;

  class TestDelegateFactory implements FlutterActivityAndFragmentDelegate.DelegateFactory {
    FlutterActivityAndFragmentDelegate delegate;

    TestDelegateFactory(FlutterActivityAndFragmentDelegate delegate) {
      this.delegate = delegate;
    }

    public FlutterActivityAndFragmentDelegate createDelegate(
        FlutterActivityAndFragmentDelegate.Host host) {
      return delegate;
    }
  }

  @Test
  public void itCreatesDefaultFragmentWithExpectedDefaults() {
    FlutterFragment fragment = FlutterFragment.createDefault();
    TestDelegateFactory delegateFactory =
        new TestDelegateFactory(new FlutterActivityAndFragmentDelegate(fragment));
    fragment.setDelegateFactory(delegateFactory);

    assertEquals("main", fragment.getDartEntrypointFunctionName());
    assertNull(fragment.getDartEntrypointLibraryUri());
    assertNull(fragment.getDartEntrypointArgs());
    assertEquals("/", fragment.getInitialRoute());
    assertArrayEquals(new String[] {}, fragment.getFlutterShellArgs().toArray());
    assertTrue(fragment.shouldAttachEngineToActivity());
    assertFalse(fragment.shouldHandleDeeplinking());
    assertNull(fragment.getCachedEngineId());
    assertTrue(fragment.shouldDestroyEngineWithHost());
    assertEquals(RenderMode.surface, fragment.getRenderMode());
    assertEquals(TransparencyMode.transparent, fragment.getTransparencyMode());
    assertFalse(fragment.shouldDelayFirstAndroidViewDraw());
  }

  @Test
  public void itCreatesNewEngineFragmentWithRequestedSettings() {
    FlutterFragment fragment =
        FlutterFragment.withNewEngine()
            .dartEntrypoint("custom_entrypoint")
            .dartLibraryUri("package:foo/bar.dart")
            .dartEntrypointArgs(new ArrayList<String>(Arrays.asList("foo", "bar")))
            .initialRoute("/custom/route")
            .shouldAttachEngineToActivity(false)
            .handleDeeplinking(true)
            .renderMode(RenderMode.texture)
            .transparencyMode(TransparencyMode.opaque)
            .build();
    TestDelegateFactory delegateFactory =
        new TestDelegateFactory(new FlutterActivityAndFragmentDelegate(fragment));
    fragment.setDelegateFactory(delegateFactory);

    assertEquals("custom_entrypoint", fragment.getDartEntrypointFunctionName());
    assertEquals("package:foo/bar.dart", fragment.getDartEntrypointLibraryUri());
    assertEquals("/custom/route", fragment.getInitialRoute());
    assertArrayEquals(new String[] {"foo", "bar"}, fragment.getDartEntrypointArgs().toArray());
    assertArrayEquals(new String[] {}, fragment.getFlutterShellArgs().toArray());
    assertFalse(fragment.shouldAttachEngineToActivity());
    assertTrue(fragment.shouldHandleDeeplinking());
    assertNull(fragment.getCachedEngineId());
    assertTrue(fragment.shouldDestroyEngineWithHost());
    assertEquals(RenderMode.texture, fragment.getRenderMode());
    assertEquals(TransparencyMode.opaque, fragment.getTransparencyMode());
  }

  @Test
  public void itCreatesNewEngineInGroupFragmentWithRequestedSettings() {
    FlutterFragment fragment =
        FlutterFragment.withNewEngineInGroup("my_cached_engine_group")
            .dartEntrypoint("custom_entrypoint")
            .initialRoute("/custom/route")
            .shouldAttachEngineToActivity(false)
            .handleDeeplinking(true)
            .renderMode(RenderMode.texture)
            .transparencyMode(TransparencyMode.opaque)
            .build();

    TestDelegateFactory delegateFactory =
        new TestDelegateFactory(new FlutterActivityAndFragmentDelegate(fragment));

    fragment.setDelegateFactory(delegateFactory);

    assertEquals("my_cached_engine_group", fragment.getCachedEngineGroupId());
    assertEquals("custom_entrypoint", fragment.getDartEntrypointFunctionName());
    assertEquals("/custom/route", fragment.getInitialRoute());
    assertArrayEquals(new String[] {}, fragment.getFlutterShellArgs().toArray());
    assertFalse(fragment.shouldAttachEngineToActivity());
    assertTrue(fragment.shouldHandleDeeplinking());
    assertNull(fragment.getCachedEngineId());
    assertTrue(fragment.shouldDestroyEngineWithHost());
    assertEquals(RenderMode.texture, fragment.getRenderMode());
    assertEquals(TransparencyMode.opaque, fragment.getTransparencyMode());
  }

  @Test
  public void itCreatesNewEngineFragmentThatDelaysFirstDrawWhenRequested() {
    FlutterFragment fragment =
        FlutterFragment.withNewEngine().shouldDelayFirstAndroidViewDraw(true).build();

    assertNotNull(fragment.shouldDelayFirstAndroidViewDraw());
  }

  @Test
  public void itCreatesCachedEngineFragmentWithExpectedDefaults() {
    FlutterFragment fragment = FlutterFragment.withCachedEngine("my_cached_engine").build();

    assertTrue(fragment.shouldAttachEngineToActivity());
    assertEquals("my_cached_engine", fragment.getCachedEngineId());
    assertFalse(fragment.shouldDestroyEngineWithHost());
    assertFalse(fragment.shouldDelayFirstAndroidViewDraw());
  }

  @Test
  public void itCreatesCachedEngineFragmentThatDestroysTheEngine() {
    FlutterFragment fragment =
        FlutterFragment.withCachedEngine("my_cached_engine")
            .destroyEngineWithFragment(true)
            .build();

    assertTrue(fragment.shouldAttachEngineToActivity());
    assertEquals("my_cached_engine", fragment.getCachedEngineId());
    assertTrue(fragment.shouldDestroyEngineWithHost());
  }

  @Test
  public void itCreatesCachedEngineFragmentThatDelaysFirstDrawWhenRequested() {
    FlutterFragment fragment =
        FlutterFragment.withCachedEngine("my_cached_engine")
            .shouldDelayFirstAndroidViewDraw(true)
            .build();

    assertNotNull(fragment.shouldDelayFirstAndroidViewDraw());
  }

  @Test
  public void itCanBeDetachedFromTheEngineAndStopSendingFurtherEvents() {
    FlutterActivityAndFragmentDelegate mockDelegate =
        mock(FlutterActivityAndFragmentDelegate.class);
    TestDelegateFactory delegateFactory = new TestDelegateFactory(mockDelegate);
    FlutterFragment fragment =
        FlutterFragment.withCachedEngine("my_cached_engine")
            .destroyEngineWithFragment(true)
            .build();

    isDelegateAttached = true;
    when(mockDelegate.isAttached()).thenAnswer(invocation -> isDelegateAttached);
    doAnswer(invocation -> isDelegateAttached = false).when(mockDelegate).onDetach();

    fragment.setDelegateFactory(delegateFactory);
    fragment.onStart();
    fragment.onResume();
    fragment.onPostResume();

    verify(mockDelegate, times(1)).onStart();
    verify(mockDelegate, times(1)).onResume();
    verify(mockDelegate, times(1)).onPostResume();

    fragment.onPause();
    fragment.detachFromFlutterEngine();
    verify(mockDelegate, times(1)).onPause();
    verify(mockDelegate, times(1)).onDestroyView();
    verify(mockDelegate, times(1)).onDetach();

    fragment.onStop();
    verify(mockDelegate, never()).onStop();

    fragment.onStart();
    fragment.onResume();
    fragment.onPostResume();
    // No more events through to the delegate.
    verify(mockDelegate, times(1)).onStart();
    verify(mockDelegate, times(1)).onResume();
    verify(mockDelegate, times(1)).onPostResume();

    fragment.onDestroy();
    // 1 time same as before.
    verify(mockDelegate, times(1)).onDestroyView();
    verify(mockDelegate, times(1)).onDetach();
  }

  @Test
  public void itDoesNotReleaseEnginewhenDetachFromFlutterEngine() {
    FlutterActivityAndFragmentDelegate mockDelegate =
        mock(FlutterActivityAndFragmentDelegate.class);
    isDelegateAttached = true;
    when(mockDelegate.isAttached()).thenAnswer(invocation -> isDelegateAttached);
    doAnswer(invocation -> isDelegateAttached = false).when(mockDelegate).onDetach();
    TestDelegateFactory delegateFactory = new TestDelegateFactory(mockDelegate);

    FlutterFragment fragment =
        FlutterFragment.withCachedEngine("my_cached_engine")
            .destroyEngineWithFragment(true)
            .build();

    fragment.setDelegateFactory(delegateFactory);
    fragment.onStart();
    fragment.onResume();
    fragment.onPostResume();
    fragment.onPause();

    assertTrue(mockDelegate.isAttached());
    fragment.detachFromFlutterEngine();
    verify(mockDelegate, times(1)).onDetach();
    verify(mockDelegate, never()).release();
    assertFalse(mockDelegate.isAttached());
  }

  @Test
  public void itReleaseEngineWhenOnDetach() {
    FlutterActivityAndFragmentDelegate mockDelegate =
        mock(FlutterActivityAndFragmentDelegate.class);
    isDelegateAttached = true;
    when(mockDelegate.isAttached()).thenAnswer(invocation -> isDelegateAttached);
    doAnswer(invocation -> isDelegateAttached = false).when(mockDelegate).onDetach();
    TestDelegateFactory delegateFactory = new TestDelegateFactory(mockDelegate);

    FlutterFragment fragment =
        spy(
            FlutterFragment.withCachedEngine("my_cached_engine")
                .destroyEngineWithFragment(true)
                .build());
    when(fragment.getContext()).thenReturn(mock(Context.class));

    fragment.setDelegateFactory(delegateFactory);
    fragment.onStart();
    fragment.onResume();
    fragment.onPostResume();
    fragment.onPause();

    assertTrue(mockDelegate.isAttached());
    fragment.onDetach();
    verify(mockDelegate, times(1)).onDetach();
    verify(mockDelegate, times(1)).release();
    assertFalse(mockDelegate.isAttached());
  }

  @Test
  public void itReturnsExclusiveAppComponent() {
    FlutterFragment fragment = FlutterFragment.createDefault();
    FlutterActivityAndFragmentDelegate delegate = new FlutterActivityAndFragmentDelegate(fragment);
    TestDelegateFactory delegateFactory = new TestDelegateFactory(delegate);
    fragment.setDelegateFactory(delegateFactory);

    assertEquals(fragment.getExclusiveAppComponent(), delegate);
  }

  @Test
  @Config(sdk = Build.API_LEVELS.API_33)
  @TargetApi(Build.API_LEVELS.API_33)
  public void itDelegatesOnBackPressedWithSetFrameworkHandlesBackForSdk33() {
    // We need to mock FlutterJNI to avoid triggering native code.
    FlutterJNI flutterJNI = mock(FlutterJNI.class);
    when(flutterJNI.isAttached()).thenReturn(true);

    FlutterEngine flutterEngine =
        new FlutterEngine(ctx, new FlutterLoader(), flutterJNI, null, false);
    FlutterEngineCache.getInstance().put("my_cached_engine", flutterEngine);

    FlutterFragment fragment =
        FlutterFragment.withCachedEngine("my_cached_engine")
            // This enables the use of onBackPressedCallback, which is what
            // sends backs to the framework if setFrameworkHandlesBack is true.
            .shouldAutomaticallyHandleOnBackPressed(true)
            .build();

    try (ActivityScenario<FragmentActivity> scenario =
        ActivityScenario.launch(FragmentActivity.class)) {
      scenario.onActivity(
          activity -> {
            activity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(android.R.id.content, fragment)
                .commitNow();

            FlutterActivityAndFragmentDelegate mockDelegate =
                mock(FlutterActivityAndFragmentDelegate.class);
            isDelegateAttached = true;
            when(mockDelegate.isAttached()).thenAnswer(invocation -> isDelegateAttached);
            doAnswer(invocation -> isDelegateAttached = false).when(mockDelegate).onDetach();
            TestDelegateFactory delegateFactory = new TestDelegateFactory(mockDelegate);
            fragment.setDelegateFactory(delegateFactory);

            // Calling onBackPressed now will still be handled by Android (the default),
            // until setFrameworkHandlesBack is set to true.
            activity.getOnBackPressedDispatcher().onBackPressed();
            verify(mockDelegate, times(0)).onBackPressed();

            // Setting setFrameworkHandlesBack to true means the delegate will receive
            // the back and Android won't handle it.
            fragment.setFrameworkHandlesBack(true);
            activity.getOnBackPressedDispatcher().onBackPressed();
            verify(mockDelegate, times(1)).onBackPressed();
          });
    }
  }

  @Test
  @Config(sdk = Build.API_LEVELS.API_34)
  @TargetApi(Build.API_LEVELS.API_34)
  public void itDelegatesOnBackPressedWithSetFrameworkHandlesBackForSdk34OrHigher() {
    // We need to mock FlutterJNI to avoid triggering native code.
    FlutterJNI flutterJNI = mock(FlutterJNI.class);
    when(flutterJNI.isAttached()).thenReturn(true);

    FlutterEngine flutterEngine =
        new FlutterEngine(ctx, new FlutterLoader(), flutterJNI, null, false);
    FlutterEngineCache.getInstance().put("my_cached_engine", flutterEngine);

    FlutterFragment fragment =
        FlutterFragment.withCachedEngine("my_cached_engine")
            // This enables the use of onBackPressedCallback, which is what
            // sends backs to the framework if setFrameworkHandlesBack is true.
            .shouldAutomaticallyHandleOnBackPressed(true)
            .build();

    try (ActivityScenario<FragmentActivity> scenario =
        ActivityScenario.launch(FragmentActivity.class)) {
      scenario.onActivity(
          activity -> {
            activity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(android.R.id.content, fragment)
                .commitNow();

            FlutterActivityAndFragmentDelegate mockDelegate =
                mock(FlutterActivityAndFragmentDelegate.class);
            isDelegateAttached = true;
            when(mockDelegate.isAttached()).thenAnswer(invocation -> isDelegateAttached);
            doAnswer(invocation -> isDelegateAttached = false).when(mockDelegate).onDetach();
            TestDelegateFactory delegateFactory = new TestDelegateFactory(mockDelegate);
            fragment.setDelegateFactory(delegateFactory);

            BackEventCompat mockBackEvent = mock(BackEventCompat.class);
            OnBackPressedDispatcher dispatcher = activity.getOnBackPressedDispatcher();

            // Back gesture events now will still be handled by Android (the default),
            // until setFrameworkHandlesBack is set to true.
            dispatcher.dispatchOnBackStarted(mockBackEvent);
            dispatcher.dispatchOnBackProgressed(mockBackEvent);
            dispatcher.onBackPressed();
            dispatcher.dispatchOnBackCancelled();
            verify(mockDelegate, times(0)).startBackGesture(any());
            verify(mockDelegate, times(0)).updateBackGestureProgress(any());
            verify(mockDelegate, times(0)).commitBackGesture();
            verify(mockDelegate, times(0)).cancelBackGesture();

            // Setting setFrameworkHandlesBack to true means the delegate will receive
            // the back and Android won't handle it.
            fragment.setFrameworkHandlesBack(true);
            dispatcher.dispatchOnBackStarted(mockBackEvent);
            dispatcher.dispatchOnBackProgressed(mockBackEvent);
            dispatcher.onBackPressed();
            dispatcher.dispatchOnBackCancelled();
            verify(mockDelegate, times(1)).startBackGesture(any());
            verify(mockDelegate, times(1)).updateBackGestureProgress(any());
            verify(mockDelegate, times(1)).commitBackGesture();
            verify(mockDelegate, times(1)).cancelBackGesture();
          });
    }
  }

  @Test
  public void itHandlesPopSystemNavigationAutomaticallyWhenEnabled() {
    // We need to mock FlutterJNI to avoid triggering native code.
    FlutterJNI flutterJNI = mock(FlutterJNI.class);
    when(flutterJNI.isAttached()).thenReturn(true);

    FlutterEngine flutterEngine =
        new FlutterEngine(ctx, new FlutterLoader(), flutterJNI, null, false);
    FlutterEngineCache.getInstance().put("my_cached_engine", flutterEngine);

    FlutterFragment fragment =
        FlutterFragment.withCachedEngine("my_cached_engine")
            .shouldAutomaticallyHandleOnBackPressed(true)
            .build();

    try (ActivityScenario<FragmentActivity> scenario =
        ActivityScenario.launch(FragmentActivity.class)) {
      scenario.onActivity(
          activity -> {
            activity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(android.R.id.content, fragment)
                .commitNow();
            final AtomicBoolean onBackPressedCalled = new AtomicBoolean(false);
            OnBackPressedCallback callback =
                new OnBackPressedCallback(true) {
                  @Override
                  public void handleOnBackPressed() {
                    onBackPressedCalled.set(true);
                  }
                };
            activity.getOnBackPressedDispatcher().addCallback(callback);

            FlutterActivityAndFragmentDelegate mockDelegate =
                mock(FlutterActivityAndFragmentDelegate.class);
            TestDelegateFactory delegateFactory = new TestDelegateFactory(mockDelegate);
            fragment.setDelegateFactory(delegateFactory);

            assertTrue(callback.isEnabled());

            assertTrue(fragment.popSystemNavigator());

            verify(mockDelegate, never()).onBackPressed();
            assertTrue(onBackPressedCalled.get());
            assertTrue(callback.isEnabled());

            callback.setEnabled(false);
            assertFalse(callback.isEnabled());
            assertTrue(fragment.popSystemNavigator());

            verify(mockDelegate, never()).onBackPressed();
            assertFalse(callback.isEnabled());
          });
    }
  }

  @Test
  public void itRegistersComponentCallbacks() {
    FlutterActivityAndFragmentDelegate mockDelegate =
        mock(FlutterActivityAndFragmentDelegate.class);
    isDelegateAttached = true;
    when(mockDelegate.isAttached()).thenAnswer(invocation -> isDelegateAttached);
    doAnswer(invocation -> isDelegateAttached = false).when(mockDelegate).onDetach();
    TestDelegateFactory delegateFactory = new TestDelegateFactory(mockDelegate);

    Context spyCtx = spy(ctx);
    // We need to mock FlutterJNI to avoid triggering native code.
    FlutterJNI flutterJNI = mock(FlutterJNI.class);
    when(flutterJNI.isAttached()).thenReturn(true);

    FlutterEngine flutterEngine =
        new FlutterEngine(spyCtx, new FlutterLoader(), flutterJNI, null, false);
    FlutterEngineCache.getInstance().put("my_cached_engine", flutterEngine);

    FlutterFragment fragment = spy(FlutterFragment.withCachedEngine("my_cached_engine").build());
    when(fragment.getContext()).thenReturn(spyCtx);
    fragment.setDelegateFactory(delegateFactory);

    fragment.onAttach(spyCtx);
    verify(spyCtx, times(1)).registerComponentCallbacks(any());
    verify(spyCtx, never()).unregisterComponentCallbacks(any());

    fragment.onDetach();
    verify(spyCtx, times(1)).registerComponentCallbacks(any());
    verify(spyCtx, times(1)).unregisterComponentCallbacks(any());
  }

  @Test
  public void itAllowsSwitchingCachedEngineBetweenFragmentsWithoutCrash() {
    // Models issue flutter/flutter#66632: an app shows a main program Fragment,
    // and simultaneously opens a Fragment suspension layer on top (sharing the same FlutterEngine).
    // Closing and reopening the suspension layer previously caused concurrent engine attachment
    // and surface rendering races, leading to SIGABRT / FlutterJNI errors.
    FlutterActivityAndFragmentDelegate mockDelegate1 =
        mock(FlutterActivityAndFragmentDelegate.class);
    // Hardcoding isAttached() to true isolates delegate preservation and event forwarding
    // without needing to mock full Activity re-attachment across fragment transactions.
    when(mockDelegate1.isAttached()).thenReturn(true);
    TestDelegateFactory delegateFactory1 = new TestDelegateFactory(mockDelegate1);

    FlutterActivityAndFragmentDelegate mockDelegate2 =
        mock(FlutterActivityAndFragmentDelegate.class);
    when(mockDelegate2.isAttached()).thenReturn(true);
    TestDelegateFactory delegateFactory2 = new TestDelegateFactory(mockDelegate2);

    FlutterFragment fragment1 =
        FlutterFragment.withCachedEngine("my_cached_engine")
            .destroyEngineWithFragment(false)
            .build();
    fragment1.setDelegateFactory(delegateFactory1);

    FlutterFragment fragment2 =
        FlutterFragment.withCachedEngine("my_cached_engine")
            .destroyEngineWithFragment(false)
            .build();
    fragment2.setDelegateFactory(delegateFactory2);

    // 1. Start main program fragment.
    fragment1.onStart();
    fragment1.onResume();
    verify(mockDelegate1, times(1)).onStart();
    verify(mockDelegate1, times(1)).onResume();

    // 2. Open suspension layer fragment (evicting fragment1 from engine).
    // When fragment2 attaches to the shared engine, ExclusiveAppComponent enforcement
    // invokes fragment1.detachFromFlutterEngine().
    fragment1.detachFromFlutterEngine();
    // Verifying onDestroyView is critical: it ensures the rendering surface
    // (FlutterView/FlutterTextureView)
    // is cleanly detached from the engine upon eviction, preventing concurrent surface JNI
    // callbacks and SIGABRT.
    verify(mockDelegate1, times(1)).onDestroyView();
    verify(mockDelegate1, times(1)).onDetach();
    // Ensure temporary eviction from the engine does not prematurely release or destroy the
    // delegate.
    verify(mockDelegate1, never()).release();

    fragment2.onStart();
    fragment2.onResume();
    verify(mockDelegate2, times(1)).onStart();
    verify(mockDelegate2, times(1)).onResume();

    // 3. Close suspension layer fragment.
    fragment2.onPause();
    fragment2.onStop();
    fragment2.detachFromFlutterEngine();
    verify(mockDelegate2, times(1)).onPause();
    verify(mockDelegate2, times(1)).onStop();
    verify(mockDelegate2, times(1)).onDestroyView();
    verify(mockDelegate2, times(1)).onDetach();
    verify(mockDelegate2, never()).release();

    // 4. Re-show / re-attach main program fragment.
    // In real Android execution, when returning to an evicted fragment whose view was destroyed
    // (via onDestroyView), Android re-creates the view hierarchy (onCreateView), re-establishing
    // the delegate connection. When the suspension layer is removed, the main program fragment
    // re-attaches to the shared engine, allowing normal lifecycle event forwarding to resume
    // without crashing or surface JNI races.
    fragment1.onStart();
    fragment1.onResume();
    verify(mockDelegate1, times(2)).onStart();
    verify(mockDelegate1, times(2)).onResume();
  }

  @Test
  public void
      itReproducesIssue68269_surfaceModeWithCachedEngineShowsRouteBelowPreviousWhenUsingHybridComposition() {
    // Reproduces issue flutter/flutter#68269:
    // In add2app with a cached FlutterEngine and FlutterFragment using RenderMode.surface,
    // navigating from a previous route (Fragment 1) to a new route with WebView / Hybrid
    // Composition
    // (Fragment 2) causes the new route to be visually shown BELOW the previous route, even though
    // gestures and button clicks still go to the WebView.
    //
    // Root cause:
    // 1. In RenderMode.surface, FlutterFragment defaults to TransparencyMode.transparent, which
    //    calls setZOrderOnTop(true) on Fragment 1's FlutterSurfaceView.
    // 2. When Fragment 2 attaches to the cached engine, Fragment 1 is detached from the engine.
    //    However, detaching does not change the visibility of Fragment 1's FlutterSurfaceView.
    // 3. When Hybrid Composition (SurfaceAndroidWebView) initializes in Fragment 2,
    //    PlatformViewsController converts Fragment 2's FlutterView to use FlutterImageView
    //    (a standard Android View in the window hierarchy).
    // 4. Because Fragment 1's FlutterSurfaceView remains View.VISIBLE with setZOrderOnTop(true),
    //    SurfaceFlinger composites Fragment 1's surface above all window views, visually covering
    //    Fragment 2's WebView and FlutterImageView, while touch events still go to Fragment 2.
    Context spyCtx = spy(ctx);
    FlutterJNI flutterJNI = mock(FlutterJNI.class);
    when(flutterJNI.isAttached()).thenReturn(true);

    FlutterEngine flutterEngine =
        new FlutterEngine(spyCtx, new FlutterLoader(), flutterJNI, null, false);
    FlutterEngineCache.getInstance().put("my_cached_engine_68269", flutterEngine);

    FragmentActivity activity = Robolectric.buildActivity(FragmentActivity.class).setup().get();

    // 1. Create Fragment 1 (representing Route 1 / previous route in add2app).
    FlutterFragment fragment1 =
        spy(
            FlutterFragment.withCachedEngine("my_cached_engine_68269")
                .renderMode(RenderMode.surface)
                .destroyEngineWithFragment(false)
                .build());
    when(fragment1.getContext()).thenReturn(spyCtx);
    when(fragment1.getActivity()).thenReturn(activity);
    FlutterActivityAndFragmentDelegate delegate1 =
        new FlutterActivityAndFragmentDelegate(fragment1);
    fragment1.setDelegateFactory(new TestDelegateFactory(delegate1));

    fragment1.onAttach(spyCtx);
    FlutterView flutterView1 = (FlutterView) delegate1.onCreateView(null, null, null, 0, false);
    FlutterSurfaceView surfaceView1 = (FlutterSurfaceView) flutterView1.renderSurface;
    assertEquals(View.VISIBLE, surfaceView1.getVisibility());

    // 2. Create Fragment 2 (representing Route 2 with WebView / Hybrid Composition in add2app).
    FlutterFragment fragment2 =
        spy(
            FlutterFragment.withCachedEngine("my_cached_engine_68269")
                .renderMode(RenderMode.surface)
                .destroyEngineWithFragment(false)
                .build());
    when(fragment2.getContext()).thenReturn(spyCtx);
    when(fragment2.getActivity()).thenReturn(activity);
    FlutterActivityAndFragmentDelegate delegate2 =
        new FlutterActivityAndFragmentDelegate(fragment2);
    fragment2.setDelegateFactory(new TestDelegateFactory(delegate2));

    // When Fragment 2 attaches to the shared engine in add2app, ExclusiveAppComponent enforcement
    // detaches Fragment 1 from the engine.
    fragment1.detachFromFlutterEngine();

    fragment2.onAttach(spyCtx);
    FlutterView flutterView2 = (FlutterView) delegate2.onCreateView(null, null, null, 0, false);

    // When SurfaceAndroidWebView (Hybrid Composition) is displayed in Route 2,
    // PlatformViewsController converts Fragment 2's FlutterView to use FlutterImageView.
    flutterView2.convertToImageView();

    // Verify that when Fragment 1 is detached from the engine (evicted by Fragment 2),
    // its underlying FlutterSurfaceView (which has setZOrderOnTop(true)) does NOT remain visible
    // covering Fragment 2's WebView.
    // This assertion FAILS (actual: VISIBLE) reproducing the bug in flutter/flutter#68269.
    assertEquals(View.GONE, surfaceView1.getVisibility());
  }
}
