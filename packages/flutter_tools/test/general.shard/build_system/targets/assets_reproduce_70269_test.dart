// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:file_testing/file_testing.dart';
import 'package:flutter_tools/src/artifacts.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/base/platform.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:flutter_tools/src/build_system/build_system.dart';
import 'package:flutter_tools/src/build_system/depfile.dart';
import 'package:flutter_tools/src/build_system/targets/assets.dart';

import '../../../src/common.dart';
import '../../../src/context.dart';
import '../../../src/package_config.dart';

void main() {
  late Environment environment;
  late FileSystem fileSystem;
  late BufferLogger logger;

  setUp(() {
    fileSystem = MemoryFileSystem.test();
    logger = BufferLogger.test();
    environment = Environment.test(
      fileSystem.currentDirectory,
      processManager: FakeProcessManager.any(),
      artifacts: Artifacts.test(),
      fileSystem: fileSystem,
      logger: logger,
      platform: FakePlatform(),
      defines: <String, String>{kBuildMode: BuildMode.debug.cliName},
    );
    environment.buildDir.childFile('app.dill').createSync(recursive: true);
    environment.buildDir.childFile('native_assets.json').createSync(recursive: true);
    fileSystem
        .file('packages/flutter_tools/lib/src/build_system/targets/assets.dart')
        .createSync(recursive: true);
  });

  testUsingContext(
    'Copies resolving symlinks when asset is directory-specified',
    () async {
      writePackageConfigFiles(directory: fileSystem.currentDirectory, mainLibName: 'example');

      // Create a file outside the assets directory using a platform-independent absolute path.
      final String targetPath = fileSystem.path.absolute('target.txt');
      fileSystem.file(targetPath)
        ..createSync(recursive: true)
        ..writeAsStringSync('target content');

      // Create a directory.
      fileSystem.directory('assets/foo').createSync(recursive: true);

      // Create a symlink in that directory pointing to the target file.
      final Link link = fileSystem.link('assets/foo/link.txt');
      link.createSync(targetPath);

      fileSystem.file('pubspec.yaml')
        ..createSync()
        ..writeAsStringSync('''
name: example

flutter:
  assets:
    - assets/foo/
''');

      await const CopyAssets().build(environment);

      // Use platform-independent child helpers to locate the copied file.
      final File copiedFile = environment.buildDir
          .childDirectory('flutter_assets')
          .childDirectory('assets')
          .childDirectory('foo')
          .childFile('link.txt');

      expect(copiedFile, exists);
      // Crucial: Verify that it was copied as a regular file, not a symlink.
      expect(fileSystem.isLinkSync(copiedFile.path), isFalse);
      expect(copiedFile.readAsStringSync(), equals('target content'));

      final File depfile = environment.buildDir.childFile('flutter_assets.d');
      expect(depfile, exists);
      final Depfile dependencies = environment.depFileService.parse(depfile);

      // The input should be the resolved target file, not the symlink.
      expect(dependencies.inputs.any((File file) => file.path == targetPath), isTrue);
    },
    overrides: <Type, Generator>{
      FileSystem: () => fileSystem,
      ProcessManager: () => FakeProcessManager.any(),
    },
  );
}
