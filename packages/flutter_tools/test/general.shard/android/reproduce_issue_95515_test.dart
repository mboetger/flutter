// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/memory.dart';
import 'package:flutter_tools/src/android/gradle.dart';
import 'package:flutter_tools/src/base/file_system.dart';
import 'package:flutter_tools/src/base/logger.dart';

import '../../src/common.dart';

void main() {
  group('reproduce_issue_95515', () {
    late BufferLogger logger;
    late FileSystem fileSystem;

    setUp(() {
      logger = BufferLogger.test();
      fileSystem = MemoryFileSystem.test();
    });

    testWithoutContext(
      'printHowToConsumeAar instructs user to configure repositories in settings.gradle',
      () {
        printHowToConsumeAar(
          buildModes: const <String>{'debug'},
          androidPackage: 'com.mycompany',
          repoDirectory: fileSystem.directory('build/'),
          logger: logger,
          fileSystem: fileSystem,
        );

        // The printed output should instruct the user to configure settings.gradle,
        // and specifically mention dependencyResolutionManagement and repositories.
        expect(logger.statusText, contains('settings.gradle'));
        expect(logger.statusText, contains('dependencyResolutionManagement'));
      },
    );
  });
}
