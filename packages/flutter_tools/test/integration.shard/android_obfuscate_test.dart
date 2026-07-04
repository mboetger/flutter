// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:convert';
import 'dart:typed_data';

import 'package:archive/archive.dart';
import 'package:file_testing/file_testing.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/io.dart';

import '../src/common.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;

  setUp(() async {
    tempDir = fileSystem.systemTempDirectory.createTempSync('flutter_tools_packages_test.');
  });

  tearDown(() {
    tryToDelete(tempDir);
  });

  test('Dart identifiers are obfuscated with build apk --obfuscate', () async {
    const projectName = 'hello_world';
    await processManager.run(<String>[
      flutterBin,
      'create',
      projectName,
    ], workingDirectory: tempDir.path);
    final String projectPath = tempDir.childDirectory(projectName).path;

    await processManager.run(<String>[
      flutterBin,
      'build',
      'apk',
      '--target-platform=android-arm',
      '--obfuscate',
      '--split-debug-info=foo/',
      '--ci',
    ], workingDirectory: projectPath);

    final File outputApkDirectory = fileSystem.file(
      fileSystem.path.join(
        projectPath,
        'build',
        'app',
        'outputs',
        'apk',
        'release',
        'app-release.apk',
      ),
    );

    expect(outputApkDirectory, exists);
    // Expect "hello_world" is not present in the compiled output.
    // This fails without the --obfuscate flag.
    expect(_containsSymbol(outputApkDirectory, 'lib/armeabi-v7a/libapp.so', projectName), false);
  });

  test('Dart identifiers are obfuscated with build aar --obfuscate', () async {
    const moduleName = 'hello_module';
    await processManager.run(<String>[
      flutterBin,
      'create',
      '-t',
      'module',
      moduleName,
      '--ci',
    ], workingDirectory: tempDir.path);
    final String projectPath = tempDir.childDirectory(moduleName).path;

    await processManager.run(<String>[
      flutterBin,
      'build',
      'aar',
      '--target-platform=android-arm',
      '--obfuscate',
      '--split-debug-info=foo/',
      '--no-debug',
      '--no-profile',
    ], workingDirectory: projectPath);

    final File outputAarDirectory = fileSystem.file(
      fileSystem.path.join(
        projectPath,
        'build',
        'host',
        'outputs',
        'repo',
        'com',
        'example',
        moduleName,
        'flutter_release',
        '1.0',
        'flutter_release-1.0.aar',
      ),
    );

    expect(outputAarDirectory, exists);
    // Expect "hello_module" is not present in the compiled output.
    // This fails without the --obfuscate flag.
    expect(_containsSymbol(outputAarDirectory, 'jni/armeabi-v7a/libapp.so', moduleName), false);
  });

  test(
    'Proguard mapping.txt includes Dart symbols when building apk with minifyEnabled and obfuscate (issue 63803)',
    () async {
      const projectName = 'hello_world';
      await processManager.run(<String>[
        flutterBin,
        'create',
        projectName,
      ], workingDirectory: tempDir.path);
      final String projectPath = tempDir.childDirectory(projectName).path;

      final File mainDart = fileSystem.file(fileSystem.path.join(projectPath, 'lib', 'main.dart'));
      mainDart.writeAsStringSync('''
import 'package:flutter/material.dart';

void main() => runApp(const MyApp());

class MyApp extends StatelessWidget {
  const MyApp({super.key});
  @override
  Widget build(BuildContext context) => const MaterialApp(home: CustomDartWidgetFor63803());
}

class CustomDartWidgetFor63803 extends StatelessWidget {
  const CustomDartWidgetFor63803({super.key});
  @override
  Widget build(BuildContext context) => const Text('Hello 63803');
}
''');

      // Enable Proguard/R8 in android/app/build.gradle.kts as reported in issue 63803.
      final File buildGradle = fileSystem.file(
        fileSystem.path.join(projectPath, 'android', 'app', 'build.gradle.kts'),
      );
      final String buildGradleContents = buildGradle.readAsStringSync();
      final String updatedGradle = buildGradleContents.replaceFirst(
        'signingConfig = signingConfigs.getByName("debug")',
        'signingConfig = signingConfigs.getByName("debug")\n            isMinifyEnabled = true\n            isShrinkResources = true\n            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")',
      );
      if (updatedGradle == buildGradleContents) {
        throw StateError('Failed to enable R8 in ${buildGradle.path}');
      }
      buildGradle.writeAsStringSync(updatedGradle);

      // Create proguard-rules.pro as reported in issue 63803.
      final File proguardRules = fileSystem.file(
        fileSystem.path.join(projectPath, 'android', 'app', 'proguard-rules.pro'),
      );
      proguardRules.writeAsStringSync('''
-keep class io.flutter.app.** { *; }
-keep class io.flutter.plugin.**  { *; }
-keep class io.flutter.util.**  { *; }
-keep class io.flutter.view.**  { *; }
-keep class io.flutter.**  { *; }
-keep class io.flutter.plugins.**  { *; }
-dontwarn android.**
-dontwarn com.google.android.play.core.**
-ignorewarnings
''');

      // Add extra-gen-snapshot-options=--obfuscate to gradle.properties as reported in issue 63803.
      final File gradleProperties = fileSystem.file(
        fileSystem.path.join(projectPath, 'android', 'gradle.properties'),
      );
      gradleProperties.writeAsStringSync(
        '\nextra-gen-snapshot-options=--obfuscate\n',
        mode: FileMode.append,
      );

      final ProcessResult result = await processManager.run(<String>[
        flutterBin,
        'build',
        'apk',
        '--target-platform=android-arm',
        '--ci',
      ], workingDirectory: projectPath);
      expect(
        result.exitCode,
        0,
        reason: 'flutter build apk failed:\n${result.stdout}\n${result.stderr}',
      );

      final File mappingTxt = fileSystem.file(
        fileSystem.path.join(
          projectPath,
          'build',
          'app',
          'outputs',
          'mapping',
          'release',
          'mapping.txt',
        ),
      );

      expect(mappingTxt, exists);
      final String mappingContent = mappingTxt.readAsStringSync();

      // In issue 63803, the user expects mapping.txt to include Dart symbols from the app's lib directory
      // so that Proguard's retrace tool can deobfuscate Dart stack traces.
      // On the current codebase, mapping.txt only contains Java/Kotlin symbols and does not include Dart symbols.
      expect(mappingContent, contains('CustomDartWidgetFor63803'));
    },
  );
}

bool _containsSymbol(File outputArchive, String libappPath, String symbol) {
  final Archive archive = ZipDecoder().decodeBytes(outputArchive.readAsBytesSync());
  final ArchiveFile? libapp = archive.findFile(libappPath);
  expect(libapp, isNotNull);

  final libappBytes = libapp!.content as Uint8List;
  final String libappStrings = utf8.decode(libappBytes, allowMalformed: true);

  return libappStrings.contains(symbol);
}
