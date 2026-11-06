// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Enforces the 3-Layer Dependency Ratchet (ADR-0010, RFC 410.0000) for the
// Android C-API embedder migration (`shell/platform/android/`):
//
// 1. Verifies that `source_set("flutter_embedder_native_src")` in
//    `shell/platform/android/BUILD.gn` maintains `check_includes = true`,
//    `FLUTTER_ENGINE_NO_PROTOTYPES` in `defines`, and restricts `deps` to
//    `//flutter/common`, `//flutter/fml`, and
//    `//flutter/shell/platform/embedder:embedder_headers`.
// 2. Verifies that no file in `:flutter_embedder_native_src` (or any newly
//    added Android file) includes forbidden internal engine headers.
// 3. Verifies that legacy files only include headers listed in
//    `allowed_internal_headers.yaml` and that the total count never exceeds
//    `total_allowed_internal_headers`.

import 'dart:convert';
import 'dart:io';

/// Allowed include prefixes in `engine/src/flutter/shell/platform/android/`.
const List<String> kAllowedIncludePrefixes = <String>[
  'flutter/shell/platform/android/',
  'flutter/shell/platform/embedder/embedder.h',
  'flutter/common/vulkan_driver_info.h',
  'flutter/fml/',
  'third_party/googletest/',
  'third_party/abseil-cpp/',
  'third_party/rapidjson/',
];

/// Forbidden internal include prefixes tracked by the 3-Layer Dependency Ratchet.
const List<String> kInternalIncludePrefixes = <String>[
  'flutter/',
  'impeller/',
  'third_party/',
  'flow/',
  'common/',
  'shell/platform/android/',
];

/// Allowed GN dependencies for `source_set("flutter_embedder_native_src")`.
const Set<String> kAllowedEmbedderNativeDeps = <String>{
  '//flutter/common',
  '//flutter/fml',
  '//flutter/shell/platform/embedder:embedder_headers',
};

/// Default source files in `source_set("flutter_embedder_native_src")`.
const List<String> kDefaultEmbedderNativeFiles = <String>[
  'android_vsync_waiter.cc',
  'android_vsync_waiter.h',
  'android_window_metrics_mapper.cc',
  'android_window_metrics_mapper.h',
  'android_semantics_mapper.cc',
  'android_semantics_mapper.h',
  'android_vm_init.cc',
  'android_vm_init.h',
  'android_hardware_buffer.cc',
  'android_hardware_buffer.h',
  'android_vulkan_external_texture.cc',
  'android_vulkan_external_texture.h',
  'android_surface_control.cc',
  'android_surface_control.h',
  'android_mutators_mapper.cc',
  'android_mutators_mapper.h',
  'android_platform_views_controller.cc',
  'android_platform_views_controller.h',
  'android_engine_group.cc',
  'android_engine_group.h',
  'flutter_embedder_native.cc',
  'flutter_embedder_native.h',
  'jni_delegate.h',
  'jni_router.cc',
  'jni_router.h',
  'jvm_invoker.cc',
  'jvm_invoker.h',
];

/// Regular expression matching `#include "..."` directives.
final RegExp kIncludeRegex = RegExp(r'^\s*#\s*include\s+"([^"]+)"', multiLine: true);

/// Returns true if [includePath] is a forbidden internal engine header.
bool isForbiddenInternalInclude(String includePath) {
  for (final String allowed in kAllowedIncludePrefixes) {
    if (includePath == allowed || includePath.startsWith(allowed)) {
      return false;
    }
  }
  for (final String prefix in kInternalIncludePrefixes) {
    if (includePath.startsWith(prefix)) {
      return true;
    }
  }
  return false;
}

/// Parses the JSON-compatible YAML baseline file at [allowedFile].
Map<String, Object?> parseBaselineFile(File allowedFile) {
  final String rawText = allowedFile.readAsStringSync();
  final nonCommentLines = <String>[];
  for (final String line in rawText.split('\n')) {
    if (!line.trimLeft().startsWith('#')) {
      nonCommentLines.add(line);
    }
  }
  final Object? decoded = jsonDecode(nonCommentLines.join('\n'));
  if (decoded is! Map<String, Object?>) {
    throw const FormatException('Baseline root must be a JSON object.');
  }
  return decoded;
}

