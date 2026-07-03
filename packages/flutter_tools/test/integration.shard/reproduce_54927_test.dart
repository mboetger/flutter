// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';

import 'package:file/file.dart';
import 'package:flutter_tools/src/artifacts.dart';
import 'package:flutter_tools/src/base/io.dart';
import 'package:flutter_tools/src/globals.dart' as globals;

import '../src/common.dart';
import '../src/context.dart';

void main() {
  testUsingContext(
    'frontend_server does not crash with OOM when initialized with a corrupted dill file',
    () async {
      final Directory tempDir = globals.fs.systemTempDirectory.createTempSync('reproduce_54927_');
      addTearDown(() {
        try {
          tempDir.deleteSync(recursive: true);
        } on FileSystemException catch (_) {
          // Ignore cleanup errors on Windows if files are locked.
        }
      });

      final File corruptedDill = tempDir.childFile('corrupted.dill');

      // Write 20 bytes of 0. This simulates a corrupted/truncated dill file
      // where the last 4 bytes (the size entry in the index) are 0.
      corruptedDill.writeAsBytesSync(List<int>.filled(20, 0));

      final String dartaotruntime = globals.artifacts!.getArtifactPath(
        Artifact.engineDartAotRuntime,
      );
      final String frontendServer = globals.artifacts!.getArtifactPath(
        Artifact.frontendServerSnapshotForEngineDartSdk,
      );
      final String sdkRoot = globals.artifacts!.getArtifactPath(Artifact.flutterPatchedSdkPath);

      final Process process = await globals.processManager.start(<String>[
        dartaotruntime,
        frontendServer,
        '--sdk-root',
        sdkRoot,
        '--incremental',
        '--initialize-from-dill',
        corruptedDill.path,
      ]);
      addTearDown(() {
        process.kill();
      });

      // Close stdin immediately. If it successfully initializes, it should exit.
      await process.stdin.close();

      // Set a timeout. If the bug is present, it will hang (infinite loop) or OOM.
      // If it's fixed, it should exit quickly.
      final exitCodeCompleter = Completer<int>();
      final timer = Timer(const Duration(seconds: 30), () {
        exitCodeCompleter.completeError(
          TimeoutException('frontend_server hung on corrupted dill file (potential infinite loop)'),
        );
        process.kill();
      });

      try {
        final int exitCode = await process.exitCode;
        if (!exitCodeCompleter.isCompleted) {
          exitCodeCompleter.complete(exitCode);
        }
      } catch (e, st) {
        if (!exitCodeCompleter.isCompleted) {
          exitCodeCompleter.completeError(e, st);
        }
      } finally {
        timer.cancel();
      }

      // On a fixed SDK, the process should exit with 0 (since we closed stdin).
      // If the bug is present, it will throw a TimeoutException (handled above)
      // or exit with a non-zero code due to OOM.
      final int exitCode = await exitCodeCompleter.future;
      expect(exitCode, 0);
    },
  );
}
