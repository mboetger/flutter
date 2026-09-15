// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:convert';

import 'package:file/file.dart';
import 'package:file/local.dart';
import 'package:path/path.dart' as path;

import '../run_command.dart';
import '../utils.dart';

/// Explicit registry of all supported Android composition modes.
///
/// Per Invariant I-11, all four composition modes (VD, TLHC, HC, HCPP) and both
/// external texture mechanisms (SurfaceTexture, SurfaceProducer) must be
/// preserved throughout the Android embedder migration.
enum CompositionMode {
  virtualDisplay,
  textureLayerHybrid,
  hybrid,
  hcpp,
  surfaceTexture,
  surfaceProducer,
}

/// Explicit list of test mains for each composition mode.
///
/// Using an explicit list instead of path globbing ensures that a file deletion
/// or relocation becomes an immediate build/test failure rather than a silent skip.
const Map<CompositionMode, List<String>> kCompositionModeMains = <CompositionMode, List<String>>{
  CompositionMode.virtualDisplay: <String>[
    'lib/platform_view/virtual_display_platform_view_main.dart',
  ],
  CompositionMode.textureLayerHybrid: <String>[
    'lib/platform_view/texture_layer_hybrid_composition_platform_view_main.dart',
  ],
  CompositionMode.hybrid: <String>['lib/platform_view/hybrid_composition_platform_view_main.dart'],
  CompositionMode.hcpp: <String>[
    'lib/hcpp/platform_view_clippath_main.dart',
    'lib/hcpp/platform_view_cliprect_surfaceview_main.dart',
    'lib/hcpp/platform_view_fractional_size_main.dart',
    'lib/hcpp/platform_view_main.dart',
    'lib/hcpp/platform_view_opacity_main.dart',
    'lib/hcpp/platform_view_overlapping_main.dart',
    'lib/hcpp/platform_view_transform_main.dart',
    'lib/hcpp/rtl_mirror_main.dart',
    'lib/hcpp/tap_color_change_main.dart',
    'lib/hcpp/upgrade_legacy_pv_types_main.dart',
  ],
  CompositionMode.surfaceTexture: <String>[
    'lib/external_texture/surface_texture_smiley_face_main.dart',
  ],
  CompositionMode.surfaceProducer: <String>[
    'lib/external_texture/surface_producer_smiley_face_main.dart',
  ],
};

/// General integration test mains that exercise common platform functionality.
const List<String> kGeneralIntegrationMains = <String>[
  'lib/platform_view/hide_show_hide_main.dart',
  'lib/platform_view_tap_color_change_main.dart',
  'lib/flutter_rendered_blue_rectangle_main.dart',
  'lib/system_ui_mode_transitions_main.dart',
];

/// Encapsulates the declaration of whether a mode is supported on a backend.
class ModeSupport {
  const ModeSupport.supported() : isSupported = true, reason = null;
  const ModeSupport.unsupported({required this.reason}) : isSupported = false;

  final bool isSupported;
  final String? reason;
}

/// Evaluates declared support for a composition mode on the given backend.
ModeSupport checkModeSupport(CompositionMode mode, ImpellerBackend backend) {
  if (mode == CompositionMode.hcpp && backend == ImpellerBackend.opengles) {
    return const ModeSupport.unsupported(
      reason: 'HCPP structurally requires Impeller Vulkan and Android API 34; on OpenGLES apps fall back to HC/TLHC.',
    );
  }
  return const ModeSupport.supported();
}