/// Extracts the list of source files declared in
/// `source_set("flutter_embedder_native_src")` from [buildGnText], combined
/// with [kDefaultEmbedderNativeFiles].
Set<String> extractEmbedderNativeFiles(String buildGnText) {
  final result = <String>{...kDefaultEmbedderNativeFiles};
  final RegExpMatch? blockMatch = RegExp(
    r'source_set\("flutter_embedder_native_src"\)\s*\{(.*?)\n\}',
    dotAll: true,
  ).firstMatch(buildGnText);
  if (blockMatch == null) {
    return result;
  }
  final String block = blockMatch.group(1)!;
  final RegExpMatch? sourcesMatch = RegExp(
    r'sources\s*=\s*\[(.*?)\]',
    dotAll: true,
  ).firstMatch(block);
  if (sourcesMatch == null) {
    return result;
  }
  for (final RegExpMatch strMatch in RegExp(r'"([^"]+)"').allMatches(sourcesMatch.group(1)!)) {
    result.add(strMatch.group(1)!);
  }
  return result;
}

/// Verifies the `source_set("flutter_embedder_native_src")` firewall in
/// [buildGnFile].
///
/// When [requireTarget] is false (during early Phase 0 before the GN target is
/// added), missing `source_set("flutter_embedder_native_src")` is permitted.
List<String> verifyBuildGnFirewall(File buildGnFile, {bool requireTarget = true}) {
  if (!buildGnFile.existsSync()) {
    return <String>['Missing BUILD.gn file: ${buildGnFile.path}'];
  }
  final String text = buildGnFile.readAsStringSync();
  final RegExpMatch? blockMatch = RegExp(
    r'source_set\("flutter_embedder_native_src"\)\s*\{(.*?)\n\}',
    dotAll: true,
  ).firstMatch(text);
  if (blockMatch == null) {
    if (!requireTarget) {
      return const <String>[];
    }
    return <String>[
      '${buildGnFile.path}: missing source_set("flutter_embedder_native_src") target.',
    ];
  }

  final String block = blockMatch.group(1)!;
  final errors = <String>[];

  if (!RegExp(r'check_includes\s*=\s*true').hasMatch(block)) {
    errors.add('${buildGnFile.path}: flutter_embedder_native_src must set check_includes = true.');
  }
  if (!block.contains('"FLUTTER_ENGINE_NO_PROTOTYPES"')) {
    errors.add(
      '${buildGnFile.path}: flutter_embedder_native_src must define "FLUTTER_ENGINE_NO_PROTOTYPES".',
    );
  }

  final RegExpMatch? depsMatch = RegExp(r'deps\s*=\s*\[(.*?)\]', dotAll: true).firstMatch(block);
  if (depsMatch == null) {
    errors.add('${buildGnFile.path}: flutter_embedder_native_src is missing deps = [...].');
  } else {
    final deps = <String>{
      for (final m in RegExp(r'"([^"]+)"').allMatches(depsMatch.group(1)!)) m.group(1)!,
    };
    final Set<String> forbiddenDeps = deps.difference(kAllowedEmbedderNativeDeps);
    if (forbiddenDeps.isNotEmpty) {
      final List<String> sortedForbidden = forbiddenDeps.toList()..sort();
      final List<String> sortedAllowed = kAllowedEmbedderNativeDeps.toList()..sort();
      errors.add(
        '${buildGnFile.path}: flutter_embedder_native_src contains forbidden deps: '
        '$sortedForbidden (allowed: $sortedAllowed).',
      );
    }
  }
  return errors;
}

/// Scans all `.h`, `.cc`, and `.cpp` files under [androidDir] and returns a
/// map from relative path to sorted forbidden internal header includes.
Map<String, List<String>> scanAndroidInternalIncludes(Directory androidDir) {
  final violationsByFile = <String, List<String>>{};
  if (!androidDir.existsSync()) {
    return violationsByFile;
  }
  final String rootPath = androidDir.path.endsWith('/') ? androidDir.path : '${androidDir.path}/';

  final List<File> files =
      androidDir
          .listSync(recursive: true, followLinks: false)
          .whereType<File>()
          .where(
            (File f) => f.path.endsWith('.h') || f.path.endsWith('.cc') || f.path.endsWith('.cpp'),
          )
          .toList()
        ..sort((File a, File b) => a.path.compareTo(b.path));

  for (final file in files) {
    final String relPath = file.path.startsWith(rootPath)
        ? file.path.substring(rootPath.length)
        : file.path;
    final String content = file.readAsStringSync();
    final violations = <String>{};
    for (final RegExpMatch match in kIncludeRegex.allMatches(content)) {
      final String header = match.group(1)!;
      if (isForbiddenInternalInclude(header)) {
        violations.add(header);
      }
    }
    if (violations.isNotEmpty) {
      final List<String> sorted = violations.toList()..sort();
      violationsByFile[relPath] = sorted;
    }
  }
  return violationsByFile;
}

