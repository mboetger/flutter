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
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.annotation.TargetApi;
import android.os.Bundle;
import android.content.Context;
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
import org.robolectric.annotation.Config;

@RunWith(AndroidJUnit4.class)
public class FlutterFragmentTest {
  private final Context ctx = ApplicationProvider.getApplicationContext();
  boolean isDelegateAttached;



  @Test
  public void itCreatesDefaultFragmentWithExpectedDefaults() {
    FlutterFragment fragment = FlutterFragment.createDefault();

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
    FlutterFragment fragment =
        FlutterFragment.withCachedEngine("my_cached_engine")
            .destroyEngineWithFragment(true)
            .build();

    isDelegateAttached = true;
    when(mockDelegate.isAttached()).thenAnswer(invocation -> isDelegateAttached);
    doAnswer(invocation -> isDelegateAttached = false).when(mockDelegate).onDetach();

    fragment.delegate = mockDelegate;
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

    FlutterFragment fragment =
        FlutterFragment.withCachedEngine("my_cached_engine")
            .destroyEngineWithFragment(true)
            .build();

    fragment.delegate = mockDelegate;
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

    FlutterFragment fragment =
        spy(
            FlutterFragment.withCachedEngine("my_cached_engine")
                .destroyEngineWithFragment(true)
                .build());
    when(fragment.getContext()).thenReturn(mock(Context.class));

    fragment.delegate = mockDelegate;
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
    fragment.delegate = delegate;

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
            fragment.delegate = mockDelegate;

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
            fragment.delegate = mockDelegate;

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
            fragment.delegate = mockDelegate;

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

    Context spyCtx = spy(ctx);
    // We need to mock FlutterJNI to avoid triggering native code.
    FlutterJNI flutterJNI = mock(FlutterJNI.class);
    when(flutterJNI.isAttached()).thenReturn(true);

    FlutterEngine flutterEngine =
        new FlutterEngine(spyCtx, new FlutterLoader(), flutterJNI, null, false);
    FlutterEngineCache.getInstance().put("my_cached_engine", flutterEngine);

    TestFlutterFragment fragment = new TestFlutterFragment(mockDelegate, spyCtx);
    Bundle args = new Bundle();
    args.putString(FlutterFragment.ARG_CACHED_ENGINE_ID, "my_cached_engine");
    fragment.setArguments(args);

    fragment.onAttach(spyCtx);
    verify(spyCtx, times(1)).registerComponentCallbacks(any());
    verify(spyCtx, never()).unregisterComponentCallbacks(any());

    fragment.onDetach();
    verify(spyCtx, times(1)).registerComponentCallbacks(any());
    verify(spyCtx, times(1)).unregisterComponentCallbacks(any());
  }

  private static class TestFlutterFragment extends FlutterFragment {
    private final FlutterActivityAndFragmentDelegate delegateToReturn;
    private final Context contextToReturn;

    TestFlutterFragment(
        FlutterActivityAndFragmentDelegate delegateToReturn, Context contextToReturn) {
      this.delegateToReturn = delegateToReturn;
      this.contextToReturn = contextToReturn;
    }

    @Override
    public FlutterActivityAndFragmentDelegate createDelegate(
        FlutterActivityAndFragmentDelegate.Host host) {
      return delegateToReturn;
    }

    @Override
    public Context getContext() {
      return contextToReturn != null ? contextToReturn : super.getContext();
    }
  }
}