/// To run this test locally:
///
/// 1. Connect an Android device or emulator.
/// 2. Run `dart pub get` in dev/bots
/// 3. Run the following command from the root of the Flutter repository:
///
/// ```sh
/// # Generate a baseline of local golden files.
/// SHARD=android_engine_vulkan_tests UPDATE_GOLDENS=1 bin/cache/dart-sdk/bin/dart dev/bots/test.dart
/// ```
///
/// 4. Then, re-run the command against the baseline images:
///
/// ```sh
/// SHARD=android_engine_vulkan_tests bin/cache/dart-sdk/bin/dart dev/bots/test.dart
/// ```
///
/// If you are trying to debug a commit, you will want to run step (3) first,
/// then apply the commit (or flag), and then run step (4). If you are trying
/// to determine flakiness in the *same* state, or want better debugging, see
/// `dev/integration_tests/android_engine_test/README.md`.
Future<void> runAndroidEngineTests({
  required ImpellerBackend impellerBackend,
  bool? androidEmbedderApi,
}) async {
  print(
    'Running Flutter Driver Android tests (backend=$impellerBackend, androidEmbedderApi=$androidEmbedderApi)',
  );

  final String androidEngineTestPath = path.join('dev', 'integration_tests', 'android_engine_test');
  const FileSystem fs = LocalFileSystem();

  // Validate the explicit mode registry against the filesystem.
  for (final MapEntry<CompositionMode, List<String>> entry in kCompositionModeMains.entries) {
    if (entry.value.isEmpty) {
      foundError(<String>[
        'CompositionMode.${entry.key.name} has no registered test mains in kCompositionModeMains.',
      ]);
    }
    for (final String relPath in entry.value) {
      final File mainFile = fs.file(path.join(androidEngineTestPath, relPath));
      if (!mainFile.existsSync()) {
        foundError(<String>[
          'Registered test main for CompositionMode.${entry.key.name} does not exist: ${mainFile.path}',
        ]);
      }
    }
  }

  for (final String relPath in kGeneralIntegrationMains) {
    final File mainFile = fs.file(path.join(androidEngineTestPath, relPath));
    if (!mainFile.existsSync()) {
      foundError(<String>['General integration test main does not exist: ${mainFile.path}']);
    }
  }

  final File androidManifestXml = fs.file(
    path.join(androidEngineTestPath, 'android', 'app', 'src', 'main', 'AndroidManifest.xml'),
  );
  final String androidManifestContents = androidManifestXml.readAsStringSync();

  final executedPerMode = <CompositionMode, int>{
    for (final CompositionMode mode in CompositionMode.values) mode: 0,
  };

  try {
    // Replace whatever the current backend is with the specified backend.
    final impellerBackendMetadata = RegExp(_impellerBackendMetadata(value: '.*'));
    androidManifestXml.writeAsStringSync(
      androidManifestContents.replaceFirst(
        impellerBackendMetadata,
        _impellerBackendMetadata(value: impellerBackend.name),
      ),
    );

    Future<void> runTest(
      String relativePath, {
      bool? useHCPPFlag,
      Map<String, String>? additionalEnvironment,
      CompositionMode? mode,
    }) async {
      final CommandResult result = await runCommand(
        'flutter',
        <String>[
          'drive',
          relativePath,
          '--no-dds',
          '--no-enable-dart-profiling',
          if (useHCPPFlag == true) '--enable-hcpp',
          if (useHCPPFlag == false) '--no-enable-hcpp',
          if (androidEmbedderApi == true) '--android-embedder-api',
          if (androidEmbedderApi == false) '--no-android-embedder-api',
          '--test-arguments=test',
          '--test-arguments=--reporter=expanded',
        ],
        workingDirectory: androidEngineTestPath,
        environment: <String, String>{
          'ANDROID_ENGINE_TEST_GOLDEN_VARIANT': impellerBackend.name,
          ...?additionalEnvironment,
        },
      );
      final String? stdout = result.flattenedStdout;
      if (stdout == null) {
        foundError(<String>['No stdout produced for $relativePath.']);
        return;
      }
      if (mode != null) {
        executedPerMode[mode] = (executedPerMode[mode] ?? 0) + 1;
      }
    }

    // 1. Run general integration tests.
    for (final String relPath in kGeneralIntegrationMains) {
      await runTest(relPath);
    }

    // 2. Run standard composition modes (non-HCPP).
    for (final MapEntry<CompositionMode, List<String>> entry in kCompositionModeMains.entries) {
      final CompositionMode mode = entry.key;
      if (mode == CompositionMode.hcpp) {
        continue;
      }
      final ModeSupport support = checkModeSupport(mode, impellerBackend);
      if (!support.isSupported) {
        print('Skipping ${mode.name} on $impellerBackend: ${support.reason}');
        continue;
      }
      for (final String relPath in entry.value) {
        await runTest(relPath, mode: mode);
      }
    }

    // 3. Run HCPP on Vulkan or fallback coverage on OpenGLES.
    final ModeSupport hcppSupport = checkModeSupport(CompositionMode.hcpp, impellerBackend);
    if (hcppSupport.isSupported) {
      const upgradeLegacyPvMain = 'lib/hcpp/upgrade_legacy_pv_types_main.dart';

      // Test flag override before manifest modification.
      await runTest(upgradeLegacyPvMain, useHCPPFlag: true, mode: CompositionMode.hcpp);

      androidManifestXml.writeAsStringSync(
        androidManifestXml.readAsStringSync().replaceFirst(
          kHcppMetadataDisabled,
          kHcppMetadataEnabled,
        ),
      );

      // Verify --no-enable-hcpp overrides manifest enabled state.
      await runTest(
        upgradeLegacyPvMain,
        useHCPPFlag: false,
        additionalEnvironment: const <String, String>{'EXPECT_HCPP': 'false'},
        mode: CompositionMode.hcpp,
      );

      for (final String relPath in kCompositionModeMains[CompositionMode.hcpp]!) {
        if (relPath == upgradeLegacyPvMain) {
          continue;
        }
        await runTest(relPath, mode: CompositionMode.hcpp);
      }
    } else {
      print('Skipping full HCPP suite on $impellerBackend: ${hcppSupport.reason}');
      // GLES fallback test: verify that requesting HCPP on OpenGLES falls back to HC/TLHC.
      print('Running HCPP->HC/TLHC GLES fallback test.');
      await runTest(
        'lib/hcpp/upgrade_legacy_pv_types_main.dart',
        useHCPPFlag: true,
        additionalEnvironment: const <String, String>{'EXPECT_HCPP': 'false'},
        mode: CompositionMode.hcpp,
      );
    }

    // 4. Assert non-empty: Every supported mode must have executed at least 1 test.
    for (final CompositionMode mode in CompositionMode.values) {
      final ModeSupport support = checkModeSupport(mode, impellerBackend);
      final int count = executedPerMode[mode] ?? 0;
      if (support.isSupported && count == 0) {
        foundError(<String>[
          'Composition mode "${mode.name}" is supported on $impellerBackend but 0 tests were executed! Per Invariant I-11, silent deletion or skipping of composition modes is strictly forbidden.',
        ]);
      }
    }

    // 5. Emit machine-readable matrix results.
    final matrixResults = <Map<String, dynamic>>[];
    for (final CompositionMode mode in CompositionMode.values) {
      final ModeSupport support = checkModeSupport(mode, impellerBackend);
      final int count = executedPerMode[mode] ?? 0;
      matrixResults.add(<String, dynamic>{
        'mode': mode.name,
        'backend': impellerBackend.name,
        'flag_state': androidEmbedderApi == null
            ? 'default'
            : (androidEmbedderApi ? 'flag-on' : 'flag-off'),
        'supported': support.isSupported,
        if (!support.isSupported) 'skip_reason': support.reason,
        'tests_executed': count,
        'passed': !hasError,
      });
    }

    final File summaryFile = fs.file(
      path.join(androidEngineTestPath, 'composition_matrix_results.json'),
    );
    summaryFile.writeAsStringSync(const JsonEncoder.withIndent('  ').convert(matrixResults));
    print('Emitted composition matrix summary to ${summaryFile.path}');
  } finally {
    // Restore original contents.
    androidManifestXml.writeAsStringSync(androidManifestContents);
  }
}

const String kHcppMetadataDisabled =
    '<meta-data android:name="io.flutter.embedding.android.EnableHcpp" android:value="false" />';
const String kHcppMetadataEnabled =
    '<meta-data android:name="io.flutter.embedding.android.EnableHcpp" android:value="true" />';

String _impellerBackendMetadata({required String value}) {
  return '<meta-data android:name="io.flutter.embedding.android.ImpellerBackend" android:value="$value" />';
}

enum ImpellerBackend { vulkan, opengles }
