// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../base/file_system.dart';
import '../plugins.dart';

/// Generates transient BUILD.bazel files that wrap legacy Gradle builds
/// for existing Flutter plugins. This allows Bazel to orchestrate the builds
/// without requiring plugin authors to immediately migrate to Bazel.
class PluginBuildGenerator {
  PluginBuildGenerator({required FileSystem fileSystem}) : _fileSystem = fileSystem;

  final FileSystem _fileSystem;

  /// Generates the transient BUILD.bazel files for all injected plugins.
  Future<void> generatePluginBuildFiles({required List<Plugin> plugins}) async {
    for (final plugin in plugins) {
      final Directory pluginAndroidDir = _fileSystem
          .directory(plugin.path)
          .childDirectory('android');

      // If the plugin does not have an Android directory, skip it.
      if (!pluginAndroidDir.existsSync()) {
        continue;
      }

      final File buildFile = pluginAndroidDir.childFile('BUILD.bazel');

      // Generate the build content utilizing a genrule to invoke Gradle
      // and an aar_import to expose the built artifact to Bazel.
      final String buildContent =
          r'''
load("@rules_android//android:rules.bzl", "aar_import")

# Transient genrule to orchestrate legacy Gradle plugins
genrule(
    name = "build_gradle_PLUGIN_NAME",
    srcs = glob(["**/*"]),
    outs = ["PLUGIN_NAME.aar"],
    cmd = """
      cd $$(dirname $(location build.gradle))
      ./gradlew assembleRelease
      cp build/outputs/aar/*-release.aar $(location PLUGIN_NAME.aar)
    """,
    visibility = ["//visibility:public"],
)

# AAR Import exposing the artifact generated above
aar_import(
    name = "plugin_PLUGIN_NAME_aar",
    aar = ":PLUGIN_NAME.aar",
    visibility = ["//visibility:public"],
)
'''
              .replaceAll('PLUGIN_NAME', plugin.name);

      buildFile.writeAsStringSync(buildContent);
    }
  }
}
