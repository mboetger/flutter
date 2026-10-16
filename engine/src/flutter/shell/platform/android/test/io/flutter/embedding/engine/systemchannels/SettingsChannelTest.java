// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine.systemchannels;

import static io.flutter.Build.API_LEVELS;
import static junit.framework.TestCase.assertEquals;
import static junit.framework.TestCase.assertFalse;
import static junit.framework.TestCase.assertNotNull;
import static junit.framework.TestCase.assertTrue;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.isNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.annotation.TargetApi;
import android.util.DisplayMetrics;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import io.flutter.embedding.engine.dart.DartExecutor;
import io.flutter.plugin.common.JSONMessageCodec;
import java.nio.ByteBuffer;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.annotation.Config;

@RunWith(AndroidJUnit4.class)
public class SettingsChannelTest {
  // Number of standard sample sizes (8, 10, 12, 14, 18, 20, 24, 30, 100 sp) in fontScaleCurves.
  private static final int EXPECTED_CURVE_SAMPLE_COUNT = 9;
  private static final float FIRST_SAMPLE_SIZE_SP = 8.0f;
  private static final float FLOAT_TOLERANCE = 0.001f;

  @Test
  @TargetApi(API_LEVELS.API_33)
  @Config(sdk = API_LEVELS.API_33)
  @SuppressWarnings("deprecation")
  // DartExecutor.send is deprecated.
  public void setDisplayMetricsDoesNothingOnAPILevel33() throws JSONException {
    final DartExecutor executor = mock(DartExecutor.class);
    executor.onAttachedToJNI();
    final SettingsChannel settingsChannel = new SettingsChannel(executor);

    final ArgumentCaptor<ByteBuffer> messageCaptor = ArgumentCaptor.forClass(ByteBuffer.class);

    DisplayMetrics metrics = new DisplayMetrics();
    metrics.density = 2.0f;
    metrics.scaledDensity = 3.0f;
    settingsChannel.startMessage().setTextScaleFactor(1.5f).setDisplayMetrics(metrics).send();

    verify(executor).send(eq("flutter/settings"), messageCaptor.capture(), isNull());
    ByteBuffer rawMessage = messageCaptor.getValue();
    rawMessage.rewind();
    JSONObject decoded = (JSONObject) JSONMessageCodec.INSTANCE.decodeMessage(rawMessage);
    assertNotNull(decoded);
    assertFalse(decoded.has("fontScaleCurves"));
    assertFalse(decoded.has("configurationId"));
  }

  @Test
  @TargetApi(API_LEVELS.API_34)
  @Config(minSdk = API_LEVELS.API_34)
  @SuppressWarnings("deprecation")
  public void sendsFontScaleCurvesOnAPILevel34WhenScaled() throws JSONException {
    final DartExecutor executor = mock(DartExecutor.class);
    executor.onAttachedToJNI();
    final SettingsChannel settingsChannel = new SettingsChannel(executor);

    final ArgumentCaptor<ByteBuffer> messageCaptor = ArgumentCaptor.forClass(ByteBuffer.class);

    DisplayMetrics metrics = new DisplayMetrics();
    metrics.density = 2.0f;
    metrics.scaledDensity = 3.0f;
    settingsChannel.startMessage().setTextScaleFactor(1.5f).setDisplayMetrics(metrics).send();

    verify(executor).send(eq("flutter/settings"), messageCaptor.capture(), isNull());
    ByteBuffer rawMessage = messageCaptor.getValue();
    rawMessage.rewind();
    JSONObject decoded = (JSONObject) JSONMessageCodec.INSTANCE.decodeMessage(rawMessage);
    assertNotNull(decoded);
    assertTrue(decoded.has("fontScaleCurves"));
    assertFalse(decoded.has("configurationId"));

    JSONArray curves = decoded.getJSONArray("fontScaleCurves");
    assertEquals(EXPECTED_CURVE_SAMPLE_COUNT, curves.length());
    JSONObject firstPoint = curves.getJSONObject(0);
    assertEquals(FIRST_SAMPLE_SIZE_SP, (float) firstPoint.getDouble("unscaled"), FLOAT_TOLERANCE);
    assertTrue(firstPoint.getDouble("scaled") > FIRST_SAMPLE_SIZE_SP);
  }

  @Test
  @TargetApi(API_LEVELS.API_34)
  @Config(minSdk = API_LEVELS.API_34)
  @SuppressWarnings("deprecation")
  public void omitsFontScaleCurvesWhenScaleFactorIsUnity() throws JSONException {
    final DartExecutor executor = mock(DartExecutor.class);
    executor.onAttachedToJNI();
    final SettingsChannel settingsChannel = new SettingsChannel(executor);

    final ArgumentCaptor<ByteBuffer> messageCaptor = ArgumentCaptor.forClass(ByteBuffer.class);

    DisplayMetrics metrics = new DisplayMetrics();
    metrics.density = 2.0f;
    metrics.scaledDensity = 2.0f;
    settingsChannel.startMessage().setTextScaleFactor(1.0f).setDisplayMetrics(metrics).send();

    verify(executor).send(eq("flutter/settings"), messageCaptor.capture(), isNull());
    ByteBuffer rawMessage = messageCaptor.getValue();
    rawMessage.rewind();
    JSONObject decoded = (JSONObject) JSONMessageCodec.INSTANCE.decodeMessage(rawMessage);
    assertNotNull(decoded);
    assertFalse(decoded.has("fontScaleCurves"));
  }

  @Test
  @TargetApi(API_LEVELS.API_34)
  @Config(minSdk = API_LEVELS.API_34)
  @SuppressWarnings("deprecation")
  public void twoDifferentSettingsChannelDoNotShareState() throws JSONException {
    final DartExecutor executor1 = mock(DartExecutor.class);
    final DartExecutor executor2 = mock(DartExecutor.class);
    final SettingsChannel settingsChannel1 = new SettingsChannel(executor1);
    final SettingsChannel settingsChannel2 = new SettingsChannel(executor2);

    DisplayMetrics metrics1 = new DisplayMetrics();
    metrics1.density = 1.0f;
    metrics1.scaledDensity = 1.0f;
    DisplayMetrics metrics2 = new DisplayMetrics();
    metrics2.density = 2.0f;
    metrics2.scaledDensity = 4.0f;

    settingsChannel1.startMessage().setTextScaleFactor(1.0f).setDisplayMetrics(metrics1).send();
    settingsChannel2.startMessage().setTextScaleFactor(2.0f).setDisplayMetrics(metrics2).send();

    final ArgumentCaptor<ByteBuffer> captor1 = ArgumentCaptor.forClass(ByteBuffer.class);
    final ArgumentCaptor<ByteBuffer> captor2 = ArgumentCaptor.forClass(ByteBuffer.class);
    verify(executor1).send(eq("flutter/settings"), captor1.capture(), isNull());
    verify(executor2).send(eq("flutter/settings"), captor2.capture(), isNull());

    ByteBuffer msg1 = captor1.getValue();
    msg1.rewind();
    JSONObject decoded1 = (JSONObject) JSONMessageCodec.INSTANCE.decodeMessage(msg1);
    assertFalse(decoded1.has("fontScaleCurves"));

    ByteBuffer msg2 = captor2.getValue();
    msg2.rewind();
    JSONObject decoded2 = (JSONObject) JSONMessageCodec.INSTANCE.decodeMessage(msg2);
    assertTrue(decoded2.has("fontScaleCurves"));
  }
}