/// Generates the JSON-compatible YAML baseline string for [androidDir].
String generateBaselineContent(Directory androidDir) {
  final Map<String, List<String>> violationsByFile = scanAndroidInternalIncludes(androidDir);
  final List<String> sortedFileKeys = violationsByFile.keys.toList()..sort();
  final orderedLegacyMap = <String, List<String>>{};
  final uniqueHeadersSet = <String>{};
  var totalCount = 0;

  for (final key in sortedFileKeys) {
    final List<String> headers = violationsByFile[key]!;
    orderedLegacyMap[key] = headers;
    uniqueHeadersSet.addAll(headers);
    totalCount += headers.length;
  }

  final List<String> sortedUniqueHeaders = uniqueHeadersSet.toList()..sort();
  final payload = <String, Object>{
    'allowed_legacy_files': orderedLegacyMap,
    'allowed_unique_headers': sortedUniqueHeaders,
    'governing_adr': 'ADR-0010',
    'rfc': 'RFC-410.0000',
    'total_allowed_internal_headers': totalCount,
  };

  const headerComment =
      '# Copyright 2013 The Flutter Authors. All rights reserved.\n'
      '# Use of this source code is governed by a BSD-style license that can be\n'
      '# found in the LICENSE file.\n'
      '#\n'
      '# JSON-compatible YAML baseline for the 3-Layer Dependency Ratchet (ADR-0010).\n';
  return '$headerComment${const JsonEncoder.withIndent('  ').convert(payload)}\n';
}

/// Verifies C++ source includes in [androidDir] against [allowedData] and
/// [embedderNativeFiles].
List<String> verifySourceIncludes(
  Directory androidDir,
  Map<String, Object?> allowedData, {
  Set<String> embedderNativeFiles = const <String>{},
}) {
  final errors = <String>[];
  final Object? rawTotal = allowedData['total_allowed_internal_headers'];
  final int totalAllowed = rawTotal is int ? rawTotal : -1;
  if (totalAllowed < 0) {
    errors.add(
      'allowed_internal_headers.yaml: total_allowed_internal_headers must be a non-negative integer.',
    );
    return errors;
  }

  final Object? rawLegacyFiles = allowedData['allowed_legacy_files'];
  final legacyFilesMap = <String, Set<String>>{};
  if (rawLegacyFiles is Map) {
    for (final MapEntry<dynamic, dynamic> entry in rawLegacyFiles.entries) {
      final key = entry.key.toString();
      final Object? val = entry.value;
      if (val is List) {
        legacyFilesMap[key] = val.map((Object? e) => e.toString()).toSet();
      }
    }
  }

  final Object? rawUniqueHeaders = allowedData['allowed_unique_headers'];
  final List<String> uniqueHeadersList = rawUniqueHeaders is List
      ? rawUniqueHeaders.map((Object? e) => e.toString()).toList()
      : <String>[];

  if (totalAllowed == 0) {
    if (legacyFilesMap.isNotEmpty) {
      errors.add(
        'allowed_internal_headers.yaml: allowed_legacy_files must be empty when '
        'total_allowed_internal_headers == 0 (found: ${legacyFilesMap.keys.toList()}).',
      );
    }
    if (uniqueHeadersList.isNotEmpty) {
      errors.add(
        'allowed_internal_headers.yaml: allowed_unique_headers must be empty when '
        'total_allowed_internal_headers == 0 (found: $uniqueHeadersList).',
      );
    }
  }

  for (final nativeFile in embedderNativeFiles) {
    if (legacyFilesMap.containsKey(nativeFile)) {
      errors.add(
        '$nativeFile: :flutter_embedder_native_src file must never appear in '
        'allowed_legacy_files.',
      );
    }
  }

  final Map<String, List<String>> violationsByFile = scanAndroidInternalIncludes(androidDir);
  var actualTotal = 0;

  for (final MapEntry<String, List<String>> entry in violationsByFile.entries) {
    final String relPath = entry.key;
    final List<String> violations = entry.value;
    actualTotal += violations.length;

    if (embedderNativeFiles.contains(relPath)) {
      errors.add(
        '$relPath: forbidden internal engine header(s) in :flutter_embedder_native_src: '
        '$violations',
      );
      continue;
    }

    final Set<String>? allowedForFile = legacyFilesMap[relPath];
    if (allowedForFile == null) {
      errors.add(
        '$relPath: new or migrated file is not in allowed_legacy_files and '
        'includes forbidden internal engine header(s): $violations',
      );
      continue;
    }

    final List<String> unallowed = violations
        .where((String h) => !allowedForFile.contains(h))
        .toList();
    if (unallowed.isNotEmpty) {
      errors.add(
        '$relPath: includes forbidden internal engine header(s) not permitted by '
        'allowed_legacy_files: $unallowed',
      );
    }
  }

  if (actualTotal > totalAllowed) {
    errors.add(
      'Total internal header includes ($actualTotal) exceeds '
      'total_allowed_internal_headers ($totalAllowed).',
    );
  }

  return errors;
}

