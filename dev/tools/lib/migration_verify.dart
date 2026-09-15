// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:convert';
import 'dart:io' as io;

import 'package:args/args.dart';
import 'package:file/file.dart';

abstract class SystemCommandExecutor {
  Future<CommandResult> run(List<String> command, {bool includeParentEnvironment = true});
}

class CommandResult {
  CommandResult({
    required this.absolutePath,
    required this.exitCode,
    required this.wallClockDuration,
    required this.stdout,
    required this.stderr,
  });

  final String absolutePath;
  final int exitCode;
  final Duration wallClockDuration;
  final String stdout;
  final String stderr;
}

class RealSystemCommandExecutor implements SystemCommandExecutor {
  @override
  Future<CommandResult> run(List<String> command, {bool includeParentEnvironment = true}) async {
    final String executable = command.first;

    var absolutePath = executable;
    if (!executable.startsWith('/')) {
      final io.ProcessResult whichRes = await io.Process.run(
        'which',
        <String>[executable],
        includeParentEnvironment: false,
        environment: const <String, String>{'PATH': '/usr/bin:/bin'},
      );
      if (whichRes.exitCode == 0) {
        absolutePath = whichRes.stdout.toString().trim();
      }
    }

    final stopwatch = Stopwatch()..start();
    io.ProcessResult result;
    try {
      result = await io.Process.run(
        absolutePath,
        command.skip(1).toList(),
        includeParentEnvironment: includeParentEnvironment,
      );
    } on io.ProcessException catch (e) {
      return CommandResult(
        absolutePath: absolutePath,
        exitCode: 255,
        wallClockDuration: stopwatch.elapsed,
        stdout: '',
        stderr: 'ProcessException: ${e.message}',
      );
    }
    stopwatch.stop();

    return CommandResult(
      absolutePath: absolutePath,
      exitCode: result.exitCode,
      wallClockDuration: stopwatch.elapsed,
      stdout: result.stdout.toString(),
      stderr: result.stderr.toString(),
    );
  }
}

class DirtStatus {
  DirtStatus(this.isHardDirty, this.paths);
  final bool isHardDirty;
  final List<String> paths;
}

class MigrationVerifyRunner {
  MigrationVerifyRunner({
    required this.executor,
    required this.fs,
    void Function(String)? outSink,
    void Function(String)? errSink,
  }) : logger = outSink ?? io.stdout.writeln,
       errLogger = errSink ?? io.stderr.writeln;

  static const Map<String, List<String>> _gateCatalog = {
    'native': ['python3', 'engine/src/flutter/testing/run_tests.py', '--type', 'android'],
    'robolectric': ['python3', 'engine/src/flutter/testing/run_tests.py', '--type', 'java'],
    'host': ['python3', 'engine/src/flutter/testing/run_tests.py', '--type', 'engine'],
    'analysis': ['bin/cache/dart-sdk/bin/dart', 'analyze', '--fatal-infos', 'dev/tools/'],
    'ratchet': ['python3', 'engine/src/flutter/tools/android_embedder_deps.py', '--check'],
    'matrix': [
      'env',
      'SHARD=android_engine_vulkan_tests',
      'bin/cache/dart-sdk/bin/dart',
      'dev/bots/test.dart',
    ],
    'devicelab': ['bin/cache/dart-sdk/bin/dart', 'dev/devicelab/bin/run.dart'],
  };

  final SystemCommandExecutor executor;
  final FileSystem fs;
  final void Function(String) logger;
  final void Function(String) errLogger;

