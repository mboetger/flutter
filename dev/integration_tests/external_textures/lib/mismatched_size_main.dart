// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_driver/driver_extension.dart';

void main() {
  enableFlutterDriverExtension(handler: (String? message) async {
    if (message == 'getPixelRatio') {
      return WidgetsBinding.instance.platformDispatcher.views.first.devicePixelRatio.toString();
    } else if (message == 'getTransformMatrix') {
      try {
        final Float32List? matrix = await channel.invokeMethod<Float32List>('getTransformMatrix');
        return matrix?.join(',') ?? '';
      } catch (e) {
        return 'error: $e';
      }
    } else if (message == 'wasBufferSizeSetCorrectly') {
      try {
        final bool? result = await channel.invokeMethod<bool>('wasBufferSizeSetCorrectly');
        return result.toString();
      } catch (e) {
        return 'error: $e';
      }
    }
    return '';
  });
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State createState() => MyAppState();
}

const MethodChannel channel = MethodChannel('texture');

class MyAppState extends State<MyApp> {
  String _status = 'Idle';
  int? _textureId;

  @override
  void initState() {
    super.initState();
    _fetchTextureId();
  }

  Future<void> _fetchTextureId() async {
    for (int i = 0; i < 30; i++) {
      try {
        final int? id = await channel.invokeMethod<int>('getTextureId');
        if (id != null && id >= 0) {
          setState(() {
            _textureId = id;
          });
          print('Fetched texture ID: $_textureId');
          return;
        }
      } catch (e) {
        // ignore
      }
      await Future<void>.delayed(const Duration(milliseconds: 300));
    }
    setState(() {
      _status = 'Error: Failed to fetch texture ID';
    });
  }

  Future<void> _playVideo(String url, bool useCorrectBufferSize) async {
    setState(() {
      _status = 'Playing (correct size: $useCorrectBufferSize)';
    });
    try {
      await channel.invokeMethod<void>('playVideo', <String, dynamic>{
        'url': url,
        'useCorrectBufferSize': useCorrectBufferSize,
      });
    } catch (e) {
      setState(() {
        _status = 'Error: $e';
      });
    }
  }

  Future<void> _stopVideo() async {
    setState(() {
      _status = 'Stopped';
    });
    await channel.invokeMethod<void>('stopVideo');
  }

  @override
  void dispose() {
    _stopVideo();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        body: Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: <Widget>[
              // Draw the texture at 320x180 (16:9 aspect ratio)
              Container(
                key: const ValueKey<String>('texture_container'),
                width: 320.0,
                height: 180.0,
                color: Colors.blue, // fallback background
                child: _textureId != null
                    ? Texture(textureId: _textureId!)
                    : const SizedBox(),
              ),
              const SizedBox(height: 20),
              Text(
                _status,
                key: const ValueKey<String>('status'),
                style: const TextStyle(fontSize: 18),
              ),
              const SizedBox(height: 20),
              ElevatedButton(
                key: const ValueKey<String>('play_wrong_size'),
                onPressed: () => _playVideo(
                  'https://github.com/intel-iot-devkit/sample-videos/raw/master/bolt-detection.mp4',
                  false,
                ),
                child: const Text('Play Mismatched Size (Wrong)'),
              ),
              ElevatedButton(
                key: const ValueKey<String>('play_correct_size'),
                onPressed: () => _playVideo(
                  'https://github.com/intel-iot-devkit/sample-videos/raw/master/bolt-detection.mp4',
                  true,
                ),
                child: const Text('Play Correct Size'),
              ),
              ElevatedButton(
                key: const ValueKey<String>('start_baseline'),
                onPressed: () async {
                  setState(() { _status = 'Running Baseline'; });
                  await channel.invokeMethod<void>('start', 30);
                },
                child: const Text('Start Baseline Drawer'),
              ),
              ElevatedButton(
                key: const ValueKey<String>('stop_baseline'),
                onPressed: () async {
                  setState(() { _status = 'Stopped Baseline'; });
                  await channel.invokeMethod<void>('stop');
                },
                child: const Text('Stop Baseline Drawer'),
              ),
              ElevatedButton(
                key: const ValueKey<String>('stop'),
                onPressed: _stopVideo,
                child: const Text('Stop'),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
