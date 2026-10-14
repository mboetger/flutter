// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:convert';
import 'dart:io' as io;

import 'package:android_driver_extensions/native_driver.dart';
import 'package:flutter_driver/flutter_driver.dart';
import 'package:test/test.dart';

final RegExp _surfaceBotPattern = RegExp(r'_S\{[^}]*bot\(B(\d+)/R(\d+)/K(\d+)\)');
final RegExp _surfaceMidPattern = RegExp(r'_S\{[^}]*mid\(B(\d+)/R(\d+)/K(\d+)\)');
final RegExp _windowBotPattern = RegExp(r'_W\{[^}]*bot\(B(\d+)/R(\d+)/K(\d+)\)');
final RegExp _windowMidPattern = RegExp(r'_W\{[^}]*mid\(B(\d+)/R(\d+)/K(\d+)\)');

({int b, int r, int k})? _parseRegion(RegExp pattern, String frame) {
  final RegExpMatch? match = pattern.firstMatch(frame);
  if (match == null) {
    return null;
  }
  return (
    b: int.parse(match.group(1)!),
    r: int.parse(match.group(2)!),
    k: int.parse(match.group(3)!),
  );
}

/// Returns true if the settled frame renders red in the bottom region (`r > 0 && k == 0`)
/// either in the Flutter surface/window `PixelCopy` or via an attached child `SurfaceView`.
bool _hasRedBottom(String frame) {
  if (frame.contains('_SV{rgb(255,0,0)}')) {
    return true;
  }
  final ({int b, int r, int k})? surfaceBot = _parseRegion(_surfaceBotPattern, frame);
  if (surfaceBot != null) {
    return surfaceBot.r > 0 && surfaceBot.k == 0;
  }
  final ({int b, int r, int k})? windowBot = _parseRegion(_windowBotPattern, frame);
  if (windowBot != null && windowBot.r > 0 && windowBot.k == 0) {
    return true;
  }
  return false;
}

/// Returns true if the frame has an uncovered black gap in the bottom region (`k > 0 && r == 0`
/// and no active red `SurfaceView`).
bool _hasBlackBottomGap(String frame) {
  if (frame.contains('_SV{rgb(255,0,0)}')) {
    return false;
  }
  final ({int b, int r, int k})? surfaceBot = _parseRegion(_surfaceBotPattern, frame);
  if (surfaceBot != null) {
    return surfaceBot.k > 0 && surfaceBot.r == 0;
  }
  final ({int b, int r, int k})? windowBot = _parseRegion(_windowBotPattern, frame);
  if (windowBot != null && windowBot.k > 0 && windowBot.r == 0 && windowBot.b == 0) {
    return true;
  }
  return false;
}

/// Returns true if the frame exhibits a full-screen black flash in the middle of the window
/// (e.g. during `FlutterView.convertToImageView()` / `revertImageView()`).
bool _hasFullScreenBlackFlash(String frame) {
  final ({int b, int r, int k})? surfaceMid = _parseRegion(_surfaceMidPattern, frame);
  if (surfaceMid != null) {
    return surfaceMid.k > 0 && surfaceMid.b == 0;
  }
  final ({int b, int r, int k})? windowMid = _parseRegion(_windowMidPattern, frame);
  return windowMid != null && windowMid.k > 0 && windowMid.b == 0;
}