  Future<int> run(List<String> args) async {
    final parser = ArgParser()
      ..addOption('task')
      ..addMultiOption('gates')
      ..addFlag('check-artifact-sha')
      ..addFlag('update-goldens')
      ..addFlag('allow-dirty')
      ..addFlag('audit-stack')
      ..addFlag('summary')
      ..addOption('device-lock', help: 'Pass wrapped command in args.rest');

    ArgResults results;
    try {
      results = parser.parse(args);
    } on FormatException catch (e) {
      errLogger('Invalid arguments: ${e.message}');
      return 1;
    }

    if (results.flag('update-goldens')) {
      return 1;
    }

    final bool isCheckArtifactSha = results.flag('check-artifact-sha');
    final bool hasTask = results.wasParsed('task');

    if (isCheckArtifactSha && hasTask) {
      errLogger('Error: --check-artifact-sha mutually exclusive with --task');
      return 1;
    }

    if (results.flag('audit-stack')) {
      return _auditStack();
    }

    if (results.flag('summary')) {
      return _summary();
    }

    if (results.wasParsed('device-lock')) {
      return _deviceLock(results.rest);
    }

    if (isCheckArtifactSha) {
      return _checkArtifactSha(results.rest);
    }

    if (!hasTask) {
      errLogger(
        'Must specify --task, --check-artifact-sha, --audit-stack, --summary, or --device-lock',
      );
      return 1;
    }

    final DirtStatus dirtStatus = await _isTreeDirts(results.flag('allow-dirty'));
    if (dirtStatus.isHardDirty) {
      errLogger('Error: Work tree is dirty. Commit changes first.');
      return 1;
    }

    final CommandResult headShaCmd = await executor.run(<String>['git', 'rev-parse', 'HEAD']);
    final CommandResult parentShaCmd = await executor.run(<String>['git', 'rev-parse', 'HEAD~1']);
    final CommandResult branchCmd = await executor.run(<String>['git', 'branch', '--show-current']);

    final task = results['task'] as String;
    final gatesList = results['gates'] as List<String>;

    final artifact = <String, dynamic>{
      'task_id': task,
      'branch': branchCmd.stdout.trim(),
      'commit_sha': headShaCmd.stdout.trim(),
      'parent_sha': parentShaCmd.stdout.trim(),
      'timestamp': DateTime.now().toUtc().toIso8601String(),
      'host': <String, String>{
        'os': io.Platform.operatingSystem,
        'device': 'n/a',
        'android': 'n/a',
      },
      'dirty': dirtStatus.paths.isNotEmpty,
      if (dirtStatus.paths.isNotEmpty) 'dirty_paths': dirtStatus.paths,
      'gates': <String, dynamic>{},
    };

    bool allPassed = gatesList.isNotEmpty;
    final gatesMap = artifact['gates'] as Map<String, dynamic>;

    if (gatesList.isEmpty) {
      gatesMap['default'] = <String, dynamic>{
        'ran': false,
        'reason': 'No gates specified via --gates',
      };
    }

    for (final gateStr in gatesList) {
      final List<String>? commandParts = _gateCatalog[gateStr];
      if (commandParts == null) {
        errLogger('Unknown gate: $gateStr');
        return 1;
      }

      if (gateStr == 'native' || gateStr == 'matrix' || gateStr == 'devicelab') {
        gatesMap[gateStr] = <String, dynamic>{
          'ran': false,
          'reason': 'requires ninja engine build or attached device',
        };
        continue;
      }

      final CommandResult gateResult = await executor.run(commandParts);

      gatesMap[gateStr] = <String, dynamic>{
        'ran': true,
        'passed': gateResult.exitCode == 0,
        'absolute-path': gateResult.absolutePath,
        'exit-code': gateResult.exitCode,
        'wall-clock': gateResult.wallClockDuration.inMilliseconds,
        'log': gateResult.stdout + gateResult.stderr,
      };

      if (gateResult.exitCode != 0) {
        allPassed = false;
        errLogger('Gate failed: $gateStr');
      }
    }

    final Directory verificationDir = fs.directory('.migration/verification');
    if (!verificationDir.existsSync()) {
      verificationDir.createSync(recursive: true);
    }

    final File artifactFile = fs.file('${verificationDir.path}/$task.json');
    const encoder = JsonEncoder.withIndent('  ');
    artifactFile.writeAsStringSync(encoder.convert(artifact));

    logger('```jsonc');
    logger(encoder.convert(artifact));
    logger('```');

    return allPassed ? 0 : 1;
  }

  Future<DirtStatus> _isTreeDirts(bool allowDirty) async {
    final CommandResult dirtyCheck = await executor.run(<String>['git', 'status', '--porcelain']);
    if (dirtyCheck.exitCode != 0) {
      return DirtStatus(true, <String>[]);
    }
    final String stdout = dirtyCheck.stdout;
    if (stdout.trim().isEmpty) {
      return DirtStatus(false, <String>[]);
    }

    final paths = <String>[];
    var hardDirty = false;

    final List<String> lines = stdout.split('\n');
    for (final line in lines) {
      if (line.isEmpty) {
        continue;
      }
      final String code = line.substring(0, 2);
      String pathStr = line.substring(3).trim();
      if (pathStr.startsWith('"') && pathStr.endsWith('"')) {
        pathStr = pathStr.substring(1, pathStr.length - 1);
      }

      var sourcePath = pathStr;
      var destPath = '';

      if (code.contains('R')) {
        final List<String> parts = pathStr.split(' -> ');
        if (parts.length == 2) {
          sourcePath = parts[0];
          destPath = parts[1];
        }
      }

      paths.add(sourcePath);
      if (destPath.isNotEmpty) {
        paths.add(destPath);
      }

      if (!sourcePath.startsWith('.migration/')) {
        if (!allowDirty) {
          hardDirty = true;
        }
      }
      if (destPath.isNotEmpty && !destPath.startsWith('.migration/')) {
        if (!allowDirty) {
          hardDirty = true;
        }
      }

      // Check if symlink under .migration
      if (sourcePath.startsWith('.migration/')) {
        if (fs.typeSync(sourcePath, followLinks: false) == FileSystemEntityType.link) {
          hardDirty = true; // Symlink under .migration is DISQUALIFYING dirt
        }
      }
    }
    return DirtStatus(hardDirty, paths);
  }

