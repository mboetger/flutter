// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

@Timeout(Duration(minutes: 5))
library;

import 'package:archive/archive.dart';
import 'package:file/file.dart';
import 'package:flutter_tools/src/base/io.dart';

import '../src/common.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;

  setUp(() {
    tempDir = createResolvedTempDirectorySync('android_so_compression_test.');
  });

  tearDown(() {
    tryToDelete(tempDir);
  });

  testWithoutContext('built APK has uncompressed .so files', () async {
    final Directory projectDir = tempDir.childDirectory('app');

    // 1. Create a Flutter app template.
    final ProcessResult createResult = await processManager.run(<String>[
      flutterBin,
      ...getLocalEngineArguments(),
      'create',
      '--template=app',
      '--platforms=android',
      'app',
    ], workingDirectory: tempDir.path);
    expect(
      createResult.exitCode,
      0,
      reason:
          'flutter create failed:\nStdout:\n${createResult.stdout}\nStderr:\n${createResult.stderr}',
    );

    // 2. Build APK in release mode.
    final ProcessResult buildResult = await processManager.run(<String>[
      flutterBin,
      ...getLocalEngineArguments(),
      'build',
      'apk',
      '--release',
      '--target-platform=android-arm,android-arm64',
    ], workingDirectory: projectDir.path);
    expect(
      buildResult.exitCode,
      0,
      reason:
          'flutter build apk failed:\nStdout:\n${buildResult.stdout}\nStderr:\n${buildResult.stderr}',
    );

    final File apkFile = projectDir
        .childDirectory('build')
        .childDirectory('app')
        .childDirectory('outputs')
        .childDirectory('flutter-apk')
        .childFile('app-release.apk');
    expect(apkFile.existsSync(), isTrue);

    final List<int> apkBytes = apkFile.readAsBytesSync();
    final Archive archive = ZipDecoder().decodeBytes(apkBytes);

    var foundSo = false;
    for (final ArchiveFile file in archive.files) {
      if (file.name.endsWith('.so')) {
        foundSo = true;
        // We expect .so files to be uncompressed (STORE).
        expect(
          file.compressionType,
          equals(ArchiveFile.STORE),
          reason:
              'File ${file.name} is compressed (compression type ${file.compressionType}), but it must be uncompressed.',
        );
      }
    }
    expect(foundSo, isTrue, reason: 'No .so files found in the APK');
  });
}
