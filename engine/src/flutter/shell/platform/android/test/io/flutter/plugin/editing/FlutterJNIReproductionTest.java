// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.plugin.editing;

import static org.junit.Assert.assertSame;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.text.InputType;
import android.text.Selection;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.FlutterInjector;
import io.flutter.embedding.android.KeyboardManager;
import io.flutter.embedding.engine.FlutterJNI;
import io.flutter.embedding.engine.systemchannels.ScribeChannel;
import io.flutter.embedding.engine.systemchannels.TextInputChannel;
import io.flutter.util.FakeKeyEvent;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@RunWith(AndroidJUnit4.class)
public class FlutterJNIReproductionTest {
  @Mock FlutterJNI mockFlutterJNI;
  @Mock TextInputChannel mockTextInputChannel;
  @Mock ScribeChannel mockScribeChannel;
  @Mock KeyboardManager mockKeyboardManager;

  @Before
  public void setUp() {
    FlutterInjector.reset();
    MockitoAnnotations.openMocks(this);
  }

  @After
  public void tearDown() {
    FlutterInjector.reset();
  }

  @Test
  public void testFlutterJNIFactoryReturnsSingletonOrExistingInstanceByDefault() {
    // Issue #66131 / #92479: FlutterJNI should be a singleton or use existing instance by default.
    // Currently, FlutterJNI.Factory.provideFlutterJNI() creates a new FlutterJNI on every call.
    FlutterJNI.Factory factory = new FlutterJNI.Factory();
    FlutterJNI instance1 = factory.provideFlutterJNI();
    FlutterJNI instance2 = factory.provideFlutterJNI();

    // Fails under current behavior because two different FlutterJNI instances are created.
    assertSame(
        "FlutterJNI.Factory should provide a singleton or existing instance by default",
        instance1,
        instance2);
  }

  @Test
  public void testInputConnectionAdaptorUsesInjectedFlutterJNI() {
    // Issue #66131: InputConnectionAdaptor creates new FlutterJNI instances instead of using an
    // injected or existing singleton FlutterJNI instance from FlutterInjector.
    FlutterJNI.Factory mockFactory =
        new FlutterJNI.Factory() {
          @Override
          public FlutterJNI provideFlutterJNI() {
            return mockFlutterJNI;
          }
        };
    FlutterInjector.setInstance(
        new FlutterInjector.Builder().setFlutterJNIFactory(mockFactory).build());

    Context context = ApplicationProvider.getApplicationContext();
    View testView = new View(context);
    ListenableEditingState editable = new ListenableEditingState(null, testView);
    // Use multi-character string so getOffsetAfter checks code points and calls isCodePointEmoji.
    editable.append("abc");
    Selection.setSelection(editable, 0, 0);
    EditorInfo outAttrs = new EditorInfo();
    outAttrs.inputType = InputType.TYPE_CLASS_TEXT;

    when(mockFlutterJNI.isCodePointRegionalIndicator(anyInt())).thenReturn(false);
    when(mockFlutterJNI.isCodePointEmoji(anyInt())).thenReturn(false);
    when(mockFlutterJNI.isCodePointEmojiModifier(anyInt())).thenReturn(false);
    when(mockFlutterJNI.isCodePointEmojiModifierBase(anyInt())).thenReturn(false);
    when(mockFlutterJNI.isCodePointVariantSelector(anyInt())).thenReturn(false);

    // Call the 7-argument constructor used by TextInputPlugin in production.
    InputConnectionAdaptor adaptor =
        new InputConnectionAdaptor(
            testView,
            0,
            mockTextInputChannel,
            mockScribeChannel,
            mockKeyboardManager,
            editable,
            outAttrs);

    // Trigger horizontal movement which calls flutterTextUtils.getOffsetAfter.
    FakeKeyEvent keyEvent = new FakeKeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_DPAD_RIGHT);
    adaptor.handleKeyEvent(keyEvent);

    // Fails because InputConnectionAdaptor created its own new FlutterJNI()
    // rather than obtaining the injected instance from FlutterInjector.
    verify(mockFlutterJNI, atLeastOnce()).isCodePointRegionalIndicator(anyInt());
  }
}
