// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine.systemchannels;

import static io.flutter.Build.API_LEVELS;
import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.annotation.TargetApi;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.dart.DartExecutor;
import io.flutter.plugin.common.MethodCall;
import io.flutter.plugin.common.MethodChannel;
import java.util.Locale;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.annotation.Config;

@Config(shadows = {})
@RunWith(AndroidJUnit4.class)
@TargetApi(API_LEVELS.API_24)
public class TextInputChannelTest {
  @Test
  public void setEditableSizeAndTransformCompletes() throws JSONException {
    TextInputChannel textInputChannel = new TextInputChannel(mock(DartExecutor.class));
    textInputChannel.setTextInputMethodHandler(mock(TextInputChannel.TextInputMethodHandler.class));
    JSONObject arguments = new JSONObject();
    arguments.put("width", 100.0);
    arguments.put("height", 20.0);
    arguments.put("transform", new JSONArray(new double[16]));
    MethodCall call = new MethodCall("TextInput.setEditableSizeAndTransform", arguments);
    MethodChannel.Result result = mock(MethodChannel.Result.class);
    textInputChannel.parsingMethodHandler.onMethodCall(call, result);
    verify(result).success(null);
  }

  @Test
  @TargetApi(API_LEVELS.API_24)
  @Config(sdk = API_LEVELS.API_24)
  public void configurationFromJsonParsesHintLocales() throws JSONException, NoSuchFieldException {
    JSONObject arguments = new JSONObject();

    // Mandatory parameters.
    arguments.put("inputAction", "TextInputAction.done");
    arguments.put("textCapitalization", "TextCapitalization.none");
    JSONObject inputType = new JSONObject();
    inputType.put("name", "TextInputType.text");
    arguments.put("inputType", inputType);

    arguments.put("hintLocales", new JSONArray(new String[] {"en", "fr"}));
    final TextInputChannel.Configuration configuration =
        TextInputChannel.Configuration.fromJson(arguments);

    final Locale[] hintLocales = {
      new Locale.Builder().setLanguage("en").build(), new Locale.Builder().setLanguage("fr").build()
    };
    assertEquals(configuration.hintLocales.length, hintLocales.length);
    assertEquals(configuration.hintLocales[0], hintLocales[0]);
    assertEquals(configuration.hintLocales[1], hintLocales[1]);
  }

  @Test
  public void setEditingState_clampsSelectionStartOutOfBounds() throws JSONException {
    TextInputChannel textInputChannel = new TextInputChannel(mock(DartExecutor.class));
    TextInputChannel.TextInputMethodHandler mockHandler = mock(TextInputChannel.TextInputMethodHandler.class);
    textInputChannel.setTextInputMethodHandler(mockHandler);
    MethodChannel.Result mockResult = mock(MethodChannel.Result.class);

    JSONObject editingState = new JSONObject();
    editingState.put("text", "hello"); // length is 5
    editingState.put("selectionBase", 6); // selection start out of bounds (6 > 5)
    editingState.put("selectionExtent", 5);
    editingState.put("composingBase", -1);
    editingState.put("composingExtent", -1);

    MethodCall call = new MethodCall("TextInput.setEditingState", editingState);

    // Under buggy code, this will throw IndexOutOfBoundsException and fail the test.
    // Under fixed code, this will succeed and clamp the selection start.
    textInputChannel.parsingMethodHandler.onMethodCall(call, mockResult);

    // Verify that the platform channel call returned successfully.
    verify(mockResult).success(null);

    // Verify that the handler was invoked with the safely clamped editing state.
    ArgumentCaptor<TextInputChannel.TextEditState> stateCaptor =
        ArgumentCaptor.forClass(TextInputChannel.TextEditState.class);
    verify(mockHandler).setEditingState(stateCaptor.capture());

    TextInputChannel.TextEditState capturedState = stateCaptor.getValue();
    assertEquals("hello", capturedState.text);
    assertEquals(5, capturedState.selectionStart); // Clamped from 6 to 5
    assertEquals(5, capturedState.selectionEnd);
  }

  @Test
  public void setEditingState_clampsSelectionEndOutOfBounds() throws JSONException {
    TextInputChannel textInputChannel = new TextInputChannel(mock(DartExecutor.class));
    TextInputChannel.TextInputMethodHandler mockHandler = mock(TextInputChannel.TextInputMethodHandler.class);
    textInputChannel.setTextInputMethodHandler(mockHandler);
    MethodChannel.Result mockResult = mock(MethodChannel.Result.class);

    JSONObject editingState = new JSONObject();
    editingState.put("text", "hello"); // length is 5
    editingState.put("selectionBase", 5);
    editingState.put("selectionExtent", 6); // selection end out of bounds (6 > 5)
    editingState.put("composingBase", -1);
    editingState.put("composingExtent", -1);

    MethodCall call = new MethodCall("TextInput.setEditingState", editingState);

    textInputChannel.parsingMethodHandler.onMethodCall(call, mockResult);

    verify(mockResult).success(null);

    ArgumentCaptor<TextInputChannel.TextEditState> stateCaptor =
        ArgumentCaptor.forClass(TextInputChannel.TextEditState.class);
    verify(mockHandler).setEditingState(stateCaptor.capture());

    TextInputChannel.TextEditState capturedState = stateCaptor.getValue();
    assertEquals("hello", capturedState.text);
    assertEquals(5, capturedState.selectionStart);
    assertEquals(5, capturedState.selectionEnd); // Clamped from 6 to 5
  }

