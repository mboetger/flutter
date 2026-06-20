// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:convert' show jsonDecode, jsonEncode;
import 'dart:io' as io;

import 'package:clang_tidy/clang_tidy.dart';
import 'package:clang_tidy/src/lint_target.dart';
import 'package:clang_tidy/src/options.dart';
import 'package:engine_repo_tools/engine_repo_tools.dart';
import 'package:path/path.dart' as path;
import 'package:test/test.dart';

void main() {
  final engineRoot = Engine.findWithin();

  group('C++ literal suffix checks', () {
    late io.File tempCppFile;
    late io.File tempCompileCommandsFile;

    void setupTempCppFile(String content) {
      final String fmlDir = path.join(engineRoot.flutterDir.path, 'fml');
      tempCppFile = io.File(path.join(fmlDir, 'literal_suffix_reproduce_temp.cc'));
      tempCppFile.writeAsStringSync(content);

      // Load the real compile_commands.json to find a valid C++ command template.
      final String? compileCommandsPath = engineRoot.latestOutput()?.compileCommandsJson.path;
      if (compileCommandsPath == null) {
        fail('No build output or compile_commands.json found.');
      }
      final originalCompileCommands = io.File(compileCommandsPath);
      final originalCommands = List<dynamic>.from(
        jsonDecode(originalCompileCommands.readAsStringSync()) as Iterable<dynamic>,
      );

      // Find the first command for a C++ file (ending in .cc).
      final templateCommand =
          originalCommands.firstWhere((dynamic cmd) {
                final map = cmd as Map<String, dynamic>;
                return (map['file'] as String).endsWith('.cc');
              }, orElse: () => fail('No C++ commands found in compile_commands.json'))
              as Map<String, dynamic>;

      // Construct a command for our temporary C++ file.
      final originalFile = templateCommand['file'] as String;
      final String originalFileBasename = path.basename(originalFile);
      final originalCommandStr = templateCommand['command'] as String;

      // Replace the file references in the command.
      final String tempFileRelative = path.relative(
        tempCppFile.path,
        from: templateCommand['directory'] as String,
      );
      final String tempFileBasename = path.basename(tempCppFile.path);
      final String newCommandStr = originalCommandStr
          .replaceAll(originalFile, tempFileRelative)
          .replaceAll(originalFileBasename, tempFileBasename);

      final tempCommand = <String, dynamic>{
        'directory': templateCommand['directory'],
        'file': tempFileRelative,
        'command': newCommandStr,
      };

      // Write the new command to a temporary compile_commands.json.
      tempCompileCommandsFile = io.File(
        path.join(io.Directory.systemTemp.path, 'temp_compile_commands.json'),
      );
      tempCompileCommandsFile.writeAsStringSync(jsonEncode(<Map<String, dynamic>>[tempCommand]));
    }

    tearDown(() {
      if (tempCppFile.existsSync()) {
        tempCppFile.deleteSync();
      }
      if (tempCompileCommandsFile.existsSync()) {
        tempCompileCommandsFile.deleteSync();
      }
    });

    test('C++ clang-tidy should FAIL on lowercase literal suffixes (negative case)', () async {
      setupTempCppFile('''
#include <cstdint>
void dummy_function() {
  int64_t a = 123ll;
  (void)a;
}
''');

      final options = Options(
        buildCommandsPath: tempCompileCommandsFile,
        lintTarget: const LintAll(),
        verbose: true,
      );
      final outSink = StringBuffer();
      final errSink = StringBuffer();
      final clangTidy = ClangTidy(
        buildCommandsPath: options.buildCommandsPath,
        lintTarget: options.lintTarget,
        outSink: outSink,
        errSink: errSink,
      );

      final int exitCode = await clangTidy.run();
      final output = '$outSink\n$errSink';

      expect(
        exitCode,
        equals(1),
        reason: 'clang-tidy should have failed due to lowercase literal suffix "ll"',
      );
      expect(
        output,
        contains('readability-uppercase-literal-suffix'),
        reason:
            'The error output should specifically reference the readability-uppercase-literal-suffix lint rule.',
      );
    });

    test('C++ clang-tidy should PASS on uppercase literal suffixes (positive case)', () async {
      setupTempCppFile('''
#include <cstdint>
void dummy_function() {
  int64_t a = 123LL;
  (void)a;
}
''');

      final options = Options(
        buildCommandsPath: tempCompileCommandsFile,
        lintTarget: const LintAll(),
        verbose: true,
      );
      final outSink = StringBuffer();
      final errSink = StringBuffer();
      final clangTidy = ClangTidy(
        buildCommandsPath: options.buildCommandsPath,
        lintTarget: options.lintTarget,
        outSink: outSink,
        errSink: errSink,
      );

      final int exitCode = await clangTidy.run();
      final output = '$outSink\n$errSink';

      expect(
        exitCode,
        equals(0),
        reason: 'clang-tidy should have passed on uppercase literal suffix "LL". Output:\n$output',
      );
    });
  });

  group('Java literal suffix checks', () {
    late io.File tempJavaFile;
    late io.File tempStampFile;

    void setupTempJavaFile(String content) {
      final String tempDir = io.Directory.systemTemp.path;
      tempJavaFile = io.File(path.join(tempDir, 'LiteralSuffixReproduceTemp.java'));
      tempJavaFile.writeAsStringSync(content);
      tempStampFile = io.File(path.join(tempDir, 'literal_suffix_reproduce.stamp'));
    }

    tearDown(() {
      if (tempJavaFile.existsSync()) {
        tempJavaFile.deleteSync();
      }
      if (tempStampFile.existsSync()) {
        tempStampFile.deleteSync();
      }
    });

    test('Java lint checks should FAIL on lowercase literal suffixes (negative case)', () async {
      setupTempJavaFile('''
package io.flutter;
public class LiteralSuffixReproduceTemp {
    public void dummy() {
        long x = 1l;
    }
}
''');

      final String scriptPath = path.join(
        engineRoot.flutterDir.path,
        'tools',
        'android_illegal_imports.py',
      );
      final io.ProcessResult result = await io.Process.run('python3', <String>[
        scriptPath,
        '--stamp',
        tempStampFile.path,
        '--files',
        tempJavaFile.path,
      ]);

      final output = '${result.stdout}\n${result.stderr}';

      expect(
        result.exitCode,
        isNot(equals(0)),
        reason: 'Java checks should have failed due to lowercase literal suffix "l"',
      );
      expect(
        output,
        contains("Use uppercase 'L' for long literals"),
        reason:
            'The error output should specifically complain about the lowercase "l" suffix for long literals.',
      );
    });

    test('Java lint checks should PASS on uppercase literal suffixes (positive case)', () async {
      setupTempJavaFile('''
package io.flutter;
public class LiteralSuffixReproduceTemp {
    public void dummy() {
        long x = 1L;
    }
}
''');

      final String scriptPath = path.join(
        engineRoot.flutterDir.path,
        'tools',
        'android_illegal_imports.py',
      );
      final io.ProcessResult result = await io.Process.run('python3', <String>[
        scriptPath,
        '--stamp',
        tempStampFile.path,
        '--files',
        tempJavaFile.path,
      ]);

      final output = '${result.stdout}\n${result.stderr}';

      expect(
        result.exitCode,
        equals(0),
        reason: 'Java checks should have passed on uppercase literal suffix "L". Output:\n$output',
      );
    });
  });
}
