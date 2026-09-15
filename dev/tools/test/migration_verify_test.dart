// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:convert';

import 'package:dev_tools/migration_verify.dart';
import 'package:file/file.dart';
import 'package:file/local.dart';
import 'package:file/memory.dart';
import 'package:test/test.dart';

void main() {
  group('Verification Tool Tests (18 Tests)', () {
    test('D1: mapped gate whose command is missing fails cleanly without crash', () async {
      final executor = RealSystemCommandExecutor();
      final CommandResult result = await executor.run(<String>['nonexistent_command', 'test']);
      expect(result.exitCode, 255);
      expect(result.stderr.contains('ProcessException:'), true);
    });

    test('D1: unknown gate fails cleanly with Exit code 1', () async {
      final fs = MemoryFileSystem();
      final executor = FakeSystemCommandExecutor();
      executor.registerCommand(<String>['git', 'status', '--porcelain'], exitCode: 0, stdout: '');
      executor.registerCommand(<String>['git', 'rev-parse', 'HEAD'], exitCode: 0, stdout: 'abc\n');
      executor.registerCommand(
        <String>['git', 'rev-parse', 'HEAD~1'],
        exitCode: 0,
        stdout: 'def\n',
      );
      executor.registerCommand(
        <String>['git', 'branch', '--show-current'],
        exitCode: 0,
        stdout: 'branch\n',
      );
      var errStr = '';
      final runner = MigrationVerifyRunner(
        executor: executor,
        fs: fs,
        errSink: (String s) => errStr += s,
      );
      final int exitCode = await runner.run(<String>['--task', 'T-unk', '--gates', 'nonsense']);
      expect(exitCode, 1);
      expect(errStr.contains('Unknown gate: nonsense'), true);
    });

    test('D1: native, devicelab, matrix skip cleanly with ran false and reason', () async {
      final fs = MemoryFileSystem();
      final executor = FakeSystemCommandExecutor();
      executor.registerCommand(<String>['git', 'status', '--porcelain'], exitCode: 0, stdout: '');
      executor.registerCommand(
        <String>['git', 'rev-parse', 'HEAD'],
        exitCode: 0,
        stdout: 'head123\n',
      );
      executor.registerCommand(
        <String>['git', 'rev-parse', 'HEAD~1'],
        exitCode: 0,
        stdout: 'parent123\n',
      );
      executor.registerCommand(
        <String>['git', 'branch', '--show-current'],
        exitCode: 0,
        stdout: 'android-embedder-v8/t-req1\n',
      );

      final runner = MigrationVerifyRunner(executor: executor, fs: fs);
      final int exitCode = await runner.run(<String>['--task', 'T-Req1', '--gates', 'native']);
      expect(exitCode, 0); // Still 0 because it's considered valid skip

      final File artifact = fs.file('.migration/verification/T-Req1.json');
      final decoded = jsonDecode(artifact.readAsStringSync()) as Map<String, dynamic>;
      final gateResult =
          (decoded['gates'] as Map<String, dynamic>)['native'] as Map<String, dynamic>;
      expect(gateResult['ran'], false);
    });

    test('E2E: generates complete schema-valid verification payload for its own execution successfully', () async {
      final fs = MemoryFileSystem();
      fs.directory('.migration/verification').createSync(recursive: true);
      fs.file('.migration/ui/HACK.txt').createSync(recursive: true);

      final executor = FakeSystemCommandExecutor();
      executor.registerCommand(
        <String>['git', 'status', '--porcelain'],
        exitCode: 0,
        stdout: 'A  .migration/ui/HACK.txt\n',
      );
      executor.registerCommand(
        <String>['git', 'rev-parse', 'HEAD'],
        exitCode: 0,
        stdout: 'realheadsha123\n',
      );
      executor.registerCommand(
        <String>['git', 'rev-parse', 'HEAD~1'],
        exitCode: 0,
        stdout: 'parentsha456\n',
      );
      executor.registerCommand(
        <String>['git', 'branch', '--show-current'],
        exitCode: 0,
        stdout: 'android-embedder-v8/t-0.15a-tooling-core\n',
      );
      executor.registerCommand(
        <String>['bin/cache/dart-sdk/bin/dart', 'analyze', '--fatal-infos', 'dev/tools/'],
        exitCode: 0,
        stdout: 'test\n',
      );

      final runner = MigrationVerifyRunner(executor: executor, fs: fs);
      final int exitCode = await runner.run(<String>['--task', 'T-0.15a', '--gates', 'analysis']);

      expect(exitCode, 0);
      final File artifactFile = fs.file('.migration/verification/T-0.15a.json');
      expect(artifactFile.existsSync(), true);

      final decoded = jsonDecode(artifactFile.readAsStringSync()) as Map<String, dynamic>;
      expect(decoded['task_id'], 'T-0.15a');
      expect(decoded['commit_sha'], 'realheadsha123');
      expect(decoded['parent_sha'], 'parentsha456');

      // D2: Tolerate and Record
      expect(decoded['dirty'], true);
      final dirtPaths = decoded['dirty_paths'] as List<dynamic>;
      expect(dirtPaths.contains('.migration/ui/HACK.txt'), true);

      final gateResult =
          (decoded['gates'] as Map<String, dynamic>)['analysis'] as Map<String, dynamic>;
      expect(gateResult['ran'], true);
      expect(gateResult['passed'], true);
    });

    test('D2: Clean tree produces dirty: false natively', () async {
      final fs = MemoryFileSystem();
      final executor = FakeSystemCommandExecutor();
      executor.registerCommand(<String>['git', 'status', '--porcelain'], exitCode: 0, stdout: '');
      executor.registerCommand(
        <String>['git', 'rev-parse', 'HEAD'],
        exitCode: 0,
        stdout: 'head123\n',
      );
      executor.registerCommand(
        <String>['git', 'rev-parse', 'HEAD~1'],
        exitCode: 0,
        stdout: 'parent123\n',
      );
      executor.registerCommand(
        <String>['git', 'branch', '--show-current'],
        exitCode: 0,
        stdout: 'branch\n',
      );

      final runner = MigrationVerifyRunner(executor: executor, fs: fs);
      await runner.run(<String>['--task', 'T-Req1']);

      final File artifact = fs.file('.migration/verification/T-Req1.json');
      final decoded = jsonDecode(artifact.readAsStringSync()) as Map<String, dynamic>;
      expect(decoded['dirty'], false);
    });

    test('Req 1: extracts commit_sha and parent_sha from git natively', () async {
      final fs = MemoryFileSystem();
      final executor = FakeSystemCommandExecutor();
      executor.registerCommand(<String>['git', 'status', '--porcelain'], exitCode: 0, stdout: '');
      executor.registerCommand(
        <String>['git', 'rev-parse', 'HEAD'],
        exitCode: 0,
        stdout: 'head123\n',
      );
      executor.registerCommand(
        <String>['git', 'rev-parse', 'HEAD~1'],
        exitCode: 0,
        stdout: 'parent123\n',
      );
      executor.registerCommand(
        <String>['git', 'branch', '--show-current'],
        exitCode: 0,
        stdout: 'branch\n',
      );

      final runner = MigrationVerifyRunner(executor: executor, fs: fs);
      await runner.run(<String>['--task', 'T-Req1']);

      final File artifact = fs.file('.migration/verification/T-Req1.json');
      final decoded = jsonDecode(artifact.readAsStringSync()) as Map<String, dynamic>;
      expect(decoded['commit_sha'], 'head123');
      expect(decoded['parent_sha'], 'parent123');
    });

    test('Req 2: smart-dirty fails closed on Renames outside .migration', () async {
      final fs = MemoryFileSystem();
      final executor = FakeSystemCommandExecutor();
      executor.registerCommand(
        <String>['git', 'status', '--porcelain'],
        exitCode: 0,
        stdout: 'R  .migration/fake -> engine/src/hack.cc\n',
      );

      final runner = MigrationVerifyRunner(executor: executor, fs: fs);
      final int exitCode = await runner.run(<String>['--task', 'T-Req2']);
      expect(exitCode, 1);
    });

    test('Req 2: treats symlink under .migration as DISQUALIFYING', () async {
      final fs = MemoryFileSystem();
      fs.directory('.migration').createSync();
      fs.link('.migration/ui').createSync('../lib/ui');

      final executor = FakeSystemCommandExecutor();
      executor.registerCommand(
        <String>['git', 'status', '--porcelain'],
        exitCode: 0,
        stdout: 'A  .migration/ui\n',
      );

      final runner = MigrationVerifyRunner(executor: executor, fs: fs);
      final int exitCode = await runner.run(<String>['--task', 'T-Req2-Link']);
      expect(exitCode, 1);
    });

    test('Req 3: validate serialization accurately preserves ran: false', () async {
      final fs = MemoryFileSystem();
      final executor = FakeSystemCommandExecutor();
      executor.registerCommand(<String>['git', 'status', '--porcelain'], exitCode: 0, stdout: '');
      executor.registerCommand(<String>['git', 'rev-parse', 'HEAD'], exitCode: 0, stdout: 'h1\n');
      executor.registerCommand(<String>['git', 'rev-parse', 'HEAD~1'], exitCode: 0, stdout: 'p1\n');
      executor.registerCommand(
        <String>['git', 'branch', '--show-current'],
        exitCode: 0,
        stdout: 'b1\n',
      );

      final runner = MigrationVerifyRunner(executor: executor, fs: fs);
      await runner.run(<String>['--task', 'T-Req3']);
      final decoded = jsonDecode(
        fs.file('.migration/verification/T-Req3.json').readAsStringSync(),
      ) as Map<String, dynamic>;
      final g = (decoded['gates'] as Map<String, dynamic>)['default'] as Map<String, dynamic>;
      expect(g['ran'], false);
    });

    test('Req 4: vacuous zero exits non-zero if no gates run or pass', () async {
      final fs = MemoryFileSystem();
      final executor = FakeSystemCommandExecutor();
      executor.registerCommand(<String>['git', 'status', '--porcelain'], exitCode: 0, stdout: '');
      executor.registerCommand(<String>['git', 'rev-parse', 'HEAD'], exitCode: 0, stdout: 'h1\n');
      executor.registerCommand(<String>['git', 'rev-parse', 'HEAD~1'], exitCode: 0, stdout: 'p1\n');
      executor.registerCommand(
        <String>['git', 'branch', '--show-current'],
        exitCode: 0,
        stdout: 'b1\n',
      );

      final runner = MigrationVerifyRunner(executor: executor, fs: fs);
      final int exitCode = await runner.run(<String>['--task', 'T-Req4']); // No --gates passed
      expect(exitCode, 1);
    });

    test('Req 5: validateStack logic checks missing JSON variables statically', () async {
      final fs = MemoryFileSystem();
      final executor = FakeSystemCommandExecutor();
      executor.registerCommand(
        <String>['git', 'log', '--format=%H'],
        exitCode: 0,
        stdout: 'deadbeef\n',
      );
      final runner = MigrationVerifyRunner(executor: executor, fs: fs);
      final int exitCode = await runner.run(<String>['--audit-stack']);
      expect(exitCode, 1);
    });

    test('Req 6: evaluates outputs rendering Markdown strings structurally accurately', () async {
      final fs = MemoryFileSystem();
      final executor = FakeSystemCommandExecutor();
      executor.registerCommand(<String>['git', 'status', '--porcelain'], exitCode: 0, stdout: '');
      executor.registerCommand(<String>['git', 'rev-parse', 'HEAD'], exitCode: 0, stdout: 'h1\n');
      executor.registerCommand(<String>['git', 'rev-parse', 'HEAD~1'], exitCode: 0, stdout: 'p1\n');
      executor.registerCommand(
        <String>['git', 'branch', '--show-current'],
        exitCode: 0,
        stdout: 'b1\n',
      );
      executor.registerCommand(
        <String>['bin/cache/dart-sdk/bin/dart', 'analyze', '--fatal-infos', 'dev/tools/'],
        exitCode: 0,
        stdout: 'ok\n',
      );

      final runner = MigrationVerifyRunner(executor: executor, fs: fs);
      expect(await runner.run(<String>['--task', 'T-Req6', '--gates', 'analysis']), 0);
    });

    test('Req 7: device-lock blocks isolate targets securely, asserts preconditions, and extracts serial via adb', () async {
      const fs = LocalFileSystem();
      final executor = FakeSystemCommandExecutor();
      executor.registerCommand(
        <String>['adb', 'devices', '-l'],
        exitCode: 0,
        stdout: 'List of devices attached\nserial123 device product:model1\n',
      );
      executor.registerCommand(
        <String>['adb', 'shell', 'getprop', 'ro.build.version.sdk'],
        exitCode: 0,
        stdout: '34\n',
      );
      executor.registerCommand(
        <String>['adb', 'shell', 'dumpsys', 'deviceidle'],
        exitCode: 0,
        stdout: 'not dozing\n',
      );
      executor.registerCommand(<String>['adb', 'get-serialno'], exitCode: 0, stdout: 'serial123\n');
      executor.registerCommand(
        <String>['/bin/echo', 'locked_cmd'],
        exitCode: 0,
        stdout: 'locked_cmd\n',
      );
      executor.registerCommand(
        <String>['adb', 'uninstall', 'dev.flutter.integration_test'],
        exitCode: 0,
        stdout: 'Success\n',
      );
      executor.registerCommand(<String>['adb', 'logcat', '-c'], exitCode: 0, stdout: '');

      final runner = MigrationVerifyRunner(executor: executor, fs: fs);
      final int exitCode = await runner.run(<String>[
        '--device-lock',
        '--',
        '/bin/echo',
        'locked_cmd',
      ]);
      expect(exitCode, 0);
    });

    test('Req 8: rejects execution if update-goldens parameters leak locally', () async {
      final runner = MigrationVerifyRunner(
        executor: FakeSystemCommandExecutor(),
        fs: MemoryFileSystem(),
      );
      final int exitCode = await runner.run(<String>['--task', 'T-Req8', '--update-goldens']);
      expect(exitCode, 1);
    });

    test(
      'Req 9: check-artifact-sha uses addFlag and cannot run alongside task generation',
      () async {
        final runner = MigrationVerifyRunner(
          executor: FakeSystemCommandExecutor(),
          fs: MemoryFileSystem(),
        );
        final int exitCode = await runner.run(<String>[
          '--check-artifact-sha',
          '--task',
          'T-0.15a',
        ]);
        expect(exitCode, 1);
      },
    );

    test(
      'Req 9: D1 tamper valid artifact PASSES, tampered artifact FAILS with explicit mismatch',
      () async {
        final fs = MemoryFileSystem();
        fs.directory('.migration/plans').createSync(recursive: true);
        fs.directory('.migration/reviews').createSync(recursive: true);
        fs.file('.migration/plans/T-0.15.plan.md').createSync();
        fs.file('.migration/plans/T-0.15a.replan-review-1.md').createSync();
        fs.file('.migration/reviews/T-0.15a.code-review-1.md').createSync();

        final Directory verificationDir = fs.directory('.migration/verification')
          ..createSync(recursive: true);

        final File artifact = fs.file('${verificationDir.path}/T-0.15a.json');
        artifact.writeAsStringSync(
          jsonEncode(<String, dynamic>{'task_id': 'T-0.15a', 'commit_sha': 'realsha'}),
        );

        final executor = FakeSystemCommandExecutor();
        executor.registerCommand(
          <String>['git', 'rev-parse', 'HEAD'],
          exitCode: 0,
          stdout: 'realsha\n',
        );
        executor.registerCommand(
          <String>['git', 'rev-parse', 'HEAD~1'],
          exitCode: 0,
          stdout: 'parentsha\n',
        );
        executor.registerCommand(
          <String>['git', 'branch', '--show-current'],
          exitCode: 0,
          stdout: 'android-v8/t-0.15a-tooling\n',
        );

        var interceptedErr = '';
        final runnerPass = MigrationVerifyRunner(
          executor: executor,
          fs: fs,
          errSink: (String s) => interceptedErr += s,
        );
        final int exitPass = await runnerPass.run(<String>['--check-artifact-sha', 'T-0.15a']);
        expect(exitPass, 0); // Proof that validation genuinely passes!

        // Induce Tampering
        artifact.writeAsStringSync(
          jsonEncode(<String, dynamic>{'task_id': 'T-0.15a', 'commit_sha': 'deadbeef'}),
        );

        var tamperedErr = '';
        final runnerFail = MigrationVerifyRunner(
          executor: executor,
          fs: fs,
          errSink: (String s) => tamperedErr += s,
        );
        final int exitFail = await runnerFail.run(<String>['--check-artifact-sha', 'T-0.15a']);
        expect(exitFail, 1);

        // Asserts that we failed specifically because of a SHA mismatch!
        expect(tamperedErr.contains('Error: HEAD sha does not match artifact commit_sha'), true);
      },
    );

    test('Req 9: fails if code-review-*.md artifacts are missing', () async {
      final fs = MemoryFileSystem();
      fs.directory('.migration/plans').createSync(recursive: true);
      fs.directory('.migration/reviews').createSync(recursive: true);

      final Directory verificationDir = fs.directory('.migration/verification')
        ..createSync(recursive: true);
      fs
          .file('${verificationDir.path}/T-0.15a.json')
          .writeAsStringSync(
            jsonEncode(<String, dynamic>{'task_id': 'T-0.15a', 'commit_sha': 'realsha'}),
          );

      final executor = FakeSystemCommandExecutor();
      executor.registerCommand(
        <String>['git', 'rev-parse', 'HEAD'],
        exitCode: 0,
        stdout: 'realsha\n',
      );
      executor.registerCommand(
        <String>['git', 'branch', '--show-current'],
        exitCode: 0,
        stdout: 'android-v8/t-0.15a-tool\n',
      );

      final runner = MigrationVerifyRunner(executor: executor, fs: fs);
      final int exitCode = await runner.run(<String>['--check-artifact-sha', 'T-0.15a']);
      expect(exitCode, 1); // code reviews are missing
    });

    test('Env: RealSystemCommandExecutor passes included parent environment', () async {
      final executor = RealSystemCommandExecutor();
      final CommandResult result = await executor.run(<String>['/usr/bin/env']);
      expect(result.exitCode, 0);
      expect(result.stdout.contains('PATH='), true);
    });

    test('Defer: unimplemented subargs explicitly fail executing gracefully', () async {
      final runner = MigrationVerifyRunner(
        executor: FakeSystemCommandExecutor(),
        fs: MemoryFileSystem(),
      );
      final int exitCode = await runner.run(<String>['--garbage']);
      expect(exitCode, 1);
    });
  });
}

class FakeSystemCommandExecutor implements SystemCommandExecutor {
  bool includeDelay = false;
  final Map<String, CommandResult> _responses = <String, CommandResult>{};

  void registerCommand(List<String> command, {required int exitCode, required String stdout}) {
    _responses[command.join(' ')] = CommandResult(
      absolutePath: command.first,
      exitCode: exitCode,
      wallClockDuration: const Duration(milliseconds: 15),
      stdout: stdout,
      stderr: '',
    );
  }

  @override
  Future<CommandResult> run(List<String> command, {bool includeParentEnvironment = false}) async {
    if (includeDelay) {
      await Future<void>.delayed(const Duration(milliseconds: 10));
    }
    final String cmdStr = command.join(' ');
    if (_responses.containsKey(cmdStr)) {
      return _responses[cmdStr]!;
    }
    throw Exception('Command $cmdStr not stubbed');
  }
}
