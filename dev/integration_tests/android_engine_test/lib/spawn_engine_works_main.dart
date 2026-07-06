// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:convert';
import 'dart:ui';

import 'package:android_driver_extensions/extension.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_driver/driver_extension.dart';

import 'src/allow_list_devices.dart';

void main() async {
  ensureAndroidDevice();
  if (PlatformDispatcher.instance.engineId == 1) {
    const MethodChannel(
      'com.example.android_engine_test/spawn',
    ).invokeMethod<void>('spawn_and_destroy');
    return;
  }

  enableFlutterDriverExtension(
    handler: (String? command) async {
      return json.encode(<String, Object?>{
        'engineId': PlatformDispatcher.instance.engineId,
        'status': 'ready',
      });
    },
    commands: <CommandExtension>[nativeDriverCommands],
  );

  // Run on full screen.
  await SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersive);

  runApp(const MainApp());
}

final class MainApp extends StatelessWidget {
  const MainApp({super.key});

  @override
  Widget build(BuildContext context) {
    return const MaterialApp(
      home: Scaffold(
        body: Center(
          child: Text(
            'One more thing...',
            style: TextStyle(fontFamily: "some font that doesn't exist", fontSize: 80),
          ),
        ),
      ),
    );
  }
}