void main() async {
  FlutterDriver? flutterDriver;
  NativeDriver? nativeDriver;

  setUpAll(() async {
    final FlutterDriver driver = await FlutterDriver.connect();
    flutterDriver = driver;
    final String? deviceSerial = io.Platform.environment['ANDROID_SERIAL'];
    nativeDriver = await AndroidNativeDriver.connect(
      driver,
      target: (deviceSerial != null && deviceSerial.isNotEmpty)
          ? AndroidDeviceTarget.bySerial(deviceSerial)
          : null,
    );
    await driver.waitUntilFirstFrameRasterized();
    await nativeDriver!.waitUntilFirstFrameDisplayed();
  });

  tearDownAll(() async {
    await nativeDriver?.close();
    await flutterDriver?.close();
  });

  Future<List<String>> recordTransition(Map<String, Object?> config) async {
    final FlutterDriver driver = flutterDriver!;
    await driver.requestData(
      'config:${json.encode(<String, Object?>{...config, 'recordFrames': true})}',
    );
    await Future<void>.delayed(const Duration(milliseconds: 1000));
    final String response = await driver.requestData('stop_recording');
    final decoded = json.decode(response) as Map<String, Object?>;
    final List<Object?> frames = decoded['frames'] as List<Object?>? ?? <Object?>[];
    return frames.cast<String>();
  }

  test('AndroidView(SurfaceView) in Column with Expanded WebView renders red and has no black bottom gap on show/hide (reproduces #194048)', () async {
    await flutterDriver!.requestData(
      'config:${json.encode(<String, Object?>{'secondMode': 'android_view_red_surface_view', 'show': false, 'waitFrames': 3})}',
    );
    await Future<void>.delayed(const Duration(milliseconds: 300));

    final List<String> show1Frames = await recordTransition(<String, Object?>{
      'show': true,
      'waitFrames': 1,
    });
    final List<String> hideFrames = await recordTransition(<String, Object?>{
      'show': false,
      'waitFrames': 1,
    });
    final List<String> show2Frames = await recordTransition(<String, Object?>{
      'show': true,
      'waitFrames': 1,
    });

    expect(show1Frames, isNotEmpty);
    expect(hideFrames, isNotEmpty);
    expect(show2Frames, isNotEmpty);

    final String finalShow1 = show1Frames.lastWhere(
      (String f) => f.contains('bot('),
      orElse: () => show1Frames.last,
    );
    final String finalShow2 = show2Frames.lastWhere(
      (String f) => f.contains('bot('),
      orElse: () => show2Frames.last,
    );

    final List<String> show1GapFrames = show1Frames.where(_hasBlackBottomGap).toList();
    final List<String> hideGapFrames = hideFrames.where(_hasBlackBottomGap).toList();
    final List<String> show2GapFrames = show2Frames.where(_hasBlackBottomGap).toList();

    expect(
      _hasRedBottom(finalShow1),
      isTrue,
      reason:
          'AndroidView(RedSurfaceView) final SHOW 1 frame did not render red in bottom region '
          '(VirtualDisplay black box): $finalShow1\n'
          '  SHOW 1 frames with black bottom gap (${show1GapFrames.length}/${show1Frames.length}): $show1GapFrames\n'
          '  HIDE frames with black bottom gap (${hideGapFrames.length}/${hideFrames.length}): $hideGapFrames\n'
          '  SHOW 2 frames with black bottom gap (${show2GapFrames.length}/${show2Frames.length}): $show2GapFrames',
    );
    expect(
      _hasRedBottom(finalShow2),
      isTrue,
      reason:
          'AndroidView(RedSurfaceView) final SHOW 2 frame did not render red in bottom region: $finalShow2',
    );
    expect(
      show1GapFrames,
      isEmpty,
      reason: 'SHOW 1 had intermediate frames with a black bottom gap: $show1GapFrames',
    );
    expect(
      hideGapFrames,
      isEmpty,
      reason: 'HIDE had intermediate frames with a black bottom gap: $hideGapFrames',
    );
    expect(
      show2GapFrames,
      isEmpty,
      reason: 'SHOW 2 had intermediate frames with a black bottom gap: $show2GapFrames',
    );
  }, timeout: Timeout.none);

  test('AndroidViewSurface(SurfaceView) with hybrid fallback in Column with Expanded WebView has no black flash or bottom gap on show/hide (reproduces #194048)', () async {
    await flutterDriver!.requestData(
      'config:${json.encode(<String, Object?>{'secondMode': 'red_surface_view', 'show': false, 'waitFrames': 3})}',
    );
    await Future<void>.delayed(const Duration(milliseconds: 300));

    final List<String> hcShowFrames = await recordTransition(<String, Object?>{
      'show': true,
      'waitFrames': 1,
    });
    final List<String> hcHideFrames = await recordTransition(<String, Object?>{
      'show': false,
      'waitFrames': 1,
    });

    expect(hcShowFrames, isNotEmpty);
    expect(hcHideFrames, isNotEmpty);

    final String finalHcShow = hcShowFrames.lastWhere(
      (String f) => f.contains('bot('),
      orElse: () => hcShowFrames.last,
    );
    expect(
      _hasRedBottom(finalHcShow),
      isTrue,
      reason:
          'HybridFallback settled SHOW frame should render red SurfaceView at bottom: $finalHcShow',
    );

    final List<String> hcShowGapFrames = hcShowFrames.where(_hasBlackBottomGap).toList();
    final List<String> hcShowFlashFrames = hcShowFrames.where(_hasFullScreenBlackFlash).toList();
    final List<String> hcHideGapFrames = hcHideFrames.where(_hasBlackBottomGap).toList();
    final List<String> hcHideFlashFrames = hcHideFrames.where(_hasFullScreenBlackFlash).toList();

    expect(
      hcShowGapFrames,
      isEmpty,
      reason:
          'HybridFallback SHOW had intermediate frames with a black bottom gap:\n'
          '  hcShowGapFrames (${hcShowGapFrames.length}/${hcShowFrames.length}): $hcShowGapFrames\n'
          '  hcShowFlashFrames (${hcShowFlashFrames.length}/${hcShowFrames.length}): $hcShowFlashFrames\n'
          '  hcHideGapFrames (${hcHideGapFrames.length}/${hcHideFrames.length}): $hcHideGapFrames\n'
          '  hcHideFlashFrames (${hcHideFlashFrames.length}/${hcHideFrames.length}): $hcHideFlashFrames',
    );
    expect(
      hcShowFlashFrames,
      isEmpty,
      reason:
          'HybridFallback SHOW had intermediate full-screen black flash frames: $hcShowFlashFrames',
    );
    expect(
      hcHideGapFrames,
      isEmpty,
      reason:
          'HybridFallback HIDE had intermediate frames with a black bottom gap: $hcHideGapFrames',
    );
    expect(
      hcHideFlashFrames,
      isEmpty,
      reason:
          'HybridFallback HIDE had intermediate full-screen black flash frames: $hcHideFlashFrames',
    );
  }, timeout: Timeout.none);
}
