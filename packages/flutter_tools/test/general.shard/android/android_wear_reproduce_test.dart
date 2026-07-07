// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:flutter_tools/src/android/android_emulator.dart';
import 'package:flutter_tools/src/android/android_sdk.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/device.dart';
import 'package:test/fake.dart';
import 'package:test/test.dart';

import '../../src/common.dart';
import '../../src/fake_process_manager.dart';

void main() {
  group('Android Wear support reproduction tests', () {
    testWithoutContext('Android emulator category is watch if it is a wear device/emulator', () {
      final properties = <String, String>{
        'hw.device.name': 'Android Wear Round',
        'tag.id': 'android-wear',
      };
      
      final emulator = AndroidEmulator(
        'wear_emulator',
        properties: properties,
        logger: BufferLogger.test(),
        processManager: FakeProcessManager.any(),
        androidSdk: FakeAndroidSdk(),
      );

      // Verify that category is watch/wearable.
      // This will fail currently because the "watch" category does not exist,
      // and emulator.category returns Category.mobile.
      expect(emulator.category, Category.fromString('watch'));
    });
  });
}

class FakeAndroidSdk extends Fake implements AndroidSdk {}
