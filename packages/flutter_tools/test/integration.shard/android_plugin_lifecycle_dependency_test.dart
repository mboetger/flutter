// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file_testing/file_testing.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/io.dart';
import 'package:flutter_tools/src/cache.dart';

import '../src/common.dart';
import 'test_utils.dart';

void main() {
  late Directory tempDir;

  setUp(() {
    Cache.flutterRoot = getFlutterRoot();
    tempDir = createResolvedTempDirectorySync('flutter_plugin_lifecycle_test.');
  });

  tearDown(() async {
    tryToDelete(tempDir);
  });

  testWithoutContext(
    'plugin fails to compile if it uses FlutterLifecycleAdapter but does not depend on a plugin providing it',
    () async {
      // 1. Create dummy_lifecycle plugin that provides the class
      processManager.runSync(<String>[
        flutterBin,
        ...getLocalEngineArguments(),
        'create',
        '--template=plugin',
        '--platforms=android',
        '--android-language=java',
        'dummy_lifecycle',
      ], workingDirectory: tempDir.path);

      final Directory dummyLifecycleDir = tempDir.childDirectory('dummy_lifecycle');

      // Create the FlutterLifecycleAdapter.java in the correct package
      final Directory lifecyclePkgDir = dummyLifecycleDir
          .childDirectory('android')
          .childDirectory('src')
          .childDirectory('main')
          .childDirectory('java')
          .childDirectory('io')
          .childDirectory('flutter')
          .childDirectory('embedding')
          .childDirectory('engine')
          .childDirectory('plugins')
          .childDirectory('lifecycle');
      lifecyclePkgDir.createSync(recursive: true);

      final File adapterFile = lifecyclePkgDir.childFile('FlutterLifecycleAdapter.java');
      adapterFile.writeAsStringSync('''
package io.flutter.embedding.engine.plugins.lifecycle;
public class FlutterLifecycleAdapter {
  public static String test() {
    return "success";
  }
}
''');

      // 2. Create test_plugin that uses the class
      processManager.runSync(<String>[
        flutterBin,
        ...getLocalEngineArguments(),
        'create',
        '--template=plugin',
        '--platforms=android',
        '--android-language=java',
        'test_plugin',
      ], workingDirectory: tempDir.path);

      final Directory testPluginDir = tempDir.childDirectory('test_plugin');
      final File testPluginJavaFile = testPluginDir
          .childDirectory('android')
          .childDirectory('src')
          .childDirectory('main')
          .childDirectory('java')
          .childDirectory('com')
          .childDirectory('example')
          .childDirectory('test_plugin')
          .childFile('TestPlugin.java');

      expect(testPluginJavaFile, exists);

      // Modify TestPlugin.java to import and use FlutterLifecycleAdapter
      testPluginJavaFile.writeAsStringSync('''
package com.example.test_plugin;

import androidx.annotation.NonNull;
import io.flutter.embedding.engine.plugins.FlutterPlugin;
import io.flutter.embedding.engine.plugins.lifecycle.FlutterLifecycleAdapter;

public class TestPlugin implements FlutterPlugin {
  @Override
  public void onAttachedToEngine(@NonNull FlutterPluginBinding flutterPluginBinding) {
    // Use the class to force compilation dependency
    String val = FlutterLifecycleAdapter.test();
  }

  @Override
  public void onDetachedFromEngine(@NonNull FlutterPluginBinding flutterPluginBinding) {}
}
''');

      // 3. Try to build the example app of test_plugin.
      // It should FAIL because test_plugin does not depend on dummy_lifecycle.
      final Directory pluginExampleAppDir = testPluginDir.childDirectory('example');

      // We need to run pub get first to ensure dependencies are resolved.
      processManager.runSync(<String>[
        flutterBin,
        ...getLocalEngineArguments(),
        'pub',
        'get',
      ], workingDirectory: pluginExampleAppDir.path);

      ProcessResult result = processManager.runSync(<String>[
        flutterBin,
        ...getLocalEngineArguments(),
        'build',
        'apk',
        '--debug',
        '--target-platform=android-arm64',
      ], workingDirectory: pluginExampleAppDir.path);

      expect(result.exitCode, isNot(0));
      expect(result.stderr, contains('cannot find symbol'));
      expect(
        result.stderr,
        contains('import io.flutter.embedding.engine.plugins.lifecycle.FlutterLifecycleAdapter;'),
      );

      // 4. Add dependency on dummy_lifecycle to test_plugin's pubspec.yaml
      final File testPluginPubspec = testPluginDir.childFile('pubspec.yaml');
      final String pubspecContent = testPluginPubspec.readAsStringSync();

      // Insert dependency
      final String newPubspecContent = pubspecContent.replaceAll(
        'dependencies:',
        'dependencies:\n  dummy_lifecycle:\n    path: ../dummy_lifecycle',
      );
      testPluginPubspec.writeAsStringSync(newPubspecContent);

      // Run pub get again to update resolution
      processManager.runSync(<String>[
        flutterBin,
        ...getLocalEngineArguments(),
        'pub',
        'get',
      ], workingDirectory: pluginExampleAppDir.path);

      // 5. Build again. It should SUCCEED now.
      result = processManager.runSync(<String>[
        flutterBin,
        ...getLocalEngineArguments(),
        'build',
        'apk',
        '--debug',
        '--target-platform=android-arm64',
      ], workingDirectory: pluginExampleAppDir.path);

      expect(result.exitCode, 0);
    },
  );
}
