// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine;

import android.app.job.JobParameters;
import android.app.job.JobService;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import io.flutter.FlutterInjector;
import io.flutter.Log;
import io.flutter.embedding.engine.dart.DartExecutor;
import io.flutter.embedding.engine.loader.FlutterLoader;
import io.flutter.plugin.common.MethodCall;
import io.flutter.plugin.common.MethodChannel;
import io.flutter.view.FlutterCallbackInformation;

/**
 * A {@link JobService} that initializes Flutter and runs a registered Dart callback
 * in the background when the device boots.
 */
public class FlutterBootJobService extends JobService {
  private static final String TAG = "FlutterBootJobService";
  private static final String CHANNEL_NAME = "io.flutter/boot";
  private static final long SAFETY_TIMEOUT_MS = 60000; // 1 minute

  private final Object lifecycleLock = new Object();
  private boolean jobFinishedCalled = false;
  
  @VisibleForTesting
  @Nullable
  FlutterEngine flutterEngine;
  
  private Handler timeoutHandler;
  private Runnable timeoutRunnable;

  @Override
  public boolean onStartJob(@NonNull final JobParameters params) {
    Log.i(TAG, "Starting FlutterBootJobService");

    final Context context = getApplicationContext();
    final long callbackHandle = FlutterBootReceiver.getBootCallbackHandle(context);
    if (callbackHandle == 0) {
      Log.e(TAG, "No boot callback registered.");
      return false;
    }

    final FlutterCallbackInformation callbackInfo =
        FlutterCallbackInformation.lookupCallbackInformation(callbackHandle);
    if (callbackInfo == null) {
      Log.e(TAG, "Failed to find callback information for handle: " + callbackHandle);
      return false;
    }

    try {
      synchronized (lifecycleLock) {
        final FlutterLoader flutterLoader = FlutterInjector.instance().flutterLoader();
        flutterLoader.startInitialization(context);
        flutterLoader.ensureInitializationComplete(context, null);

        flutterEngine = new FlutterEngine(context);

        // Set up the method channel for Dart to signal completion.
        final MethodChannel channel = new MethodChannel(
            flutterEngine.getDartExecutor().getBinaryMessenger(),
            CHANNEL_NAME
        );
        channel.setMethodCallHandler(new MethodChannel.MethodCallHandler() {
          @Override
          public void onMethodCall(@NonNull MethodCall call, @NonNull MethodChannel.Result result) {
            if (call.method.equals("jobFinished")) {
              Log.i(TAG, "Dart signaled job finished");
              destroyEngineAndFinish(params);
              result.success(null);
            } else {
              result.notImplemented();
            }
          }
        });

        // Set up safety timeout.
        timeoutHandler = new Handler(Looper.getMainLooper());
        timeoutRunnable = new Runnable() {
          @Override
          public void run() {
            Log.w(TAG, "Safety timeout reached. Force stopping job.");
            destroyEngineAndFinish(params);
          }
        };
        timeoutHandler.postDelayed(timeoutRunnable, SAFETY_TIMEOUT_MS);

        final DartExecutor.DartCallback dartCallback = new DartExecutor.DartCallback(
            context.getAssets(),
            flutterLoader.findAppBundlePath(),
            callbackInfo
        );

        flutterEngine.getDartExecutor().executeDartCallback(dartCallback);
      }
    } catch (Exception e) {
      Log.e(TAG, "Failed to start Flutter engine for boot callback", e);
      cleanup();
      return false;
    }

    return true;
  }

  @Override
  public boolean onStopJob(@NonNull JobParameters params) {
    Log.i(TAG, "Stopping FlutterBootJobService (onStopJob)");
    synchronized (lifecycleLock) {
      jobFinishedCalled = true;
      cleanup();
    }
    return false; // Do not reschedule
  }

  private void destroyEngineAndFinish(@NonNull JobParameters params) {
    synchronized (lifecycleLock) {
      if (jobFinishedCalled) {
        return;
      }
      jobFinishedCalled = true;
      cleanup();
      jobFinished(params, false);
    }
  }

  private void cleanup() {
    if (timeoutHandler != null && timeoutRunnable != null) {
      timeoutHandler.removeCallbacks(timeoutRunnable);
      timeoutHandler = null;
      timeoutRunnable = null;
    }
    if (flutterEngine != null) {
      flutterEngine.destroy();
      flutterEngine = null;
    }
  }
}
