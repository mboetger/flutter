// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import android.app.job.JobParameters;
import android.app.job.JobService;
import android.os.PersistableBundle;
import androidx.annotation.NonNull;
import io.flutter.FlutterInjector;
import io.flutter.Log;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.embedding.engine.dart.DartExecutor;
import io.flutter.embedding.engine.loader.FlutterLoader;
import io.flutter.plugin.common.MethodCall;
import io.flutter.plugin.common.MethodChannel;
import java.util.Arrays;
import java.util.List;

/** An Android {@link JobService} (API 21+) that executes Dart code in a {@link FlutterEngine}. */
public class FlutterJobService extends JobService {
  private static final String TAG = "FlutterJobService";
  private static final String CHANNEL_NAME = "io.flutter/background_service";

  public static final String EXTRA_DART_ENTRYPOINT = "dart_entrypoint";
  public static final String EXTRA_DART_ENTRYPOINT_ARGS = "dart_entrypoint_args";

  protected FlutterEngine flutterEngine;
  private MethodChannel methodChannel;
  private JobParameters activeParams;

  @Override
  public boolean onStartJob(@NonNull final JobParameters params) {
    if (activeParams != null) {
      Log.w(
          TAG,
          "FlutterJobService is already running a job. Rejecting job ID: " + params.getJobId());
      return false;
    }

    if (flutterEngine != null) {
      Log.w(TAG, "FlutterEngine already exists from a previous job. Destroying it to start fresh.");
      cleanup();
    }

    activeParams = params;

    flutterEngine = provideFlutterEngine();

    methodChannel =
        new MethodChannel(flutterEngine.getDartExecutor().getBinaryMessenger(), CHANNEL_NAME);
    methodChannel.setMethodCallHandler(
        new MethodChannel.MethodCallHandler() {
          @Override
          public void onMethodCall(@NonNull MethodCall call, @NonNull MethodChannel.Result result) {
            if (call.method.equals("jobFinished")) {
              Boolean needsReschedule = call.argument("needsReschedule");
              if (needsReschedule == null) {
                needsReschedule = false;
              }
              if (activeParams != null) {
                jobFinished(activeParams, needsReschedule);
                activeParams = null;
              }
              result.success(null);
            } else {
              result.notImplemented();
            }
          }
        });

    PersistableBundle extras = params.getExtras();
    String entrypointName = extras != null ? extras.getString(EXTRA_DART_ENTRYPOINT) : null;
    if (entrypointName == null) {
      entrypointName = "main";
    }

    String[] entrypointArgsArray =
        extras != null ? extras.getStringArray(EXTRA_DART_ENTRYPOINT_ARGS) : null;
    List<String> entrypointArgs = null;
    if (entrypointArgsArray != null) {
      entrypointArgs = Arrays.asList(entrypointArgsArray);
    }

    FlutterLoader flutterLoader = FlutterInjector.instance().flutterLoader();
    String pathToBundle = flutterLoader.findAppBundlePath();
    DartExecutor.DartEntrypoint dartEntrypoint =
        new DartExecutor.DartEntrypoint(pathToBundle, entrypointName);
    flutterEngine.getDartExecutor().executeDartEntrypoint(dartEntrypoint, entrypointArgs);

    return true;
  }

  @Override
  public boolean onStopJob(@NonNull JobParameters params) {
    cleanup();
    return false;
  }

  /**
   * Provides the {@link FlutterEngine} to use.
   *
   * <p>Subclasses can override this to supply a custom or pre-existing {@code FlutterEngine}.
   */
  @NonNull
  protected FlutterEngine provideFlutterEngine() {
    return new FlutterEngine(this);
  }

  @Override
  public void onDestroy() {
    cleanup();
    super.onDestroy();
  }

  private void cleanup() {
    if (methodChannel != null) {
      methodChannel.setMethodCallHandler(null);
      methodChannel = null;
    }
    if (flutterEngine != null) {
      flutterEngine.destroy();
      flutterEngine = null;
    }
    activeParams = null;
  }
}