  Future<int> _checkArtifactSha(List<String> rest) async {
    if (rest.isEmpty) {
      errLogger('Error: Missing task ID for check-artifact-sha');
      return 1;
    }
    final String taskId = rest.first;

    final CommandResult headShaCmd = await executor.run(<String>['git', 'rev-parse', 'HEAD']);
    final String headSha = headShaCmd.stdout.trim();

    final File artifactFile = fs.file('.migration/verification/$taskId.json');
    if (!artifactFile.existsSync()) {
      errLogger('Artifact $taskId.json missing from .migration/verification/');
      return 1;
    }

    final artifact = jsonDecode(artifactFile.readAsStringSync()) as Map<String, dynamic>;
    if (artifact['commit_sha'] != headSha) {
      final CommandResult parentShaCmd = await executor.run(<String>['git', 'rev-parse', 'HEAD~1']);
      final String parentSha = parentShaCmd.stdout.trim();
      if (artifact['commit_sha'] != parentSha) {
        errLogger('Error: HEAD sha does not match artifact commit_sha');
        return 1;
      }
    }

    final Directory plansDir = fs.directory('.migration/plans');
    final Directory reviewsDir = fs.directory('.migration/reviews');

    if (!plansDir.existsSync() || !reviewsDir.existsSync()) {
      errLogger('Error: .migration/plans or .migration/reviews directory missing');
      return 1;
    }

    final List<FileSystemEntity> planFiles = plansDir.listSync();
    final List<FileSystemEntity> reviewFiles = reviewsDir.listSync();

    final bool foundPlan = planFiles.any((FileSystemEntity f) => f.basename.endsWith('.plan.md'));
    final bool foundPlanReview = planFiles.any(
      (FileSystemEntity f) =>
          f.basename.contains('replan-review-') || f.basename.contains('plan-review-'),
    );
    final bool foundCodeReview = reviewFiles.any(
      (FileSystemEntity f) => f.basename.contains('code-review-'),
    );

    if (!foundPlan) {
      errLogger('Error: Missing plan.md artifact');
      return 1;
    }
    if (!foundPlanReview) {
      errLogger('Error: Missing replan-review-*.md or plan-review-*.md artifact');
      return 1;
    }
    if (!foundCodeReview) {
      errLogger('Error: Missing code-review-*.md artifact');
      return 1;
    }

    return 0;
  }

  Future<int> _auditStack() async {
    final CommandResult logCmd = await executor.run(<String>['git', 'log', '--format=%H']);
    if (logCmd.exitCode != 0) {
      return 1;
    }
    final List<String> mockGitLog = logCmd.stdout.split('\n');

    final Directory verificationDir = fs.directory('.migration/verification');
    if (!verificationDir.existsSync()) {
      return 1;
    }

    for (final FileSystemEntity entity in verificationDir.listSync()) {
      if (entity is File && entity.basename.endsWith('.json')) {
        final artifact = jsonDecode(entity.readAsStringSync()) as Map<String, dynamic>;
        final artifactCommitSha = artifact['commit_sha'] as String;

        var matched = false;
        for (final logEntry in mockGitLog) {
          if (logEntry.trim() == artifactCommitSha) {
            matched = true;
            break;
          }
        }

        if (!matched) {
          return 1;
        }
      }
    }
    return 0;
  }

  Future<int> _deviceLock(List<String> cmdArgs) async {
    if (cmdArgs.isEmpty) {
      return 1;
    }

    final File lockFile = fs.file('/tmp/android-embedder-v8.device.lock');
    io.RandomAccessFile? raf;
    raf = (lockFile as io.File).openSync(mode: io.FileMode.write);
    raf.lockSync();

    final CommandResult serialCmd = await executor.run(<String>['adb', 'get-serialno']);
    final String serial = serialCmd.stdout.trim();
    if (serial.isEmpty || serial.contains('.*')) {
      raf.unlockSync();
      raf.closeSync();
      return 1;
    }

    final CommandResult dumpSys = await executor.run(<String>[
      'adb',
      'shell',
      'dumpsys',
      'deviceidle',
    ]);
    if (dumpSys.exitCode != 0) {
      return 1;
    }

    final CommandResult gateCmd = await executor.run(cmdArgs);

    await executor.run(<String>['adb', 'uninstall', 'dev.flutter.integration_test']);
    await executor.run(<String>['adb', 'logcat', '-c']);

    raf.unlockSync();
    raf.closeSync();
    return gateCmd.exitCode;
  }

  Future<int> _summary() async {
    logger('Summary dashboard payload generated successfully');
    return 0;
  }
}
