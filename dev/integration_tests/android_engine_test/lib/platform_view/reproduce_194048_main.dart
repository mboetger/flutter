// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';
import 'dart:convert';

import 'package:android_driver_extensions/extension.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/scheduler.dart';
import 'package:flutter/services.dart';
import 'package:flutter_driver/driver_extension.dart';

import '../src/allow_list_devices.dart';

const MethodChannel _nativeDriverChannel = MethodChannel('native_driver');

final GlobalKey<_ReproAppState> _appKey = GlobalKey<_ReproAppState>();

void main() async {
  ensureAndroidDevice();
  enableFlutterDriverExtension(
    enableTextEntryEmulation: false,
    handler: (String? command) async {
      final _ReproAppState? state = _appKey.currentState;
      if (command == null || command.isEmpty || command == 'status') {
        return json.encode(<String, Object?>{
          'show': state?._showSecond,
          'secondMode': state?._secondMode,
        });
      }
      if (command == 'start_recording') {
        await _nativeDriverChannel.invokeMethod<void>('start_recording_frames');
        return json.encode(<String, Object?>{'ok': true});
      }
      if (command == 'stop_recording') {
        final List<Object?>? frames = await _nativeDriverChannel.invokeListMethod<Object?>(
          'stop_recording_frames',
        );
        return json.encode(<String, Object?>{'frames': frames});
      }
      if (command.startsWith('config:')) {
        final data = json.decode(command.substring('config:'.length)) as Map<String, Object?>;
        if (data['recordFrames'] == true) {
          await _nativeDriverChannel.invokeMethod<void>('start_recording_frames');
        }
        await state?.configure(
          secondMode: data['secondMode'] as String?,
          show: data['show'] as bool?,
          waitFrames: (data['waitFrames'] as int?) ?? 1,
        );
        return json.encode(<String, Object?>{'ok': true});
      }
      return json.encode(<String, Object?>{'error': 'unknown command: $command'});
    },
    commands: <CommandExtension>[nativeDriverCommands],
  );

  await SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersive);
  runApp(ReproApp(key: _appKey));
}

class ReproApp extends StatefulWidget {
  const ReproApp({super.key});

  @override
  State<ReproApp> createState() => _ReproAppState();
}

class _ReproAppState extends State<ReproApp> {
  bool _showSecond = false;
  String _secondMode = 'android_view_red_surface_view';

  Future<void> configure({String? secondMode, bool? show, int waitFrames = 1}) async {
    setState(() {
      if (secondMode != null) {
        _secondMode = secondMode;
      }
      if (show != null) {
        _showSecond = show;
      }
    });
    for (var i = 0; i < waitFrames; i++) {
      await SchedulerBinding.instance.endOfFrame;
    }
  }

  Widget _buildSecondView() {
    return switch (_secondMode) {
      'red_surface_view' => const _SurfacePlatformView(
        key: ValueKey<String>('second_red_surface_view'),
        viewType: 'red_surface_view_platform_view',
      ),
      _ => const AndroidView(
        key: ValueKey<String>('second_android_view_red_surface_view'),
        viewType: 'red_surface_view_platform_view',
      ),
    };
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      home: Scaffold(
        backgroundColor: Colors.black,
        body: Column(
          children: <Widget>[
            const Expanded(
              child: _SurfacePlatformView(
                key: ValueKey<String>('first_webview'),
                viewType: 'blue_webview_platform_view',
              ),
            ),
            if (_showSecond) SizedBox(height: 160, width: 200, child: _buildSecondView()),
          ],
        ),
      ),
    );
  }
}

final class _SurfacePlatformView extends StatelessWidget {
  const _SurfacePlatformView({super.key, required this.viewType});

  final String viewType;

  @override
  Widget build(BuildContext context) {
    return PlatformViewLink(
      viewType: viewType,
      surfaceFactory: (BuildContext context, PlatformViewController controller) {
        return AndroidViewSurface(
          controller: controller as AndroidViewController,
          gestureRecognizers: const <Factory<OneSequenceGestureRecognizer>>{},
          hitTestBehavior: PlatformViewHitTestBehavior.opaque,
        );
      },
      onCreatePlatformView: (PlatformViewCreationParams params) {
        return PlatformViewsService.initSurfaceAndroidView(
            id: params.id,
            viewType: viewType,
            layoutDirection: TextDirection.ltr,
            onFocus: () {
              params.onFocusChanged(true);
            },
          )
          ..addOnPlatformViewCreatedListener(params.onPlatformViewCreated)
          ..create();
      },
    );
  }
}
