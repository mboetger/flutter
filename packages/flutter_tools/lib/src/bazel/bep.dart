// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:convert';
import '../base/file_system.dart';
import '../base/logger.dart';

/// Parses Bazel Build Event Protocol (BEP) streams and routes them to the CLI logger.
class BepParser {
  BepParser({required this.logger});

  final Logger logger;

  /// Parses a BEP JSON file dynamically as it is populated or after completion.
  ///
  /// Each line in a BEP JSON file constitutes a standalone JSON object representing
  /// one lifecycle event of the Bazel build.
  Future<void> parseFile(File bepJsonFile) async {
    if (!bepJsonFile.existsSync()) {
      return;
    }

    final Stream<String> lines = bepJsonFile
        .openRead()
        .transform(utf8.decoder)
        .transform(const LineSplitter());

    await for (final String line in lines) {
      if (line.trim().isEmpty) {
        continue;
      }

      try {
        final event = json.decode(line) as Map<String, dynamic>;
        _processEvent(event);
      } on Exception {
        // Skip malformed BEP lines
      }
    }
  }

  void _processEvent(Map<String, dynamic> event) {
    if (event.containsKey('progress')) {
      final progress = event['progress'] as Map<String, dynamic>;
      if (progress.containsKey('stdout')) {
        logger.printStatus(progress['stdout'] as String);
      }
      if (progress.containsKey('stderr')) {
        logger.printError(progress['stderr'] as String);
      }
    } else if (event.containsKey('aborted')) {
      final aborted = event['aborted'] as Map<String, dynamic>;
      logger.printError('Bazel build aborted: ${aborted['description']}');
    }
  }
}
