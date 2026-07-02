// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine.systemchannels;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.refEq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.res.AssetManager;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import static org.mockito.ArgumentMatchers.any;
import io.flutter.embedding.engine.FlutterJNI;
import io.flutter.embedding.engine.dart.DartExecutor;
import io.flutter.plugin.common.MethodCall;
import io.flutter.plugin.common.MethodChannel;
import android.graphics.Rect;
import java.util.ArrayList;
import java.util.List;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;

@RunWith(AndroidJUnit4.class)
public class PlatformChannelTest {
  @Test
  public void platformChannel_hasStringsMessage() {
    MethodChannel rawChannel = mock(MethodChannel.class);
    FlutterJNI mockFlutterJNI = mock(FlutterJNI.class);
    DartExecutor dartExecutor = new DartExecutor(mockFlutterJNI, mock(AssetManager.class));
    PlatformChannel fakePlatformChannel = new PlatformChannel(dartExecutor);
    PlatformChannel.PlatformMessageHandler mockMessageHandler =
        mock(PlatformChannel.PlatformMessageHandler.class);
    fakePlatformChannel.setPlatformMessageHandler(mockMessageHandler);
    Boolean returnValue = true;
    when(mockMessageHandler.clipboardHasStrings()).thenReturn(returnValue);
    MethodCall methodCall = new MethodCall("Clipboard.hasStrings", null);
    MethodChannel.Result mockResult = mock(MethodChannel.Result.class);
    fakePlatformChannel.parsingMethodCallHandler.onMethodCall(methodCall, mockResult);

    JSONObject expected = new JSONObject();
    try {
      expected.put("value", returnValue);
    } catch (JSONException e) {
    }
    verify(mockResult).success(refEq(expected));
  }

  @Test
  public void platformChannel_shareInvokeMessage() {
    MethodChannel rawChannel = mock(MethodChannel.class);
    FlutterJNI mockFlutterJNI = mock(FlutterJNI.class);
    DartExecutor dartExecutor = new DartExecutor(mockFlutterJNI, mock(AssetManager.class));
    PlatformChannel fakePlatformChannel = new PlatformChannel(dartExecutor);
    PlatformChannel.PlatformMessageHandler mockMessageHandler =
        mock(PlatformChannel.PlatformMessageHandler.class);
    fakePlatformChannel.setPlatformMessageHandler(mockMessageHandler);

    ArgumentCaptor<String> valueCapture = ArgumentCaptor.forClass(String.class);
    doNothing().when(mockMessageHandler).share(valueCapture.capture());

    final String expectedContent = "Flutter";
    MethodCall methodCall = new MethodCall("Share.invoke", expectedContent);
    MethodChannel.Result mockResult = mock(MethodChannel.Result.class);
    fakePlatformChannel.parsingMethodCallHandler.onMethodCall(methodCall, mockResult);

    assertEquals(valueCapture.getValue(), expectedContent);
    verify(mockResult).success(null);
  }

  @Test
  public void platformChannel_setSystemGestureExclusionRects() throws JSONException {
    FlutterJNI mockFlutterJNI = mock(FlutterJNI.class);
    DartExecutor dartExecutor = new DartExecutor(mockFlutterJNI, mock(AssetManager.class));
    PlatformChannel fakePlatformChannel = new PlatformChannel(dartExecutor);
    PlatformChannel.PlatformMessageHandler mockMessageHandler =
        mock(PlatformChannel.PlatformMessageHandler.class);
    fakePlatformChannel.setPlatformMessageHandler(mockMessageHandler);

    JSONArray rectsJson = new JSONArray();
    JSONObject rectJson = new JSONObject();
    rectJson.put("left", 10);
    rectJson.put("top", 20);
    rectJson.put("right", 30);
    rectJson.put("bottom", 40);
    rectsJson.put(rectJson);

    MethodCall methodCall = new MethodCall("SystemChrome.setSystemGestureExclusionRects", rectsJson);
    MethodChannel.Result mockResult = mock(MethodChannel.Result.class);
    fakePlatformChannel.parsingMethodCallHandler.onMethodCall(methodCall, mockResult);

    ArgumentCaptor<List<Rect>> rectsCaptor = ArgumentCaptor.forClass(List.class);
    verify(mockMessageHandler).setSystemGestureExclusionRects(rectsCaptor.capture());
    assertEquals(1, rectsCaptor.getValue().size());
    assertEquals(new Rect(10, 20, 30, 40), rectsCaptor.getValue().get(0));
    verify(mockResult).success(null);
  }

  @Test
  public void platformChannel_getSystemGestureExclusionRects() throws JSONException {
    FlutterJNI mockFlutterJNI = mock(FlutterJNI.class);
    DartExecutor dartExecutor = new DartExecutor(mockFlutterJNI, mock(AssetManager.class));
    PlatformChannel fakePlatformChannel = new PlatformChannel(dartExecutor);
    PlatformChannel.PlatformMessageHandler mockMessageHandler =
        mock(PlatformChannel.PlatformMessageHandler.class);
    fakePlatformChannel.setPlatformMessageHandler(mockMessageHandler);

    List<Rect> rects = new ArrayList<>();
    rects.add(new Rect(10, 20, 30, 40));
    when(mockMessageHandler.getSystemGestureExclusionRects()).thenReturn(rects);

    MethodCall methodCall = new MethodCall("SystemChrome.getSystemGestureExclusionRects", null);
    MethodChannel.Result mockResult = mock(MethodChannel.Result.class);
    fakePlatformChannel.parsingMethodCallHandler.onMethodCall(methodCall, mockResult);

    ArgumentCaptor<JSONArray> resultCaptor = ArgumentCaptor.forClass(JSONArray.class);
    verify(mockResult).success(resultCaptor.capture());

    JSONArray resultJson = resultCaptor.getValue();
    assertEquals(1, resultJson.length());
    JSONObject rectJson = resultJson.getJSONObject(0);
    assertEquals(10, rectJson.getInt("left"));
    assertEquals(20, rectJson.getInt("top"));
    assertEquals(30, rectJson.getInt("right"));
    assertEquals(40, rectJson.getInt("bottom"));
  }
}
