// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'package:process/process.dart';

import '../base/file_system.dart';
import '../base/io.dart';
import '../base/logger.dart';
import 'bep.dart';

/// A wrapper class for interacting with the Bazelisk CLI.
///
/// Since we are shipping Bazelisk bundled with the Flutter SDK for internal orchestration,
/// this utility ensures the correct binary path is resolved and handles basic execution.
class Bazelisk {
  Bazelisk({required this.fileSystem, required this.logger, required this.processManager});

  final FileSystem fileSystem;
  final Logger logger;
  final ProcessManager processManager;

  /// Resolves the path to the bundled Bazelisk binary.
  Future<String> _getBazeliskPath() async {
    // TODO(bazel-migration): Define the artifact caching and fetching logic.
    // For now, we assume bazelisk exists in our bundled bin directory.
    return 'bazel';
  }

  /// Executes a 'bazel build' command for a given [target].
  ///
  /// This command will eventually be updated to stream BEP logs explicitly for finer-grained
  /// CLI reporting on stdout/stderr.
  Future<void> build({
    required String target,
    required String workingDirectory,
    List<String> extraArgs = const <String>[],
  }) async {
    final String executable = await _getBazeliskPath();
    final File bepFile = fileSystem.systemTempDirectory
        .createTempSync('bazel_bep_')
        .childFile('bep.json');
    final cmd = <String>[
      executable,
      'build',
      target,
      '--build_event_json_file=${bepFile.path}',
      ...extraArgs,
    ];

    logger.printStatus('Executing: ${cmd.join(' ')}');

    final ProcessResult result = await processManager.run(cmd, workingDirectory: workingDirectory);

    // Consume BEP logs
    final parser = BepParser(logger: logger);
    await parser.parseFile(bepFile);

    try {
      if (bepFile.existsSync()) {
        bepFile.deleteSync();
      }
    } on FileSystemException catch (_) {
      // Ignored
    }

    if (result.exitCode != 0) {
      logger.printError('Bazel build failed with exit code ${result.exitCode}.');
      logger.printError(result.stderr.toString());
      throw Exception('Bazelisk execution failed.');
    } else {
      logger.printStatus(result.stdout.toString());
    }
  }

  /// Queries the Bazel dependency graph.
  Future<String> query({required String queryExpr, required String workingDirectory}) async {
    final String executable = await _getBazeliskPath();
    final cmd = <String>[executable, 'query', queryExpr];

    final ProcessResult result = await processManager.run(cmd, workingDirectory: workingDirectory);

    if (result.exitCode != 0) {
      throw Exception('Bazelisk query failed: ${result.stderr}');
    }
    return result.stdout.toString();
  }
}
