// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.Bundle;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.FrameLayout;
import androidx.fragment.app.FragmentActivity;
import androidx.test.core.app.ActivityScenario;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.util.ArrayList;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class FlutterFragmentXmlInflationTest {

  // A subclass that simulates how a developer would customize a fragment for XML inflation.
  public static class CustomFlutterFragment extends FlutterFragment {
    @Override
    public String getDartEntrypointFunctionName() {
      return "custom_entrypoint";
    }

    @Override
    public String getInitialRoute() {
      return "/custom_route";
    }
  }

  @Test
  public void testDefaultConstructorHasDefaultConfiguration() {
    // This simulates inflating io.flutter.embedding.android.FlutterFragment directly in XML.
    FlutterFragment fragment = new FlutterFragment();
    
    // It will have default configuration.
    assertEquals("main", fragment.getDartEntrypointFunctionName());
    assertNull(fragment.getInitialRoute());
  }

  @Test
  public void testSetArgumentsAfterAttachmentDoesNotThrowInModernAndroidX() {
    // In modern AndroidX, calling setArguments() after attachment but before state saving
    // does not throw. However, this is NOT a recommended way to configure a FlutterFragment
    // because configuration options read during onAttach/onCreateView (e.g., cachedEngineId,
    // renderMode, transparencyMode, shouldAttachEngineToActivity) will have already been
    // consumed and will ignore these new arguments. Only options read during onStart
    // (like dartEntrypoint and initialRoute) will be applied, leading to a "partial
    // configuration" trap.
    try (ActivityScenario<FragmentActivity> scenario = ActivityScenario.launch(FragmentActivity.class)) {
      scenario.onActivity(activity -> {
        FlutterFragment fragment = new FlutterFragment();
        
        // Mock the delegate to avoid JNI calls and engine creation during lifecycle events.
        FlutterActivityAndFragmentDelegate mockDelegate = mock(FlutterActivityAndFragmentDelegate.class);
        
        // Mock onCreateView to return a mock View with a mock ViewTreeObserver to avoid NullPointerException/IllegalStateException in onDestroyView.
        View mockView = mock(View.class);
        ViewTreeObserver mockVTO = mock(ViewTreeObserver.class);
        when(mockView.getViewTreeObserver()).thenReturn(mockVTO);
        
        // Mock LayoutParams to avoid NullPointerException during measurement in the layout pass.
        FrameLayout.LayoutParams mockLP = new FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT
        );
        when(mockView.getLayoutParams()).thenReturn(mockLP);
        
        when(mockDelegate.onCreateView(any(), any(), any(), anyInt(), anyBoolean())).thenReturn(mockView);
        
        fragment.setDelegateFactory(host -> mockDelegate);
        
        activity.getSupportFragmentManager()
            .beginTransaction()
            .add(android.R.id.content, fragment)
            .commitNow();
            
        // This does not throw in modern AndroidX Fragment.
        Bundle args = new Bundle();
        args.putString("dart_entrypoint", "custom_point");
        fragment.setArguments(args);
        
        assertEquals("custom_point", fragment.getDartEntrypointFunctionName());
      });
    }
  }

  @Test
  public void testSubclassAllowsConfigurationForXmlInflation() {
    // If they subclass FlutterFragment, they can configure it.
    CustomFlutterFragment fragment = new CustomFlutterFragment();
    
    assertEquals("custom_entrypoint", fragment.getDartEntrypointFunctionName());
    assertEquals("/custom_route", fragment.getInitialRoute());
  }

  @Test
  public void testXmlInflationWithNoNamespaceAttributes() {
    Context context = ApplicationProvider.getApplicationContext();
    AttributeSet attrs = mock(AttributeSet.class);

    // Mock AttributeSet to return literal values without namespace
    when(attrs.getAttributeValue(null, "flutter_dart_entrypoint")).thenReturn("xml_entrypoint");
    when(attrs.getAttributeValue(null, "flutter_initial_route")).thenReturn("/xml_route");
    when(attrs.getAttributeValue(null, "flutter_handle_deeplinking")).thenReturn("true");
    when(attrs.getAttributeValue(null, "flutter_render_mode")).thenReturn("texture");
    when(attrs.getAttributeValue(null, "flutter_transparency_mode")).thenReturn("opaque");
    when(attrs.getAttributeValue(null, "flutter_should_attach_engine_to_activity")).thenReturn("false");
    when(attrs.getAttributeValue(null, "flutter_should_delay_first_android_view_draw")).thenReturn("true");
    when(attrs.getAttributeValue(null, "flutter_should_automatically_handle_on_back_pressed")).thenReturn("true");
    when(attrs.getAttributeValue(null, "flutter_cached_engine_id")).thenReturn("my_cached_engine");
    when(attrs.getAttributeValue(null, "flutter_destroy_engine_with_fragment")).thenReturn("true");
    when(attrs.getAttributeValue(null, "flutter_enable_state_restoration")).thenReturn("true");
    when(attrs.getAttributeValue(null, "flutter_dart_entrypoint_args")).thenReturn("arg1, arg2, arg3");

    FlutterFragment fragment = new FlutterFragment();
    fragment.onInflate(context, attrs, null);

    Bundle args = fragment.getArguments();
    assertEquals("xml_entrypoint", args.getString("dart_entrypoint"));
    assertEquals("/xml_route", args.getString("initial_route"));
    assertEquals(true, args.getBoolean("handle_deeplinking"));
    assertEquals("texture", args.getString("flutterview_render_mode"));
    assertEquals("opaque", args.getString("flutterview_transparency_mode"));
    assertEquals(false, args.getBoolean("should_attach_engine_to_activity"));
    assertEquals(true, args.getBoolean("should_delay_first_android_view_draw"));
    assertEquals(true, args.getBoolean("should_automatically_handle_on_back_pressed"));
    assertEquals("my_cached_engine", args.getString("cached_engine_id"));
    assertEquals(true, args.getBoolean("destroy_engine_with_fragment"));
    assertEquals(true, args.getBoolean("enable_state_restoration"));
    
    ArrayList<String> entrypointArgs = args.getStringArrayList("dart_entrypoint_args");
    assertEquals(3, entrypointArgs.size());
    assertEquals("arg1", entrypointArgs.get(0));
    assertEquals("arg2", entrypointArgs.get(1));
    assertEquals("arg3", entrypointArgs.get(2));
  }

  @Test
  public void testXmlInflationWithNamespaceAttributes() {
    Context context = ApplicationProvider.getApplicationContext();
    AttributeSet attrs = mock(AttributeSet.class);

    // Mock AttributeSet to return literal values with namespace
    when(attrs.getAttributeValue("http://schemas.android.com/apk/res-auto", "flutter_dart_entrypoint")).thenReturn("xml_entrypoint_ns");
    when(attrs.getAttributeValue("http://schemas.android.com/apk/res-auto", "flutter_initial_route")).thenReturn("/xml_route_ns");

    FlutterFragment fragment = new FlutterFragment();
    fragment.onInflate(context, attrs, null);

    Bundle args = fragment.getArguments();
    assertEquals("xml_entrypoint_ns", args.getString("dart_entrypoint"));
    assertEquals("/xml_route_ns", args.getString("initial_route"));
  }

  @Test
  public void testXmlInflationWithResourceReferences() {
    Context mockContext = mock(Context.class);
    android.content.res.Resources mockResources = mock(android.content.res.Resources.class);
    when(mockContext.getResources()).thenReturn(mockResources);

    AttributeSet attrs = mock(AttributeSet.class);

    // Mock AttributeSet to return resource references
    when(attrs.getAttributeValue(null, "flutter_dart_entrypoint")).thenReturn("@string/entrypoint");
    when(attrs.getAttributeResourceValue(null, "flutter_dart_entrypoint", 0)).thenReturn(1001);
    when(mockResources.getString(1001)).thenReturn("resolved_entrypoint");

    when(attrs.getAttributeValue(null, "flutter_handle_deeplinking")).thenReturn("@bool/deeplink");
    when(attrs.getAttributeResourceValue(null, "flutter_handle_deeplinking", 0)).thenReturn(1002);
    when(mockResources.getBoolean(1002)).thenReturn(true);

    when(attrs.getAttributeValue(null, "flutter_dart_entrypoint_args")).thenReturn("@array/args");
    when(attrs.getAttributeResourceValue(null, "flutter_dart_entrypoint_args", 0)).thenReturn(1003);
    when(mockResources.getStringArray(1003)).thenReturn(new String[]{"res_arg1", "res_arg2"});

    FlutterFragment fragment = new FlutterFragment();
    fragment.onInflate(mockContext, attrs, null);

    Bundle args = fragment.getArguments();
    assertEquals("resolved_entrypoint", args.getString("dart_entrypoint"));
    assertEquals(true, args.getBoolean("handle_deeplinking"));
    
    ArrayList<String> entrypointArgs = args.getStringArrayList("dart_entrypoint_args");
    assertEquals(2, entrypointArgs.size());
    assertEquals("res_arg1", entrypointArgs.get(0));
    assertEquals("res_arg2", entrypointArgs.get(1));
  }

  @Test
  public void testXmlInflationPreservesExistingArguments() {
    Context context = ApplicationProvider.getApplicationContext();
    AttributeSet attrs = mock(AttributeSet.class);

    when(attrs.getAttributeValue(null, "flutter_dart_entrypoint")).thenReturn("xml_entrypoint");

    FlutterFragment fragment = new FlutterFragment();
    
    // Set some initial arguments
    Bundle initialArgs = new Bundle();
    initialArgs.putString("existing_key", "existing_value");
    initialArgs.putString("dart_entrypoint", "initial_entrypoint");
    fragment.setArguments(initialArgs);

    fragment.onInflate(context, attrs, null);

    Bundle args = fragment.getArguments();
    // Verify that the existing key is preserved
    assertEquals("existing_value", args.getString("existing_key"));
    // Verify that the XML attribute overwrites the initial value
    assertEquals("xml_entrypoint", args.getString("dart_entrypoint"));
  }

  @Test
  public void testXmlInflationBooleanDistinction() {
    Context context = ApplicationProvider.getApplicationContext();
    AttributeSet attrs = mock(AttributeSet.class);

    // Set one boolean to false, and leave another unset
    when(attrs.getAttributeValue(null, "flutter_handle_deeplinking")).thenReturn("false");
    // flutter_should_attach_engine_to_activity is omitted (returns null)

    FlutterFragment fragment = new FlutterFragment();
    fragment.onInflate(context, attrs, null);

    Bundle args = fragment.getArguments();
    // Verify that handle_deeplinking is explicitly set to false
    assertEquals(false, args.getBoolean("handle_deeplinking"));
    // Verify that should_attach_engine_to_activity is NOT set in the bundle (contains key is false)
    assertEquals(false, args.containsKey("should_attach_engine_to_activity"));
  }

  @Test
  public void testXmlInflationEmptyStringListAttribute() {
    Context context = ApplicationProvider.getApplicationContext();
    AttributeSet attrs = mock(AttributeSet.class);

    // Mock AttributeSet to return empty/whitespace values for the string list
    when(attrs.getAttributeValue(null, "flutter_dart_entrypoint_args")).thenReturn("  ");

    FlutterFragment fragment = new FlutterFragment();
    fragment.onInflate(context, attrs, null);

    Bundle args = fragment.getArguments();
    ArrayList<String> entrypointArgs = args.getStringArrayList("dart_entrypoint_args");
    assertEquals(0, entrypointArgs.size());
  }
}
