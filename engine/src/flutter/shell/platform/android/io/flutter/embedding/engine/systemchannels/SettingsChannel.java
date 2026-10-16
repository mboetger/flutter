// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine.systemchannels;

import static io.flutter.Build.API_LEVELS;

import android.annotation.SuppressLint;
import android.os.Build;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import io.flutter.Log;
import io.flutter.embedding.engine.dart.DartExecutor;
import io.flutter.plugin.common.BasicMessageChannel;
import io.flutter.plugin.common.JSONMessageCodec;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class SettingsChannel {
  private static final String TAG = "SettingsChannel";

  public static final String CHANNEL_NAME = "flutter/settings";
  private static final String TEXT_SCALE_FACTOR = "textScaleFactor";
  private static final String NATIVE_SPELL_CHECK_SERVICE_DEFINED = "nativeSpellCheckServiceDefined";
  private static final String BRIEFLY_SHOW_PASSWORD = "brieflyShowPassword";
  private static final String ALWAYS_USE_24_HOUR_FORMAT = "alwaysUse24HourFormat";
  private static final String PLATFORM_BRIGHTNESS = "platformBrightness";
  private static final String FONT_SCALE_CURVES = "fontScaleCurves";
  private static final String FONT_SCALE_CURVE_UNSCALED = "unscaled";
  private static final String FONT_SCALE_CURVE_SCALED = "scaled";

  // Standard unscaled font sizes (in sp) used by Android 14+ FontScaleConverter lookup tables:
  // 8sp, 10sp, 12sp, 14sp, 18sp, 20sp, 24sp, 30sp, and 100sp.
  private static final float[] FONT_SCALE_SAMPLE_SIZES_SP =
      new float[] {8f, 10f, 12f, 14f, 18f, 20f, 24f, 30f, 100f};

  // Number of keys ("unscaled" and "scaled") in each serialized curve point map.
  private static final int FONT_SCALE_CURVE_POINT_ENTRY_COUNT = 2;

  @NonNull public final BasicMessageChannel<Object> channel;

  public SettingsChannel(@NonNull DartExecutor dartExecutor) {
    this.channel = new BasicMessageChannel<>(dartExecutor, CHANNEL_NAME, JSONMessageCodec.INSTANCE);
  }

  @SuppressLint("AnnotateVersionCheck")
  public static boolean hasNonlinearTextScalingSupport() {
    return Build.VERSION.SDK_INT >= API_LEVELS.API_34;
  }

  @NonNull
  public MessageBuilder startMessage() {
    return new MessageBuilder();
  }

  public class MessageBuilder {
    @NonNull private Map<String, Object> message = new HashMap<>();
    @Nullable private DisplayMetrics displayMetrics;

    MessageBuilder() {}

    @NonNull
    public MessageBuilder setDisplayMetrics(@NonNull DisplayMetrics displayMetrics) {
      this.displayMetrics = displayMetrics;
      return this;
    }

    @NonNull
    public MessageBuilder setTextScaleFactor(float textScaleFactor) {
      message.put(TEXT_SCALE_FACTOR, textScaleFactor);
      return this;
    }

    @NonNull
    public MessageBuilder setNativeSpellCheckServiceDefined(
        boolean nativeSpellCheckServiceDefined) {
      message.put(NATIVE_SPELL_CHECK_SERVICE_DEFINED, nativeSpellCheckServiceDefined);
      return this;
    }

    @NonNull
    public MessageBuilder setBrieflyShowPassword(@NonNull boolean brieflyShowPassword) {
      message.put(BRIEFLY_SHOW_PASSWORD, brieflyShowPassword);
      return this;
    }

    @NonNull
    public MessageBuilder setUse24HourFormat(boolean use24HourFormat) {
      message.put(ALWAYS_USE_24_HOUR_FORMAT, use24HourFormat);
      return this;
    }

    @NonNull
    public MessageBuilder setPlatformBrightness(@NonNull PlatformBrightness brightness) {
      message.put(PLATFORM_BRIGHTNESS, brightness.name);
      return this;
    }

    public void send() {
      Log.v(
          TAG,
          "Sending message: \n"
              + "textScaleFactor: "
              + message.get(TEXT_SCALE_FACTOR)
              + "\n"
              + "alwaysUse24HourFormat: "
              + message.get(ALWAYS_USE_24_HOUR_FORMAT)
              + "\n"
              + "platformBrightness: "
              + message.get(PLATFORM_BRIGHTNESS));
      final DisplayMetrics metrics = this.displayMetrics;
      if (hasNonlinearTextScalingSupport() && metrics != null && metrics.density > 0.0f) {
        final Object rawScaleFactor = message.get(TEXT_SCALE_FACTOR);
        final float textScaleFactor =
            rawScaleFactor instanceof Number ? ((Number) rawScaleFactor).floatValue() : 1.0f;
        if (textScaleFactor != 1.0f) {
          final List<Map<String, Object>> curves =
              new ArrayList<>(FONT_SCALE_SAMPLE_SIZES_SP.length);
          for (float sp : FONT_SCALE_SAMPLE_SIZES_SP) {
            final float scaled =
                TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, sp, metrics)
                    / metrics.density;
            final Map<String, Object> point = new HashMap<>(FONT_SCALE_CURVE_POINT_ENTRY_COUNT);
            point.put(FONT_SCALE_CURVE_UNSCALED, sp);
            point.put(FONT_SCALE_CURVE_SCALED, scaled);
            curves.add(point);
          }
          message.put(FONT_SCALE_CURVES, curves);
        }
      }
      channel.send(message);
    }
  }

  /**
   * The brightness mode of the host platform.
   *
   * <p>The {@code name} property is the serialized representation of each brightness mode when
   * communicated via message channel.
   */
  public enum PlatformBrightness {
    light("light"),
    dark("dark");

    @NonNull public String name;

    PlatformBrightness(@NonNull String name) {
      this.name = name;
    }
  }
}
