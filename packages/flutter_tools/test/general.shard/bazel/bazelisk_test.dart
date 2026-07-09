// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:file/file.dart';
import 'package:file/memory.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/bazel/bazelisk.dart';
import '../../src/common.dart';
import '../../src/fake_process_manager.dart';

void main() {
  group('Bazelisk', () {
    late MemoryFileSystem fileSystem;
    late BufferLogger logger;
    late FakeProcessManager processManager;

    setUp(() {
      fileSystem = MemoryFileSystem.test();
      logger = BufferLogger.test();
      processManager = FakeProcessManager.empty();
    });

    testWithoutContext('executes build with proper BEP args', () async {
      final Directory workingDirectory = fileSystem.directory('/app')..createSync(recursive: true);

      processManager.addCommand(
        FakeCommand(
          command: <Pattern>[
            'bazelisk',
            'build',
            '//...',
            RegExp(r'--build_event_json_file=.*bep\.json'),
          ],
        ),
      );

      final bazelisk = Bazelisk(
        fileSystem: fileSystem,
        logger: logger,
        processManager: processManager,
      );

      await bazelisk.build(target: '//...', workingDirectory: workingDirectory.path);

      expect(processManager, hasNoRemainingExpectations);
    });

    testWithoutContext('throws tool exit on bazelisk failure', () async {
      final Directory workingDirectory = fileSystem.directory('/app')..createSync(recursive: true);

      processManager.addCommand(
        FakeCommand(
          command: <Pattern>[
            'bazelisk',
            'build',
            '//...',
            RegExp(r'--build_event_json_file=.*bep\.json'),
          ],
          exitCode: 1,
          stderr: 'Build failed',
        ),
      );

      final bazelisk = Bazelisk(
        fileSystem: fileSystem,
        logger: logger,
        processManager: processManager,
      );

      await expectLater(
        () => bazelisk.build(target: '//...', workingDirectory: workingDirectory.path),
        throwsToolExit(message: 'Bazelisk build failed with exit code 1.'),
      );
    });
  });
}