/// Runs the dependency ratchet check with CLI [args], returning an exit code.
int runCheck(List<String> args, {StringSink? outSink, StringSink? errSink}) {
  final StringSink out = outSink ?? stdout;
  final StringSink err = errSink ?? stderr;

  String? allowedFilePath;
  String? androidDirPath;
  String? buildGnPath;
  var generateBaseline = false;

  for (var i = 0; i < args.length; i++) {
    final String arg = args[i];
    if (arg == '--generate-baseline') {
      generateBaseline = true;
    } else if (arg.startsWith('--allowed-file=')) {
      allowedFilePath = arg.substring('--allowed-file='.length);
    } else if (arg == '--allowed-file' && i + 1 < args.length) {
      allowedFilePath = args[++i];
    } else if (arg.startsWith('--android-dir=')) {
      androidDirPath = arg.substring('--android-dir='.length);
    } else if (arg == '--android-dir' && i + 1 < args.length) {
      androidDirPath = args[++i];
    } else if (arg.startsWith('--build-gn=')) {
      buildGnPath = arg.substring('--build-gn='.length);
    } else if (arg == '--build-gn' && i + 1 < args.length) {
      buildGnPath = args[++i];
    }
  }

  if (allowedFilePath == null || androidDirPath == null || buildGnPath == null) {
    err.writeln(
      'Usage: dart check_android_embedder_deps.dart '
      '--allowed-file=<path> --android-dir=<path> --build-gn=<path> '
      '[--generate-baseline]',
    );
    return 1;
  }

  final allowedFile = File(allowedFilePath);
  final androidDir = Directory(androidDirPath);
  final buildGnFile = File(buildGnPath);

  if (generateBaseline) {
    final String generated = generateBaselineContent(androidDir);
    allowedFile.parent.createSync(recursive: true);
    allowedFile.writeAsStringSync(generated);
    out.writeln('Generated baseline at ${allowedFile.path}.');
    return 0;
  }

  if (!allowedFile.existsSync()) {
    err.writeln('ERROR: Missing baseline file: ${allowedFile.path}');
    return 1;
  }

  final Map<String, Object?> allowedData;
  try {
    allowedData = parseBaselineFile(allowedFile);
  } on Object catch (e) {
    err.writeln('ERROR: Failed to parse ${allowedFile.path}: $e');
    return 1;
  }

  final Object? rawTotal = allowedData['total_allowed_internal_headers'];
  final int totalAllowed = rawTotal is int ? rawTotal : -1;
  final requireBuildGnTarget = totalAllowed == 0;

  final String buildGnText = buildGnFile.existsSync() ? buildGnFile.readAsStringSync() : '';
  final Set<String> embedderNativeFiles = extractEmbedderNativeFiles(buildGnText);

  final errors = <String>[
    ...verifyBuildGnFirewall(buildGnFile, requireTarget: requireBuildGnTarget),
    ...verifySourceIncludes(androidDir, allowedData, embedderNativeFiles: embedderNativeFiles),
  ];

  if (errors.isNotEmpty) {
    err.writeln('Android Embedder C-API Dependency Ratchet FAILED:');
    for (final error in errors) {
      err.writeln('  - $error');
    }
    return 1;
  }

  out.writeln(
    'Android Embedder C-API Dependency Ratchet PASSED '
    '(total_allowed_internal_headers = $totalAllowed).',
  );
  return 0;
}

void main(List<String> args) {
  exitCode = runCheck(args);
}
