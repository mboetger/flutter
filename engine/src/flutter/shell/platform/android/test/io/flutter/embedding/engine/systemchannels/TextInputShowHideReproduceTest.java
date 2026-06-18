// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine.systemchannels;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.view.View;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.plugin.common.MethodCall;
import io.flutter.plugin.common.MethodChannel;
import io.flutter.plugin.editing.TextInputPlugin;
import java.lang.reflect.Method;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;

@RunWith(AndroidJUnit4.class)
public class TextInputShowHideReproduceTest {
  @Test
  public void testTextInputPluginShowTextInputMethodReturnTypeIsBoolean() throws NoSuchMethodException {
    Method showMethod = TextInputPlugin.class.getDeclaredMethod("showTextInput", View.class);
    assertEquals(boolean.class, showMethod.getReturnType());
  }

  @Test
  public void testTextInputPluginHideTextInputMethodReturnTypeIsBoolean() throws NoSuchMethodException {
    // hideTextInput is private in TextInputPlugin.
    Method hideMethod = TextInputPlugin.class.getDeclaredMethod("hideTextInput", View.class);
    assertEquals(boolean.class, hideMethod.getReturnType());
  }

  @Test
  public void testTextInputMethodHandlerShowMethodReturnTypeIsBoolean() throws NoSuchMethodException {
    Method showMethod = TextInputChannel.TextInputMethodHandler.class.getDeclaredMethod("show");
    assertEquals(boolean.class, showMethod.getReturnType());
  }

  @Test
  public void testTextInputMethodHandlerHideMethodReturnTypeIsBoolean() throws NoSuchMethodException {
    Method hideMethod = TextInputChannel.TextInputMethodHandler.class.getDeclaredMethod("hide");
    assertEquals(boolean.class, hideMethod.getReturnType());
  }

  @Test
  public void testTextInputChannelShowReturnsBoolean() {
    TextInputChannel textInputChannel = new TextInputChannel(mock(io.flutter.embedding.engine.dart.DartExecutor.class));
    TextInputChannel.TextInputMethodHandler mockHandler = mock(TextInputChannel.TextInputMethodHandler.class);
    textInputChannel.setTextInputMethodHandler(mockHandler);

    MethodCall call = new MethodCall("TextInput.show", null);
    MethodChannel.Result mockResult = mock(MethodChannel.Result.class);
    textInputChannel.parsingMethodHandler.onMethodCall(call, mockResult);

    ArgumentCaptor<Object> captor = ArgumentCaptor.forClass(Object.class);
    verify(mockResult).success(captor.capture());
    assertNotEquals(null, captor.getValue());
    assertEquals(Boolean.class, captor.getValue().getClass());
  }

  @Test
  public void testTextInputChannelHideReturnsBoolean() {
    TextInputChannel textInputChannel = new TextInputChannel(mock(io.flutter.embedding.engine.dart.DartExecutor.class));
    TextInputChannel.TextInputMethodHandler mockHandler = mock(TextInputChannel.TextInputMethodHandler.class);
    textInputChannel.setTextInputMethodHandler(mockHandler);

    MethodCall call = new MethodCall("TextInput.hide", null);
    MethodChannel.Result mockResult = mock(MethodChannel.Result.class);
    textInputChannel.parsingMethodHandler.onMethodCall(call, mockResult);

    ArgumentCaptor<Object> captor = ArgumentCaptor.forClass(Object.class);
    verify(mockResult).success(captor.capture());
    assertNotEquals(null, captor.getValue());
    assertEquals(Boolean.class, captor.getValue().getClass());
  }
}
