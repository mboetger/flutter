// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package io.flutter.embedding.engine;

import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import io.flutter.Log;

/**
 * A {@link BroadcastReceiver} that receives {@link Intent#ACTION_BOOT_COMPLETED}
 * and schedules {@link FlutterBootJobService} to run the registered Dart callback.
 */
public class FlutterBootReceiver extends BroadcastReceiver {
  private static final String TAG = "FlutterBootReceiver";
  private static final String SHARED_PREFS_NAME = "io.flutter.embedding.engine.FlutterBootReceiver";
  private static final String KEY_CALLBACK_HANDLE = "callback_handle";
  
  @VisibleForTesting
  static final int JOB_ID = 191823791;

  @Override
  public void onReceive(@NonNull Context context, @NonNull Intent intent) {
    Log.i(TAG, "Received intent: " + intent.getAction());
    if (Intent.ACTION_BOOT_COMPLETED.equals(intent.getAction())) {
      scheduleJob(context);
    }
  }

  /**
   * Registers a Dart callback to be executed when the device boots.
   *
   * @param context The application context.
   * @param callbackHandle The handle of the Dart callback.
   */
  public static void setBootCallback(@NonNull Context context, long callbackHandle) {
    Log.i(TAG, "Setting boot callback handle: " + callbackHandle);
    SharedPreferences prefs = context.getSharedPreferences(SHARED_PREFS_NAME, Context.MODE_PRIVATE);
    prefs.edit().putLong(KEY_CALLBACK_HANDLE, callbackHandle).apply();

    setReceiverEnabled(context, true);
  }

  /**
   * Clears the registered Dart callback for device boot.
   *
   * @param context The application context.
   */
  public static void clearBootCallback(@NonNull Context context) {
    Log.i(TAG, "Clearing boot callback");
    SharedPreferences prefs = context.getSharedPreferences(SHARED_PREFS_NAME, Context.MODE_PRIVATE);
    prefs.edit().remove(KEY_CALLBACK_HANDLE).apply();

    setReceiverEnabled(context, false);
  }

  static long getBootCallbackHandle(@NonNull Context context) {
    SharedPreferences prefs = context.getSharedPreferences(SHARED_PREFS_NAME, Context.MODE_PRIVATE);
    return prefs.getLong(KEY_CALLBACK_HANDLE, 0);
  }

  private static void setReceiverEnabled(@NonNull Context context, boolean enabled) {
    ComponentName receiver = new ComponentName(context, FlutterBootReceiver.class);
    PackageManager pm = context.getPackageManager();
    int state = enabled
        ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
        : PackageManager.COMPONENT_ENABLED_STATE_DISABLED;
    pm.setComponentEnabledSetting(receiver, state, PackageManager.DONT_KILL_APP);
  }

  private void scheduleJob(@NonNull Context context) {
    Log.i(TAG, "Scheduling FlutterBootJobService");
    ComponentName serviceComponent = new ComponentName(context, FlutterBootJobService.class);
    JobInfo.Builder builder = new JobInfo.Builder(JOB_ID, serviceComponent);
    // We want it to run immediately.
    builder.setMinimumLatency(0);
    builder.setOverrideDeadline(0);

    JobScheduler jobScheduler = (JobScheduler) context.getSystemService(Context.JOB_SCHEDULER_SERVICE);
    if (jobScheduler != null) {
      jobScheduler.schedule(builder.build());
    } else {
      Log.e(TAG, "JobScheduler not available");
    }
  }
}
