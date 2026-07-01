// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:ui';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'system_channels.dart';

/// Provides APIs for Android boot integration.
abstract final class AndroidBoot {
  /// Registers a Dart callback to be executed when the Android device boots.
  ///
  /// The [callback] must be a top-level or static function.
  ///
  /// It must be annotated with `@pragma('vm:entry-point')` to prevent it
  /// from being tree-shaken in release mode.
  ///
  /// The app must also declare the `android.permission.RECEIVE_BOOT_COMPLETED`
  /// permission in its `AndroidManifest.xml`. The receiver and service
  /// are automatically merged from the Flutter embedding.
  static Future<void> setCallback(Function callback) async {
    if (defaultTargetPlatform != TargetPlatform.android) {
      return;
    }
    final CallbackHandle? handle = PlatformDispatcher.instance.toCallbackHandle(callback);
    if (handle == null) {
      throw ArgumentError('The callback must be a top-level or static function.');
    }
    await SystemChannels.platform.invokeMethod<void>(
      'AndroidBoot.setCallback',
      handle.toRawHandle(),
    );
  }

  /// Clears the registered Dart callback for device boot.
  static Future<void> clearCallback() async {
    if (defaultTargetPlatform != TargetPlatform.android) {
      return;
    }
    await SystemChannels.platform.invokeMethod<void>('AndroidBoot.clearCallback');
  }

  /// Signals that the boot job has finished.
  ///
  /// This must be called at the end of the boot callback to allow the system
  /// to release resources and shut down the background process.
  static Future<void> jobFinished() async {
    if (defaultTargetPlatform != TargetPlatform.android) {
      return;
    }
    const MethodChannel channel = MethodChannel('io.flutter/boot');
    await channel.invokeMethod<void>('jobFinished');
  }
}
