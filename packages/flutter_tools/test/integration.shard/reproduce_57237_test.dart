// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:archive/archive.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/build_info.dart';

import '../src/common.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;

  setUp(() async {
    tempDir = createResolvedTempDirectorySync('flutter_reproduce_57237_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  testWithoutContext(
    'libflutter.so is included in armeabi-v7a when abiFilters is armeabi-v7a and target-platform is android-x64',
    () async {
      final Directory projectDir = tempDir.childDirectory('app');

      // 1. Create a new flutter project
      processManager.runSync(<String>[
        flutterBin,
        'create',
        '--template=app',
        '--platforms=android',
        'app',
      ], workingDirectory: tempDir.path);

      // 2. Put a dummy .so library in the folder android/app/src/main/jniLibs/armeabi-v7a
      final Directory armDir = projectDir.childDirectory(
        'android/app/src/main/jniLibs/armeabi-v7a',
      );
      armDir.createSync(recursive: true);
      armDir.childFile('libcustom.so').writeAsStringSync('dummy elf content');

      // 3. Modify build.gradle.kts to add ndk.abiFilters and keep dummy .so from stripping
      final File buildGradle = projectDir.childFile('android/app/build.gradle.kts');
      final String buildGradleContents = buildGradle.readAsStringSync();

      final String updatedBuildGradleContents = buildGradleContents.replaceFirst(
        'defaultConfig {',
        '''
defaultConfig {
        ndk {
            abiFilters.clear()
            abiFilters.addAll(listOf("armeabi-v7a"))
        }
        packaging {
            jniLibs {
                keepDebugSymbols.add("**/libcustom.so")
            }
        }''',
      );
      buildGradle.writeAsStringSync(updatedBuildGradleContents);

      // 4. Build the app in debug mode targeting android-x64 (simulating running on an x86_64 emulator)
      processManager.runSync(<String>[
        flutterBin,
        ...getLocalEngineArguments(),
        'build',
        'apk',
        '--debug',
        '--target-platform',
        'android-x64',
      ], workingDirectory: projectDir.path);

      // 5. Verify that libflutter.so is packaged in the APK under lib/armeabi-v7a
      expect(
        _checkLibIsInApk(projectDir, 'lib/armeabi-v7a/libflutter.so', buildMode: BuildMode.debug),
        true,
      );
    },
  );
}

bool _checkLibIsInApk(
  Directory appDir,
  String filename, {
  BuildMode buildMode = BuildMode.release,
  String productFlavor = '',
}) {
  final apkName = productFlavor.isEmpty
      ? 'app-${buildMode.cliName}.apk'
      : 'app-$productFlavor-${buildMode.cliName}.apk';

  final String apkDir = productFlavor.isEmpty
      ? buildMode.cliName
      : '$productFlavor/${buildMode.cliName}';

  final File apkFile = appDir.childDirectory('build/app/outputs/apk/$apkDir').childFile(apkName);

  if (!apkFile.existsSync()) {
    throw StateError('APK file not found at ${apkFile.path}');
  }

  final List<int> bytes = apkFile.readAsBytesSync();
  final Archive archive = ZipDecoder().decodeBytes(bytes);

  return archive.any((ArchiveFile file) => file.name == filename);
}
