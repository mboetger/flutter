// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';
import 'dart:math';
import 'dart:typed_data';
import 'package:flutter_driver/flutter_driver.dart';
import 'package:test/test.dart' hide TypeMatcher, isInstanceOf;
import 'package:image/image.dart' as img;

void main() {
  group('Texture Mismatched Size Test', () {
    late FlutterDriver driver;

    setUpAll(() async {
      driver = await FlutterDriver.connect();
    });

    tearDownAll(() async {
      await driver.close();
    });


    test('reproduces wrong size or green bar when useCorrectBufferSize is false', () async {
      final SerializableFinder playWrongButton = find.byValueKey('play_wrong_size');
      final SerializableFinder playCorrectButton = find.byValueKey('play_correct_size');
      final SerializableFinder stopButton = find.byValueKey('stop');
      final SerializableFinder textureContainer = find.byValueKey('texture_container');

      // Wait for the app to be ready
      await driver.waitFor(playWrongButton);

      // 1. Play with WRONG size (mismatched buffer size)
      print('Playing video with mismatched buffer size...');
      await driver.tap(playWrongButton);
      // Wait for video to load and play for a few seconds
      await Future<void>.delayed(const Duration(seconds: 3));
      print('Status: ${await driver.getText(find.byValueKey('status'))}');

      // Capture screenshot
      print('Capturing screenshot of mismatched size playback...');
      final List<int> wrongScreenshotBytes = await driver.screenshot();
      final img.Image wrongImage = img.decodePng(Uint8List.fromList(wrongScreenshotBytes))!;

      final String wrongMatrixStr = await driver.requestData('getTransformMatrix');
      print('Transform matrix with WRONG size: $wrongMatrixStr');

      final String wasSetWrong = await driver.requestData('wasBufferSizeSetCorrectly');
      print('wasBufferSizeSetCorrectly with WRONG size: $wasSetWrong');

      // Check for green pixels in the texture area
      final double pixelRatio = double.parse(await driver.requestData('getPixelRatio'));
      print('Device pixel ratio: $pixelRatio');
      final DriverOffset center = await driver.getCenter(textureContainer);
      print('Center of texture container: $center');
      
      final int left = ((center.dx - 160) * pixelRatio).round();
      final int top = ((center.dy - 90) * pixelRatio).round();
      final int width = (320 * pixelRatio).round();
      final int height = (180 * pixelRatio).round();
      print('Texture physical bounds: left=$left, top=$top, width=$width, height=$height');

      print('wrongImage dimensions: ${wrongImage.width}x${wrongImage.height}');
      int greenPixelCountWrong = 0;
      Map<String, int> colorCounts = {};
      for (int y = top; y < top + height; y++) {
        for (int x = left; x < left + width; x++) {
          final img.Pixel pixel = wrongImage.getPixel(x, y);
          final num r = pixel.r;
          final num g = pixel.g;
          final num b = pixel.b;
          
          if (x % 100 == 0 && y % 50 == 0) {
            print('Sample pixel at ($x, $y): R=$r, G=$g, B=$b');
          }
          
          if (g > 80 && r < 50 && b < 50) {
            greenPixelCountWrong++;
          }
          
          final String colorKey = 'R=${r.round()},G=${g.round()},B=${b.round()}';
          colorCounts[colorKey] = (colorCounts[colorKey] ?? 0) + 1;
        }
      }

      print('Green pixels with WRONG size: $greenPixelCountWrong');
      // Print the top 10 most common colors
      final List<MapEntry<String, int>> sortedColors = colorCounts.entries.toList()
        ..sort((MapEntry<String, int> a, MapEntry<String, int> b) => b.value.compareTo(a.value));
      print('Top 10 colors in texture area:');
      for (int i = 0; i < min(10, sortedColors.length); i++) {
        print('  ${sortedColors[i].key}: ${sortedColors[i].value} pixels');
      }

      // 2. Play with CORRECT size
      print('Stopping video...');
      await driver.tap(stopButton);
      await Future<void>.delayed(const Duration(seconds: 2));
      
      print('Playing video with correct buffer size...');
      await driver.tap(playCorrectButton);
      await Future<void>.delayed(const Duration(seconds: 3));
      print('Status: ${await driver.getText(find.byValueKey('status'))}');

      print('Capturing screenshot of correct size playback...');
      final List<int> correctScreenshotBytes = await driver.screenshot();
      final img.Image correctImage = img.decodePng(Uint8List.fromList(correctScreenshotBytes))!;

      final String correctMatrixStr = await driver.requestData('getTransformMatrix');
      print('Transform matrix with CORRECT size: $correctMatrixStr');

      final String wasSetCorrect = await driver.requestData('wasBufferSizeSetCorrectly');
      print('wasBufferSizeSetCorrectly with CORRECT size: $wasSetCorrect');

      int greenPixelCountCorrect = 0;
      for (int y = top; y < top + height; y++) {
        for (int x = left; x < left + width; x++) {
          final img.Pixel pixel = correctImage.getPixel(x, y);
          final num r = pixel.r;
          final num g = pixel.g;
          final num b = pixel.b;
          
          if (g > 80 && r < 50 && b < 50) {
            greenPixelCountCorrect++;
          }
        }
      }

      print('Green pixels with CORRECT size: $greenPixelCountCorrect');
      
      expect(wasSetWrong, 'false', reason: 'Expected buffer size to NOT be set correctly when buffer size is mismatched');
      expect(wasSetCorrect, 'true', reason: 'Expected buffer size to BE set correctly when buffer size is correct');

    }, timeout: const Timeout(Duration(minutes: 3)));
  });
}
