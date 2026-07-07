// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.android;

import android.app.Notification;
import android.app.Service;
import android.content.Intent;
import android.os.Build;
import android.os.IBinder;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import io.flutter.FlutterInjector;
import io.flutter.Log;
import io.flutter.embedding.engine.FlutterEngine;
import io.flutter.embedding.engine.dart.DartExecutor;
import io.flutter.embedding.engine.loader.FlutterLoader;
import io.flutter.plugin.common.MethodCall;
import io.flutter.plugin.common.MethodChannel;
import java.util.List;

/**
 * A background or foreground Android {@link Service} that executes Dart code in a {@link
 * FlutterEngine}.
 */
public class FlutterService extends Service {
  private static final String TAG = "FlutterService";
  private static final String CHANNEL_NAME = "io.flutter/background_service";

  public static final String EXTRA_DART_ENTRYPOINT = "dart_entrypoint";
  public static final String EXTRA_DART_ENTRYPOINT_ARGS = "dart_entrypoint_args";
  public static final String EXTRA_FOREGROUND_SERVICE = "foreground_service";
  public static final String EXTRA_FOREGROUND_NOTIFICATION_ID = "foreground_notification_id";
  public static final String EXTRA_FOREGROUND_SERVICE_TYPE = "foreground_service_type";

  protected FlutterEngine flutterEngine;
  private MethodChannel methodChannel;

  @Nullable
  @Override
  public IBinder onBind(@NonNull Intent intent) {
    return null;
  }

  @Override
  public int onStartCommand(@Nullable Intent intent, int flags, int startId) {
    if (intent == null) {
      Log.w(TAG, "FlutterService started with a null Intent. Stopping service.");
      stopSelf();
      return START_NOT_STICKY;
    }

    if (flutterEngine == null) {
      flutterEngine = provideFlutterEngine();

      methodChannel =
          new MethodChannel(flutterEngine.getDartExecutor().getBinaryMessenger(), CHANNEL_NAME);
      methodChannel.setMethodCallHandler(
          new MethodChannel.MethodCallHandler() {
            @Override
            public void onMethodCall(
                @NonNull MethodCall call, @NonNull MethodChannel.Result result) {
              if (call.method.equals("stopService")) {
                stopSelf();
                result.success(null);
              } else {
                result.notImplemented();
              }
            }
          });

      String entrypointName = intent.getStringExtra(EXTRA_DART_ENTRYPOINT);
      if (entrypointName == null) {
        entrypointName = "main";
      }
      List<String> entrypointArgs = intent.getStringArrayListExtra(EXTRA_DART_ENTRYPOINT_ARGS);

      FlutterLoader flutterLoader = FlutterInjector.instance().flutterLoader();
      String pathToBundle = flutterLoader.findAppBundlePath();
      DartExecutor.DartEntrypoint dartEntrypoint =
          new DartExecutor.DartEntrypoint(pathToBundle, entrypointName);
      flutterEngine.getDartExecutor().executeDartEntrypoint(dartEntrypoint, entrypointArgs);
    }

    boolean isForeground = intent.getBooleanExtra(EXTRA_FOREGROUND_SERVICE, false);
    if (isForeground) {
      int notificationId = intent.getIntExtra(EXTRA_FOREGROUND_NOTIFICATION_ID, 0);
      if (notificationId == 0) {
        Log.e(TAG, "Invalid foreground notification ID (0). Refusing to start foreground service.");
      } else {
        Notification notification = getForegroundNotification();
        if (notification != null) {
          if (intent.hasExtra(EXTRA_FOREGROUND_SERVICE_TYPE)
              && Build.VERSION.SDK_INT >= io.flutter.Build.API_LEVELS.API_29) {
            int serviceType = intent.getIntExtra(EXTRA_FOREGROUND_SERVICE_TYPE, 0);
            startForeground(notificationId, notification, serviceType);
          } else {
            startForeground(notificationId, notification);
          }
        } else {
          Log.e(
              TAG,
              "getForegroundNotification() returned null. Foreground service cannot start. "
                  + "Ensure getForegroundNotification() is overridden and returns a valid Notification "
                  + "when EXTRA_FOREGROUND_SERVICE is true.");
        }
      }
    }

    return START_NOT_STICKY;
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

  /**
   * Provides the {@link Notification} to use when running as a foreground service.
   *
   * <p>Subclasses must override this to supply a valid {@code Notification} when using foreground
   * execution mode.
   */
  @Nullable
  protected Notification getForegroundNotification() {
    return null;
  }

  @Override
  public void onDestroy() {
    if (methodChannel != null) {
      methodChannel.setMethodCallHandler(null);
      methodChannel = null;
    }
    if (flutterEngine != null) {
      flutterEngine.destroy();
      flutterEngine = null;
    }
    super.onDestroy();
  }
}
