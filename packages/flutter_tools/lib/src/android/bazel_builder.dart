// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../base/logger.dart';
import '../build_info.dart';
import '../project.dart';
import 'android_builder.dart';

/// An implementation of [AndroidBuilder] that uses Bazel as the underlying
/// build system for Flutter artifacts, rather than Gradle.
///
/// This is a skeleton implementation as part of the Bazel migration plan.
class BazelBuilder implements AndroidBuilder {
  BazelBuilder({required Logger logger}) : _logger = logger;

  final Logger _logger;

  @override
  Future<void> buildAar({
    required FlutterProject project,
    required Set<AndroidBuildInfo> androidBuildInfo,
    required String target,
    required Future<void> Function(FlutterProject, {required bool releaseMode}) generateTooling,
    String? outputDirectoryPath,
    required String buildNumber,
  }) async {
    _logger.printStatus('Building AAR via Bazel is not yet fully implemented.');
    // TODO(boetger): Implement Bazel AAR build orchestration.
  }

  @override
  Future<void> buildApk({
    required FlutterProject project,
    required AndroidBuildInfo androidBuildInfo,
    required String target,
    bool configOnly = false,
  }) async {
    _logger.printStatus(
      'Building APK via Bazel + Gradle orchestration is not yet fully implemented.',
    );
    // TODO(boetger): 1. Invoke `bazel build //...` to produce libapp.so and flutter_assets.
    // 2. Invoke Gradle specifically for assembling the APK wrapper.
  }

  @override
  Future<void> buildAab({
    required FlutterProject project,
    required AndroidBuildInfo androidBuildInfo,
    required String target,
    bool validateDeferredComponents = true,
    bool deferredComponentsEnabled = false,
    bool configOnly = false,
  }) async {
    _logger.printStatus(
      'Building AAB via Bazel + Gradle orchestration is not yet fully implemented.',
    );
    // TODO(boetger): Implement Bazel AAB build orchestration.
  }

  @override
  Future<List<String>> getBuildVariants({required FlutterProject project}) async {
    return <String>['debug', 'profile', 'release'];
  }

  @override
  Future<String> outputsAppLinkSettings(
    String buildVariant, {
    required FlutterProject project,
  }) async {
    // Return empty for now as a stub.
    return '';
  }
}
