// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:integration_test/integration_test.dart';
import 'package:path/path.dart' as path;

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('Write to app-specific external storage succeeds', (
    WidgetTester tester,
  ) async {
    if (kIsWeb || !Platform.isAndroid) {
      return;
    }

    // Under Android Scoped Storage (enforced on Android 11+ / API 30+),
    // writing directly to the root of external storage (/storage/emulated/0/)
    // is blocked by the OS and throws a FileSystemException.
    //
    // Instead, apps should write to their app-specific external storage directory,
    // which does not require any runtime permissions and is always writable.
    const channel = MethodChannel('integration_ui/storage');
    final String? externalFilesDirPath = await channel.invokeMethod<String>('getExternalFilesDir');
    expect(externalFilesDirPath, isNotNull);

    final directory = Directory(externalFilesDirPath!);
    if (!directory.existsSync()) {
      directory.createSync(recursive: true);
    }

    final file = File(path.join(directory.path, 'test_write.txt'));
    debugPrint('Attempting to write to ${file.path}');

    try {
      await file.writeAsString('success');
      expect(file.existsSync(), isTrue);
      expect(await file.readAsString(), 'success');
      debugPrint('Write succeeded!');
    } finally {
      if (file.existsSync()) {
        await file.delete();
      }
    }
  });
}