  @Test
  public void setEditingState_clampsComposingEndOutOfBounds() throws JSONException {
    TextInputChannel textInputChannel = new TextInputChannel(mock(DartExecutor.class));
    TextInputChannel.TextInputMethodHandler mockHandler = mock(TextInputChannel.TextInputMethodHandler.class);
    textInputChannel.setTextInputMethodHandler(mockHandler);
    MethodChannel.Result mockResult = mock(MethodChannel.Result.class);

    JSONObject editingState = new JSONObject();
    editingState.put("text", "hello"); // length is 5
    editingState.put("selectionBase", 5);
    editingState.put("selectionExtent", 5);
    editingState.put("composingBase", 0);
    editingState.put("composingExtent", 6); // composing end out of bounds (6 > 5)

    MethodCall call = new MethodCall("TextInput.setEditingState", editingState);

    textInputChannel.parsingMethodHandler.onMethodCall(call, mockResult);

    verify(mockResult).success(null);

    ArgumentCaptor<TextInputChannel.TextEditState> stateCaptor =
        ArgumentCaptor.forClass(TextInputChannel.TextEditState.class);
    verify(mockHandler).setEditingState(stateCaptor.capture());

    TextInputChannel.TextEditState capturedState = stateCaptor.getValue();
    assertEquals("hello", capturedState.text);
    assertEquals(0, capturedState.composingStart);
    assertEquals(5, capturedState.composingEnd); // Clamped from 6 to 5
  }

  @Test(expected = IndexOutOfBoundsException.class)
  public void setEditingState_throwsOnReversedComposingRangeOutOfBounds() throws JSONException {
    JSONObject editingState = new JSONObject();
    editingState.put("text", "hello"); // length is 5
    editingState.put("selectionBase", 5);
    editingState.put("selectionExtent", 5);
    editingState.put("composingBase", 6); // composing start out of bounds (6 > 5)
    editingState.put("composingExtent", 5); // composing end is 5

    TextInputChannel.TextEditState.fromJson(editingState);
  }

  @Test
  public void setEditingState_clampsComposingStartAndEndOutOfBounds() throws JSONException {
    TextInputChannel textInputChannel = new TextInputChannel(mock(DartExecutor.class));
    TextInputChannel.TextInputMethodHandler mockHandler = mock(TextInputChannel.TextInputMethodHandler.class);
    textInputChannel.setTextInputMethodHandler(mockHandler);
    MethodChannel.Result mockResult = mock(MethodChannel.Result.class);

    JSONObject editingState = new JSONObject();
    editingState.put("text", "hello"); // length is 5
    editingState.put("selectionBase", 5);
    editingState.put("selectionExtent", 5);
    editingState.put("composingBase", 6); // composing start out of bounds (6 > 5)
    editingState.put("composingExtent", 7); // composing end out of bounds (7 > 5)

    MethodCall call = new MethodCall("TextInput.setEditingState", editingState);

    textInputChannel.parsingMethodHandler.onMethodCall(call, mockResult);

    verify(mockResult).success(null);

    ArgumentCaptor<TextInputChannel.TextEditState> stateCaptor =
        ArgumentCaptor.forClass(TextInputChannel.TextEditState.class);
    verify(mockHandler).setEditingState(stateCaptor.capture());

    TextInputChannel.TextEditState capturedState = stateCaptor.getValue();
    assertEquals("hello", capturedState.text);
    assertEquals(5, capturedState.composingStart); // Clamped from 6 to 5
    assertEquals(5, capturedState.composingEnd); // Clamped from 7 to 5
  }

  @Test
  public void setEditingState_clampsSelectionStartAndEndOutOfBounds() throws JSONException {
    TextInputChannel textInputChannel = new TextInputChannel(mock(DartExecutor.class));
    TextInputChannel.TextInputMethodHandler mockHandler = mock(TextInputChannel.TextInputMethodHandler.class);
    textInputChannel.setTextInputMethodHandler(mockHandler);
    MethodChannel.Result mockResult = mock(MethodChannel.Result.class);

    JSONObject editingState = new JSONObject();
    editingState.put("text", "hello"); // length is 5
    editingState.put("selectionBase", 6); // selection start out of bounds (6 > 5)
    editingState.put("selectionExtent", 7); // selection end out of bounds (7 > 5)
    editingState.put("composingBase", -1);
    editingState.put("composingExtent", -1);

    MethodCall call = new MethodCall("TextInput.setEditingState", editingState);

    textInputChannel.parsingMethodHandler.onMethodCall(call, mockResult);

    verify(mockResult).success(null);

    ArgumentCaptor<TextInputChannel.TextEditState> stateCaptor =
        ArgumentCaptor.forClass(TextInputChannel.TextEditState.class);
    verify(mockHandler).setEditingState(stateCaptor.capture());

    TextInputChannel.TextEditState capturedState = stateCaptor.getValue();
    assertEquals("hello", capturedState.text);
    assertEquals(5, capturedState.selectionStart); // Clamped from 6 to 5
    assertEquals(5, capturedState.selectionEnd); // Clamped from 7 to 5
  }
}

