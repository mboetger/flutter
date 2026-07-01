// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:io' as io;

import 'package:file/memory.dart';
import 'package:flutter_tools/src/artifacts.dart';
import 'package:flutter_tools/src/base/build.dart';
import 'package:flutter_tools/src/base/common.dart';
import 'package:flutter_tools/src/base/logger.dart';
import 'package:flutter_tools/src/build_info.dart';
import 'package:flutter_tools/src/macos/xcode.dart';
import 'package:platform/platform.dart';

import '../../src/common.dart';
import '../../src/fake_process_manager.dart';

void main() {
  group('AOTSnapshotter gen_snapshot execution failure reproduction', () {
    late MemoryFileSystem fileSystem;
    late Artifacts artifacts;
    late FakeProcessManager processManager;
    late BufferLogger logger;

    setUp(() {
      fileSystem = MemoryFileSystem.test();
      artifacts = Artifacts.test();
      processManager = FakeProcessManager.empty();
      logger = BufferLogger.test();
    });

    testWithoutContext(
      'throws ToolExit with 32-bit warning on Linux when gen_snapshot exists but fails with errorCode 2',
      () async {
        final snapshotter = AOTSnapshotter(
          fileSystem: fileSystem,
          logger: logger,
          xcode: Xcode.test(processManager: processManager),
          artifacts: artifacts,
          processManager: processManager,
          platform: FakePlatform(operatingSystem: 'linux'),
        );
        final String outputPath = fileSystem.path.join('build', 'out');
        final String genSnapshotPath = artifacts.getArtifactPath(
          Artifact.genSnapshot,
          platform: TargetPlatform.android_arm,
          mode: BuildMode.profile,
        );

        // Create the gen_snapshot file so it exists.
        fileSystem.file(genSnapshotPath).createSync(recursive: true);

        processManager.addCommand(
          FakeCommand(
            command: <String>[
              genSnapshotPath,
              '--deterministic',
              '--snapshot_kind=app-aot-elf',
              '--elf=build/out/app.so',
              '--no-sim-use-hardfp',
              '--no-use-integer-division',
              'main.dill',
            ],
            exception: const io.ProcessException(
              'android-arm-profile/linux-x64/gen_snapshot',
              <String>[],
              'No such file or directory',
              2,
            ),
          ),
        );

        await expectLater(
          () => snapshotter.build(
            platform: TargetPlatform.android_arm,
            buildMode: BuildMode.profile,
            mainPath: 'main.dill',
            outputPath: outputPath,
            dartObfuscation: false,
          ),
          throwsToolExit(message: RegExp('32-bit')),
        );

        expect(processManager, hasNoRemainingExpectations);
      },
    );

    testWithoutContext(
      'throws ToolExit without 32-bit warning on Windows when gen_snapshot exists and fails with errorCode 2',
      () async {
        final snapshotter = AOTSnapshotter(
          fileSystem: fileSystem,
          logger: logger,
          xcode: Xcode.test(processManager: processManager),
          artifacts: artifacts,
          processManager: processManager,
          platform: FakePlatform(operatingSystem: 'windows'),
        );
        final String outputPath = fileSystem.path.join('build', 'out');
        final String genSnapshotPath = artifacts.getArtifactPath(
          Artifact.genSnapshot,
          platform: TargetPlatform.android_arm,
          mode: BuildMode.profile,
        );

        // Create the gen_snapshot file so it exists.
        fileSystem.file(genSnapshotPath).createSync(recursive: true);

        processManager.addCommand(
          FakeCommand(
            command: <String>[
              genSnapshotPath,
              '--deterministic',
              '--snapshot_kind=app-aot-elf',
              '--elf=build/out/app.so',
              '--no-sim-use-hardfp',
              '--no-use-integer-division',
              'main.dill',
            ],
            exception: const io.ProcessException(
              'android-arm-profile/linux-x64/gen_snapshot',
              <String>[],
              'No such file or directory',
              2,
            ),
          ),
        );

        // Expect ToolExit, but it should NOT contain "32-bit" warning.
        Future<void> action() => snapshotter.build(
          platform: TargetPlatform.android_arm,
          buildMode: BuildMode.profile,
          mainPath: 'main.dill',
          outputPath: outputPath,
          dartObfuscation: false,
        );

        try {
          await action();
          fail('Should have thrown ToolExit');
        } on ToolExit catch (e) {
          expect(e.message, isNot(contains('32-bit')));
        }

        expect(processManager, hasNoRemainingExpectations);
      },
    );

    testWithoutContext(
      'throws ToolExit without 32-bit warning on Linux when gen_snapshot exists but fails with errorCode 13 (Permission Denied)',
      () async {
        final snapshotter = AOTSnapshotter(
          fileSystem: fileSystem,
          logger: logger,
          xcode: Xcode.test(processManager: processManager),
          artifacts: artifacts,
          processManager: processManager,
          platform: FakePlatform(operatingSystem: 'linux'),
        );
        final String outputPath = fileSystem.path.join('build', 'out');
        final String genSnapshotPath = artifacts.getArtifactPath(
          Artifact.genSnapshot,
          platform: TargetPlatform.android_arm,
          mode: BuildMode.profile,
        );

        // Create the gen_snapshot file so it exists.
        fileSystem.file(genSnapshotPath).createSync(recursive: true);

        processManager.addCommand(
          FakeCommand(
            command: <String>[
              genSnapshotPath,
              '--deterministic',
              '--snapshot_kind=app-aot-elf',
              '--elf=build/out/app.so',
              '--no-sim-use-hardfp',
              '--no-use-integer-division',
              'main.dill',
            ],
            exception: const io.ProcessException(
              'android-arm-profile/linux-x64/gen_snapshot',
              <String>[],
              'Permission denied',
              13,
            ),
          ),
        );

        Future<void> action() => snapshotter.build(
          platform: TargetPlatform.android_arm,
          buildMode: BuildMode.profile,
          mainPath: 'main.dill',
          outputPath: outputPath,
          dartObfuscation: false,
        );

        try {
          await action();
          fail('Should have thrown ToolExit');
        } on ToolExit catch (e) {
          expect(e.message, isNot(contains('32-bit')));
        }

        expect(processManager, hasNoRemainingExpectations);
      },
    );

    testWithoutContext(
      'throws ToolExit without 32-bit warning on Linux when gen_snapshot does not exist',
      () async {
        final snapshotter = AOTSnapshotter(
          fileSystem: fileSystem,
          logger: logger,
          xcode: Xcode.test(processManager: processManager),
          artifacts: artifacts,
          processManager: processManager,
          platform: FakePlatform(operatingSystem: 'linux'),
        );
        final String outputPath = fileSystem.path.join('build', 'out');
        final String genSnapshotPath = artifacts.getArtifactPath(
          Artifact.genSnapshot,
          platform: TargetPlatform.android_arm,
          mode: BuildMode.profile,
        );

        // Do NOT create the gen_snapshot file.

        processManager.addCommand(
          FakeCommand(
            command: <String>[
              genSnapshotPath,
              '--deterministic',
              '--snapshot_kind=app-aot-elf',
              '--elf=build/out/app.so',
              '--no-sim-use-hardfp',
              '--no-use-integer-division',
              'main.dill',
            ],
            exception: const io.ProcessException(
              'android-arm-profile/linux-x64/gen_snapshot',
              <String>[],
              'No such file or directory',
              2,
            ),
          ),
        );

        Future<void> action() => snapshotter.build(
          platform: TargetPlatform.android_arm,
          buildMode: BuildMode.profile,
          mainPath: 'main.dill',
          outputPath: outputPath,
          dartObfuscation: false,
        );

        try {
          await action();
          fail('Should have thrown ToolExit');
        } on ToolExit catch (e) {
          expect(e.message, isNot(contains('32-bit')));
        }

        expect(processManager, hasNoRemainingExpectations);
      },
    );
  });
}
