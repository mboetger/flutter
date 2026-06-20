// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine.systemchannels;

import static io.flutter.Build.API_LEVELS;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.annotation.TargetApi;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.dart.DartExecutor;
import io.flutter.plugin.common.MethodCall;
import io.flutter.plugin.common.MethodChannel;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

@Config(shadows = {})
@RunWith(AndroidJUnit4.class)
@TargetApi(API_LEVELS.API_24)
public class TextInputChannelReproduceTest {
  @Test
  public void setEditingStateWithInvalidSelectionDoesNotThrow() throws JSONException {
    TextInputChannel textInputChannel = new TextInputChannel(mock(DartExecutor.class));
    textInputChannel.setTextInputMethodHandler(mock(TextInputChannel.TextInputMethodHandler.class));

    JSONObject editingState = new JSONObject();
    editingState.put("text", "hello");
    editingState.put("selectionBase", 0);
    editingState.put("selectionExtent", 10); // Out of bounds!
    editingState.put("composingBase", -1);
    editingState.put("composingExtent", -1);

    MethodCall call = new MethodCall("TextInput.setEditingState", editingState);
    MethodChannel.Result result = mock(MethodChannel.Result.class);

    // This call should not throw an unhandled IndexOutOfBoundsException.
    textInputChannel.parsingMethodHandler.onMethodCall(call, result);

    // Verify that the error is properly reported back to the platform channel.
    verify(result).error(eq("error"), anyString(), isNull());
  }

  @Test
  public void setClientWithInvalidAutofillSelectionDoesNotThrow() throws JSONException {
    TextInputChannel textInputChannel = new TextInputChannel(mock(DartExecutor.class));
    textInputChannel.setTextInputMethodHandler(mock(TextInputChannel.TextInputMethodHandler.class));

    // Construct a configuration with invalid autofill editing state
    JSONObject editingState = new JSONObject();
    editingState.put("text", "hello");
    editingState.put("selectionBase", 0);
    editingState.put("selectionExtent", 10); // Out of bounds!
    editingState.put("composingBase", -1);
    editingState.put("composingExtent", -1);

    JSONObject autofill = new JSONObject();
    autofill.put("uniqueIdentifier", "unused");
    autofill.put("hints", new JSONArray("[\"username\"]"));
    autofill.put("editingValue", editingState);

    JSONObject configuration = new JSONObject();
    configuration.put("inputAction", "TextInputAction.done");
    configuration.put("textCapitalization", "TextCapitalization.none");
    configuration.put("inputType", new JSONObject("{\"name\":\"TextInputType.text\"}"));
    configuration.put("autofill", autofill);

    JSONArray args = new JSONArray();
    args.put(1); // Client ID
    args.put(configuration);

    MethodCall call = new MethodCall("TextInput.setClient", args);
    MethodChannel.Result result = mock(MethodChannel.Result.class);

    // Verify this does not throw IndexOutOfBoundsException
    textInputChannel.parsingMethodHandler.onMethodCall(call, result);

    // Verify error is reported
    verify(result).error(eq("error"), anyString(), isNull());
  }
}
