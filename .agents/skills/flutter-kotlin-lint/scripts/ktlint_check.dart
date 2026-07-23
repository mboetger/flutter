// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:convert';
import 'dart:io';

Future<void> main(List<String> args) async {
  final bool format = args.contains('--format') || args.contains('-F');

  // 1. Find repo root.
  final Directory repoRoot = _findRepoRoot();
  
  // 2. Find changed Kotlin files.
  final List<String> changedFiles = await _getChangedKotlinFiles(repoRoot);
  if (changedFiles.isEmpty) {
    print('No changed Kotlin files found.');
    exit(0);
  }

  print('Found changed Kotlin files:');
  for (final String file in changedFiles) {
    print('  $file');
  }

  // 3. Identify ktlint version from .ci.yaml.
  final String? ktlintVersion = await _getKtlintVersion(repoRoot);
  if (ktlintVersion == null) {
    stderr.writeln('Error: Could not identify ktlint version from .ci.yaml.');
    exit(1);
  }
  print('Identified ktlint version: $ktlintVersion');

  // 4. Download/export ktlint if not already cached.
  final String ktlintBin = await _ensureKtlintBinary(ktlintVersion);

  // 5. Run ktlint.
  final String editorconfig = 'dev/bots/test/analyze-test-input/.editorconfig';
  final String baseline = 'dev/bots/test/analyze-test-input/ktlint-baseline.xml';

  final List<String> ktlintArgs = <String>[
    '--editorconfig=${repoRoot.path}/$editorconfig',
    '--baseline=${repoRoot.path}/$baseline',
  ];

  if (format) {
    ktlintArgs.add('-F');
    print('Running ktlint in format mode...');
  }

  // Only pass files that actually exist.
  final List<String> existingFiles = changedFiles
      .map((String relPath) => '${repoRoot.path}/$relPath')
      .where((String absPath) => File(absPath).existsSync())
      .toList();

  if (existingFiles.isEmpty) {
    print('No existing changed Kotlin files to lint.');
    exit(0);
  }

  ktlintArgs.addAll(existingFiles);

  Process process;
  if (Platform.isWindows) {
    final List<String> javaArgs = <String>['-jar', ktlintBin]..addAll(ktlintArgs);
    print('Running: java ${javaArgs.join(' ')}');
    process = await Process.start('java', javaArgs, mode: ProcessStartMode.inheritStdio);
  } else {
    print('Running: $ktlintBin ${ktlintArgs.join(' ')}');
    process = await Process.start(ktlintBin, ktlintArgs, mode: ProcessStartMode.inheritStdio);
  }
  final int exitCode = await process.exitCode;

  if (exitCode == 0) {
    print('ktlint passed!');
  } else {
    stderr.writeln('ktlint failed with exit code $exitCode');
    if (!format) {
      print('You can try running with --format to automatically fix some issues.');
    }
    exit(exitCode);
  }
}

Directory _findRepoRoot() {
  Directory dir = Directory.current;
  while (dir.path != dir.parent.path) {
    if (File('${dir.path}/.ci.yaml').existsSync()) {
      return dir;
    }
    dir = dir.parent;
  }
  throw StateError('Could not find repo root containing .ci.yaml');
}

Future<List<String>> _getChangedKotlinFiles(Directory repoRoot) async {
  // Get merge base
  String mergeBase = 'HEAD';
  try {
    final ProcessResult upstreamResult = await Process.run(
      'git',
      <String>['merge-base', 'upstream/master', 'HEAD'],
      workingDirectory: repoRoot.path,
    );
    if (upstreamResult.exitCode == 0) {
      mergeBase = (upstreamResult.stdout as String).trim();
    } else {
      final ProcessResult originResult = await Process.run(
        'git',
        <String>['merge-base', 'origin/master', 'HEAD'],
        workingDirectory: repoRoot.path,
      );
      if (originResult.exitCode == 0) {
        mergeBase = (originResult.stdout as String).trim();
      }
    }
  } catch (e) {
    // Ignore, fallback to HEAD
  }

  // Get diffs
  final List<List<String>> diffCommands = <List<String>>[
    <String>['diff', '--name-only', mergeBase, 'HEAD'],
    <String>['diff', '--name-only'],
    <String>['diff', '--cached', '--name-only'],
  ];

  final Set<String> changedFiles = <String>{};
  for (final List<String> cmd in diffCommands) {
    final ProcessResult result = await Process.run('git', cmd, workingDirectory: repoRoot.path);
    if (result.exitCode == 0) {
      final List<String> lines = (result.stdout as String).split('\n');
      for (final String line in lines) {
        final String trimmed = line.trim();
        if (trimmed.endsWith('.kt') || trimmed.endsWith('.kts')) {
          changedFiles.add(trimmed);
        }
      }
    }
  }

  return changedFiles.toList()..sort();
}

Future<String?> _getKtlintVersion(Directory repoRoot) async {
  final File ciYaml = File('${repoRoot.path}/.ci.yaml');
  if (!ciYaml.existsSync()) {
    return null;
  }

  final String content = await ciYaml.readAsString();
  // Regex to match: {"dependency": "ktlint", "version": "version_1_5_0"}
  // or similar.
  final RegExp regExp = RegExp(r'"dependency"\s*:\s*"ktlint"\s*,\s*"version"\s*:\s*"([^"]+)"');
  final Match? match = regExp.firstMatch(content);
  return match?.group(1);
}

Future<String> _ensureKtlintBinary(String version) async {
  final String home = Platform.environment['HOME'] ?? Platform.environment['USERPROFILE'] ?? '';
  if (home.isEmpty) {
    throw StateError('Could not find HOME or USERPROFILE environment variable');
  }

  final String cacheDir = '$home/.cipd/cache/ktlint/$version';
  final String binName = 'ktlint';
  final String ktlintBin = '$cacheDir/$binName';

  if (File(ktlintBin).existsSync()) {
    print('Using cached ktlint from $ktlintBin');
    return ktlintBin;
  }

  print('Downloading ktlint $version via CIPD...');
  Directory(cacheDir).createSync(recursive: true);

  // We always use the linux-amd64 package because it is the only one available
  // in CIPD and it contains a cross-platform shell script wrapper around a JAR.
  const String cipdPlatform = 'linux-amd64';

  final String ensureContent = 'flutter/ktlint/$cipdPlatform $version\n';
  
  // Run cipd export
  final Process process = await Process.start(
    'cipd',
    <String>['export', '-root', cacheDir, '-ensure-file', '-'],
  );

  process.stdin.write(ensureContent);
  await process.stdin.close();

  final int exitCode = await process.exitCode;
  if (exitCode != 0) {
    final String stderrContent = await process.stderr.transform(utf8.decoder).join();
    final String stdoutContent = await process.stdout.transform(utf8.decoder).join();
    stderr.writeln('CIPD export failed with exit code $exitCode');
    stderr.writeln('Stdout: $stdoutContent');
    stderr.writeln('Stderr: $stderrContent');
    exit(1);
  }

  if (!Platform.isWindows) {
    await Process.run('chmod', <String>['+x', ktlintBin]);
  }

  return ktlintBin;
}
