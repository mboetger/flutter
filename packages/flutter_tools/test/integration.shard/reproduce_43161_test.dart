// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:archive/archive.dart';
import 'package:file_testing/file_testing.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/io.dart';

import '../src/common.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;

  setUp(() {
    tempDir = createResolvedTempDirectorySync('reproduce_43161.');
  });

  tearDown(() {
    tryToDelete(tempDir);
  });

  testWithoutContext('flutter build aar packages plugin dependencies in POM', () {
    final Directory helloDir = tempDir.childDirectory('hello');

    // 1. Create a module project.
    ProcessResult result = processManager.runSync(<String>[
      flutterBin,
      'create',
      '--org',
      'io.flutter.devicelab',
      '--template=module',
      'hello',
      ...getLocalEngineArguments(),
    ], workingDirectory: tempDir.path);
    expect(result.exitCode, 0, reason: result.stderr.toString());

    // 2. Create a plugin project.
    result = processManager.runSync(<String>[
      flutterBin,
      'create',
      '--org',
      'io.flutter.devicelab',
      '--template=plugin',
      '--platforms=android',
      'plugin_with_android',
      ...getLocalEngineArguments(),
    ], workingDirectory: tempDir.path);
    expect(result.exitCode, 0, reason: result.stderr.toString());

    // 3. Make module depend on the plugin.
    final File pubspecFile = helloDir.childFile('pubspec.yaml');
    String pubspecContent = pubspecFile.readAsStringSync();
    pubspecContent = pubspecContent.replaceFirst(
      'dependencies:',
      'dependencies:\n  plugin_with_android:\n    path: ../plugin_with_android',
    );
    pubspecFile.writeAsStringSync(pubspecContent, flush: true);

    // Run flutter pub get
    result = processManager.runSync(<String>[
      flutterBin,
      'pub',
      'get',
    ], workingDirectory: helloDir.path);
    expect(result.exitCode, 0, reason: result.stderr.toString());

    // 4. Run flutter build aar.
    result = processManager.runSync(<String>[
      flutterBin,
      'build',
      'aar',
      '--no-profile',
      ...getLocalEngineArguments(),
    ], workingDirectory: helloDir.path);
    expect(result.exitCode, 0, reason: result.stderr.toString());

    // 5. Verify Maven repository contents.
    final Directory repoDir = helloDir.childDirectory('build/host/outputs/repo');
    expect(repoDir, exists);

    // Verify debug and release POM files exist.
    final File debugPom = repoDir
        .childDirectory('io/flutter/devicelab/hello/flutter_debug/1.0')
        .childFile('flutter_debug-1.0.pom');
    final File releasePom = repoDir
        .childDirectory('io/flutter/devicelab/hello/flutter_release/1.0')
        .childFile('flutter_release-1.0.pom');

    expect(debugPom, exists);
    expect(releasePom, exists);

    // 6. Verify POM includes plugin_with_android dependency.
    final String debugPomContent = debugPom.readAsStringSync();
    final String releasePomContent = releasePom.readAsStringSync();

    expect(debugPomContent, contains('plugin_with_android_debug'));
    expect(releasePomContent, contains('plugin_with_android_release'));

    // Check that we also generated the plugin AAR files.
    final File debugPluginAar = repoDir
        .childDirectory('io/flutter/devicelab/plugin_with_android/plugin_with_android_debug/1.0')
        .childFile('plugin_with_android_debug-1.0.aar');
    final File releasePluginAar = repoDir
        .childDirectory('io/flutter/devicelab/plugin_with_android/plugin_with_android_release/1.0')
        .childFile('plugin_with_android_release-1.0.aar');

    expect(debugPluginAar, exists);
    expect(releasePluginAar, exists);

    // 7. Verify class is inside the plugin's AAR but NOT inside the module's own AAR.
    final File debugModuleAar = repoDir
        .childDirectory('io/flutter/devicelab/hello/flutter_debug/1.0')
        .childFile('flutter_debug-1.0.aar');
    expect(debugModuleAar, exists);

    // Verify that the module's AAR does not contain the plugin class.
    final Archive moduleAarArchive = ZipDecoder().decodeBytes(debugModuleAar.readAsBytesSync());
    final ArchiveFile? moduleClassesJarFile = moduleAarArchive.findFile('classes.jar');
    expect(moduleClassesJarFile, isNotNull);
    final Archive moduleClassesJarArchive = ZipDecoder().decodeBytes(
      moduleClassesJarFile!.content as List<int>,
    );
    final bool moduleHasPluginClass = moduleClassesJarArchive.files.any(
      (ArchiveFile file) => file.name.contains('PluginWithAndroidPlugin'),
    );
    expect(moduleHasPluginClass, isFalse);

    // Verify that the plugin's AAR contains the plugin class.
    final Archive pluginAarArchive = ZipDecoder().decodeBytes(debugPluginAar.readAsBytesSync());
    final ArchiveFile? pluginClassesJarFile = pluginAarArchive.findFile('classes.jar');
    expect(pluginClassesJarFile, isNotNull);
    final Archive pluginClassesJarArchive = ZipDecoder().decodeBytes(
      pluginClassesJarFile!.content as List<int>,
    );
    final bool pluginHasPluginClass = pluginClassesJarArchive.files.any(
      (ArchiveFile file) => file.name.contains('PluginWithAndroidPlugin'),
    );
    expect(pluginHasPluginClass, isTrue);
  });
}
